import 'dart:ffi';

import 'dart:math' as math;

import 'engine_event.dart';
import 'loopcore_bindings.g.dart';

enum _Kind { launch, stop, record }

final class _Pending {
  _Pending(this.kind, this.target, {this.slot = -1, this.bars = 0});
  final _Kind kind;
  final double target;
  final int slot;
  final int bars;
}

final class _Recording {
  _Recording(this.slot, this.start, this.length);
  final int slot;
  final double start;

  /// Nốt thu được (track instrument): {p, v, s, d}; d = null khi phím còn giữ.
  final notes = <Map<String, num?>>[];
  final open = <int, int>{}; // pitch → chỉ số trong [notes]

  /// 0 = thu tự do, chờ RECORD_STOP.
  double length;

  /// Vòng đầu của pedal mode (04 §2.5): thu khi transport CHƯA chạy, đo bằng giây.
  bool firstLoop = false;
  double seconds = 0;
}

/// Mô phỏng transport + ClipScheduler của engine (04 §3) cho [FakeEngineClient].
///
/// Đủ để UI thấy đúng các trạng thái: queued nhấp nháy rồi chuyển playing đúng ranh giới quantize,
/// thu theo số bar, scene, stop, overdub. Hành vi được khoá bằng bộ test hợp đồng dùng chung
/// (`test/contract/clip_scheduler_contract.dart`) để engine thật sau này chạy cùng bộ test.
///
/// Giản lược so với engine thật (ghi rõ để không nhầm):
/// - TRANSPORT_STOP bỏ bản đang thu dở (ô trở về Empty).
/// - Không có âm thanh; meter track chỉ là đường bao giả theo phách khi bật simulate.
final class FakeSessionSim {
  FakeSessionSim(this._emit);

  final void Function(EngineEvent) _emit;
  static const _eps = 1e-9;
  static const _tracks = LE_MAX_TRACKS;
  static const _scenes = LE_MAX_SCENES;

  bool playing = false;
  double beat = 0;
  double bpm = 120;
  int beatsPerBar = 4;
  int quantize = LeQuantize.LE_Q_1_BAR;
  int countInBars = 0;
  double sampleRate = 48000;

  final _len = List.generate(_tracks, (_) => List<double?>.filled(_scenes, null));
  final _kind = List.generate(_tracks, (_) => List<String>.filled(_scenes, 'midi'));
  final _clipId = List.generate(_tracks, (_) => List<String>.filled(_scenes, ''));
  final _notes = List.generate(_tracks, (_) => List<List<Map<String, num>>>.generate(_scenes, (_) => []));

  /// "audio" | "instrument" theo `track.configure` — quyết định take là audio hay MIDI.
  final trackKind = List<String>.filled(_tracks, 'audio');

  /// `midi.setRecordQuantize` (beat, 0 = tắt): áp dụng khi ghép nốt lúc thu xong.
  double recordQuantize = 0;
  final _gainDb = List.generate(_tracks, (_) => List<double>.filled(_scenes, 0));

  /// Ô có lớp overdub hoàn tác được (`clip.info.hasUndo`, `clip.undoOverdub`).
  final _undo = List.generate(_tracks, (_) => List<bool>.filled(_scenes, false));

  /// Nốt đang giữ trong lượt overdub MIDI: track → (pitch → chỉ số nốt trong clip).
  final _odOpen = List.generate(_tracks, (_) => <int, int>{});
  final _odNotesBefore = List<List<Map<String, num>>?>.filled(_tracks, null); // (dành cho undo MIDI nếu có sau này)
  final _warp = List.generate(_tracks, (_) => List<String>.filled(_scenes, 'stretch'));
  final _state = List.generate(_tracks, (_) => List<int>.filled(_scenes, LeClipState.LE_CLIP_EMPTY));
  final _playingSlot = List<int>.filled(_tracks, -1);
  final _launchBeat = List<double>.filled(_tracks, 0);
  final _pending = List<_Pending?>.filled(_tracks, null);
  final _rec = List<_Recording?>.filled(_tracks, null);
  final mute = List<bool>.filled(_tracks, false);
  final solo = List<bool>.filled(_tracks, false);
  final armed = List<bool>.filled(_tracks, false);
  int _recCounter = 0;

  // ───────── Pedal mode (04 §2.5) ─────────

  /// `transport.setTempoMode`: "fixed" | "firstLoop".
  String tempoMode = 'fixed';
  bool _hasTempo = true;

  /// Số beat của vòng đầu: các vòng sau (pedal mode) làm tròn lên bội số của nó.
  double? _firstLoopBeats;

  /// `engine.info.tempoState.hasTempo`: false khi pedal mode đang chờ vòng đầu.
  bool get hasTempo => tempoMode == 'fixed' || _hasTempo;

  /// `tempoState.firstLoopBeats` / kết quả `transport.setTempoMode`: null khi chưa có vòng đầu.
  double? get firstLoopBeats => tempoMode == 'firstLoop' && hasTempo ? _firstLoopBeats : null;
  bool get waitingFirstLoop => !hasTempo;

  bool get _anyClip => _len.any((row) => row.any((l) => l != null));

  /// Với firstLoop: đã có ≥ 1 clip thì coi là có tempo (BPM lấy từ model, 05 §3).
  /// [firstLoopBeats] (từ model khi mở lại project) giữ nguyên dù replay gửi op này TRƯỚC các clip.
  bool setTempoMode(String mode, {double? firstLoopBeats}) {
    tempoMode = mode;
    _hasTempo = mode == 'fixed' || _anyClip;
    _firstLoopBeats = mode == 'firstLoop' ? firstLoopBeats : null;
    return hasTempo;
  }

  /// Trở về "chưa có tempo" khi transport dừng VÀ mọi clip đã bị xoá (04 §2.5).
  void _maybeLoseTempo() {
    if (tempoMode == 'firstLoop' && _hasTempo && !playing && !_anyClip && !anyRecording) {
      _hasTempo = false;
      _firstLoopBeats = null;
      _emit(const TempoChanged(bpm: 0)); // 05 §2: về "chưa có tempo" → TEMPO_CHANGED(value = 0)
    }
  }

  double get quantizeBeats => switch (quantize) {
    LeQuantize.LE_Q_NONE => 0,
    LeQuantize.LE_Q_1_16 => 0.25,
    LeQuantize.LE_Q_1_8 => 0.5,
    LeQuantize.LE_Q_1_4 => 1,
    LeQuantize.LE_Q_1_2 => 2,
    LeQuantize.LE_Q_1_BAR => beatsPerBar.toDouble(),
    LeQuantize.LE_Q_2_BAR => 2.0 * beatsPerBar,
    LeQuantize.LE_Q_4_BAR => 4.0 * beatsPerBar,
    _ => beatsPerBar.toDouble(),
  };

  /// 04 §3.2: `ceil((now - ε) / q) * q`; đứng đúng ranh giới thì dùng luôn.
  double boundary(double now) {
    final q = quantizeBeats;
    return q == 0 ? now : ((now - _eps) / q).ceil() * q;
  }

  bool hasClip(int t, int s) => _len[t][s] != null;
  int clipState(int t, int s) => _state[t][s];
  bool get anyRecording => _rec.any((r) => r != null);

  // ───────── Lệnh RT ─────────

  /// Trả false nếu lệnh không hợp lệ (như `le_send`).
  bool handle(int type, int track, int slot, int i0, double d0, [double f0 = 0]) {
    bool validTrack() => track >= 0 && track < _tracks;
    bool validSlot() => slot >= 0 && slot < _scenes;
    switch (type) {
      case LeCommandType.LE_CMD_TRANSPORT_PLAY:
        playing = true;
      case LeCommandType.LE_CMD_TRANSPORT_STOP:
        _stopTransport();
      case LeCommandType.LE_CMD_SET_BPM:
        if (d0 < 20 || d0 > 300) return false;
        bpm = d0;
      case LeCommandType.LE_CMD_SET_QUANTIZE:
        if (i0 < 0 || i0 > LeQuantize.LE_Q_4_BAR) return false;
        quantize = i0;
      case LeCommandType.LE_CMD_SET_COUNT_IN:
        countInBars = i0.clamp(0, 2);
      case LeCommandType.LE_CMD_CLIP_LAUNCH:
        if (!validTrack() || !validSlot()) return false;
        final target = _startIfStopped();
        if (hasClip(track, slot)) {
          _queueLaunch(track, slot, target);
        } else {
          _queueStop(track, target); // ô trống = dừng track (hành vi Ableton)
        }
      case LeCommandType.LE_CMD_CLIP_STOP:
        if (!validTrack()) return false;
        _queueStop(track, boundary(beat));
      case LeCommandType.LE_CMD_SCENE_LAUNCH:
        if (!validSlot()) return false;
        final target = _startIfStopped();
        for (var t = 0; t < _tracks; t++) {
          if (hasClip(t, slot)) {
            _queueLaunch(t, slot, target);
          } else {
            _queueStop(t, target);
          }
        }
      case LeCommandType.LE_CMD_STOP_ALL:
        final target = boundary(beat);
        for (var t = 0; t < _tracks; t++) {
          _queueStop(t, target);
        }
      case LeCommandType.LE_CMD_CLIP_RECORD:
        if (!validTrack() || !validSlot() || hasClip(track, slot) || i0 < 0) return false;
        if (waitingFirstLoop) {
          // Vòng đầu: thu NGAY, không quantize / count-in / metronome, transport chưa chạy; luôn tự do.
          if (anyRecording) return false;
          _cancelPendingVisual(track);
          _rec[track] = _Recording(slot, 0, 0)..firstLoop = true;
          _state[track][slot] = LeClipState.LE_CLIP_RECORDING;
          return true;
        }
        final start = _startIfStopped() + countInBars * beatsPerBar;
        _cancelPendingVisual(track);
        _pending[track] = _Pending(_Kind.record, start, slot: slot, bars: i0);
        _state[track][slot] = LeClipState.LE_CLIP_QUEUED_RECORD;
        _applyDue(beat); // transport vừa khởi động ở beat 0 (không count-in) → thu ngay
      case LeCommandType.LE_CMD_RECORD_STOP:
        if (!validTrack()) return false;
        if (_pending[track]?.kind == _Kind.record) {
          _cancelPendingVisual(track); // đang QueuedRecord → huỷ, ô về Empty (04 §3.4)
          return true;
        }
        final r = _rec[track];
        if (r != null && r.firstLoop) {
          _finishFirstLoop(track, r);
        } else if (r != null && r.length == 0) {
          final fl = _firstLoopBeats;
          if (tempoMode == 'firstLoop' && fl != null) {
            // Pedal mode, các vòng sau: làm tròn LÊN tới bội số của vòng đầu.
            r.length = math.max(1, ((beat - r.start - _eps) / fl).ceil()) * fl;
          } else {
            // Làm tròn LÊN theo quantize, tối thiểu 1 bar.
            final end = boundary(beat);
            r.length = end - r.start < beatsPerBar ? beatsPerBar.toDouble() : end - r.start;
          }
        }
      case LeCommandType.LE_CMD_OVERDUB_TOGGLE:
        if (!validTrack()) return false;
        final p = _playingSlot[track];
        if (p < 0) return false;
        if (_state[track][p] != LeClipState.LE_CLIP_OVERDUBBING &&
            _kind[track][p] == 'audio' &&
            _warp[track][p] == 'repitch') {
          // Như engine: clip audio Re-Pitch (khác tempo) không vào được overdub → LE_EVT_ERROR, ô vẫn Playing.
          _emit(EngineErrorEvent(errorCode: LeError.LE_ERR_OVERDUB_UNSUPPORTED, track: track, slot: p));
          return true;
        }
        if (_state[track][p] == LeClipState.LE_CLIP_OVERDUBBING) {
          _state[track][p] = LeClipState.LE_CLIP_PLAYING;
          _endOverdub(track, p);
        } else {
          _state[track][p] = LeClipState.LE_CLIP_OVERDUBBING;
          _odNotesBefore[track] = [for (final n in _notes[track][p]) Map.of(n)];
        }
      case LeCommandType.LE_CMD_TRACK_MUTE:
        if (!validTrack()) return false;
        mute[track] = i0 != 0;
      case LeCommandType.LE_CMD_TRACK_SOLO:
        if (!validTrack()) return false;
        solo[track] = i0 != 0;
      case LeCommandType.LE_CMD_NOTE_ON:
        if (!validTrack() || i0 < 0 || i0 > 127) return false;
        final r = _rec[track];
        if (r != null && beat >= r.start - _eps) {
          r.open[i0] = r.notes.length;
          r.notes.add({'p': i0, 'v': (f0 * 127).round().clamp(1, 127), 's': beat - r.start, 'd': null});
        }
        final od = _playingSlot[track];
        if (od >= 0 && _state[track][od] == LeClipState.LE_CLIP_OVERDUBBING && _kind[track][od] == 'midi') {
          // Overdub MIDI: nốt chồng vào clip ở vị trí trong vòng lặp.
          final len = _len[track][od]!;
          _odOpen[track][i0] = _notes[track][od].length;
          _notes[track][od].add({
            'p': i0,
            'v': (f0 * 127).round().clamp(1, 127),
            's': (beat - _launchBeat[track]) % len,
            'd': 0.25,
          });
        }
      case LeCommandType.LE_CMD_NOTE_OFF:
        if (!validTrack()) return false;
        final r = _rec[track];
        final idx = r?.open.remove(i0);
        if (r != null && idx != null) {
          final s = r.notes[idx]['s']!;
          r.notes[idx]['d'] = (beat - r.start - s).clamp(1 / 64, double.infinity);
        }
        final odIdx = _odOpen[track].remove(i0);
        final od = _playingSlot[track];
        if (odIdx != null && od >= 0) {
          final n = _notes[track][od][odIdx];
          final len = _len[track][od]!;
          n['d'] = (((beat - _launchBeat[track]) % len) - n['s']! + len) % len;
          if (n['d']! < 1 / 64) n['d'] = 1 / 64;
        }
      case LeCommandType.LE_CMD_ALL_NOTES_OFF:
        for (var t = 0; t < _tracks; t++) {
          if (track >= 0 && t != track) continue;
          final r = _rec[t];
          if (r == null) continue;
          for (final e in r.open.entries.toList()) {
            handle(LeCommandType.LE_CMD_NOTE_OFF, t, -1, e.key, 0);
          }
        }
      case LeCommandType.LE_CMD_TRACK_ARM:
        if (!validTrack()) return false;
        armed[track] = i0 != 0;
      case LeCommandType.LE_CMD_LOOP_BUTTON:
        if (!validTrack() || slot < -1 || slot >= _scenes) return false;
        final s = slot >= 0 ? slot : _loopTarget(track);
        if (s == null) return false;
        return _loopPress(track, s);
    }
    return true;
  }

  /// `slot = −1`: ô đang Recording / Overdubbing / Playing của track, không có thì ô trống đầu tiên (05 §3).
  int? _loopTarget(int t) {
    final r = _rec[t];
    if (r != null) return r.slot;
    final p = _pending[t];
    if (p != null && p.kind == _Kind.record) return p.slot;
    if (_playingSlot[t] >= 0) return _playingSlot[t];
    for (var s = 0; s < _scenes; s++) {
      if (!hasClip(t, s) && _state[t][s] == LeClipState.LE_CLIP_EMPTY) return s;
    }
    return null;
  }

  /// Một lần chạm nút LOOP / footswitch: Empty → thu tự do · Recording → chốt · Playing → overdub ·
  /// Overdubbing → phát · Stopped / Queued → launch.
  bool _loopPress(int t, int s) => switch (_state[t][s]) {
    LeClipState.LE_CLIP_EMPTY => handle(LeCommandType.LE_CMD_CLIP_RECORD, t, s, 0, 0),
    LeClipState.LE_CLIP_RECORDING ||
    LeClipState.LE_CLIP_QUEUED_RECORD => handle(LeCommandType.LE_CMD_RECORD_STOP, t, -1, 0, 0),
    LeClipState.LE_CLIP_PLAYING ||
    LeClipState.LE_CLIP_OVERDUBBING => handle(LeCommandType.LE_CMD_OVERDUB_TOGGLE, t, -1, 0, 0),
    _ => handle(LeCommandType.LE_CMD_CLIP_LAUNCH, t, s, 0, 0),
  };

  /// Vòng đầu pedal mode: T giây → nb = beatsPerBar·2^k sao cho bpm ∈ [80, 160) (T quá ngắn: nb = beatsPerBar,
  /// bpm tối đa 300). Transport khởi động sao cho beat 0 = sample đầu của take; clip phát tiếp liền mạch.
  void _finishFirstLoop(int t, _Recording r) {
    final secs = r.seconds;
    if (secs <= 0) {
      _rec[t] = null;
      _state[t][r.slot] = LeClipState.LE_CLIP_EMPTY;
      return;
    }
    var nb = beatsPerBar.toDouble();
    var b = 60 * nb / secs;
    if (b >= 160) {
      b = math.min(b, 300);
    } else {
      while (b < 80) {
        nb *= 2;
        b = 60 * nb / secs;
      }
    }
    bpm = b;
    _firstLoopBeats = nb;
    _hasTempo = true;
    r.length = nb;
    playing = true;
    beat = nb; // đã qua đúng nb beat kể từ sample đầu của take
    _emit(TempoChanged(bpm: b));
    _finishRecording(t, r);
  }

  /// Thời gian thực trôi [seconds]: đo vòng đầu (transport chưa chạy) + transport nếu đang chạy.
  void advanceSeconds(double seconds) {
    for (final r in _rec) {
      if (r != null && r.firstLoop) r.seconds += seconds;
    }
    advanceBeats(seconds * bpm / 60);
  }

  double _startIfStopped() {
    if (playing) return boundary(beat);
    playing = true;
    beat = 0; // 04 §3.2: transport dừng mà nhận launch → Play từ beat 0, áp dụng ngay
    return 0;
  }

  void _cancelPendingVisual(int t) {
    final p = _pending[t];
    if (p == null) return;
    if (p.kind == _Kind.launch && p.slot != _playingSlot[t]) {
      _state[t][p.slot] = LeClipState.LE_CLIP_STOPPED;
    } else if (p.kind == _Kind.record) {
      _state[t][p.slot] = LeClipState.LE_CLIP_EMPTY;
    }
    final cur = _playingSlot[t];
    if (cur >= 0 && _state[t][cur] == LeClipState.LE_CLIP_QUEUED_STOP) {
      _state[t][cur] = LeClipState.LE_CLIP_PLAYING;
    }
    _pending[t] = null;
  }

  void _queueLaunch(int t, int s, double target) {
    _cancelPendingVisual(t);
    _pending[t] = _Pending(_Kind.launch, target, slot: s);
    _state[t][s] = LeClipState.LE_CLIP_QUEUED_PLAY;
    final cur = _playingSlot[t];
    if (cur >= 0 && cur != s) _state[t][cur] = LeClipState.LE_CLIP_QUEUED_STOP;
    _applyDue(beat);
  }

  void _queueStop(int t, double target) {
    _cancelPendingVisual(t);
    final r = _rec[t];
    if (r != null) {
      if (r.length == 0) handle(LeCommandType.LE_CMD_RECORD_STOP, t, -1, 0, 0);
      return;
    }
    final cur = _playingSlot[t];
    if (cur < 0) return; // không phát gì: huỷ launch đang chờ (nếu có) là đủ
    _pending[t] = _Pending(_Kind.stop, target);
    _state[t][cur] = LeClipState.LE_CLIP_QUEUED_STOP;
    _applyDue(beat);
  }

  void _stopTransport() {
    playing = false;
    beat = 0;
    _stopTransportClips();
    _maybeLoseTempo();
  }

  void _stopTransportClips() {
    for (var t = 0; t < _tracks; t++) {
      _pending[t] = null;
      final r = _rec[t];
      if (r != null) {
        _state[t][r.slot] = LeClipState.LE_CLIP_EMPTY; // bỏ bản thu dở
        _rec[t] = null;
      }
      _playingSlot[t] = -1;
      for (var s = 0; s < _scenes; s++) {
        if (_state[t][s] == LeClipState.LE_CLIP_OVERDUBBING) _endOverdub(t, s); // stop kết thúc overdub
        _state[t][s] = hasClip(t, s) ? LeClipState.LE_CLIP_STOPPED : LeClipState.LE_CLIP_EMPTY;
      }
    }
  }

  // ───────── Thời gian ─────────

  /// Cho transport chạy thêm [beats] (chỉ khi đang play). Áp dụng lệnh theo đúng thứ tự thời gian.
  void advanceBeats(double beats) {
    if (!playing || beats <= 0) return;
    final end = beat + beats;
    while (true) {
      final next = _nextEventBeat();
      if (next == null || next > end + _eps) break;
      beat = next > beat ? next : beat;
      _applyDue(beat);
    }
    beat = end;
  }

  double? _nextEventBeat() {
    double? best;
    for (var t = 0; t < _tracks; t++) {
      final p = _pending[t];
      if (p != null && (best == null || p.target < best)) best = p.target;
      final r = _rec[t];
      if (r != null && r.length > 0) {
        final e = r.start + r.length;
        if (best == null || e < best) best = e;
      }
    }
    return best;
  }

  void _applyDue(double now) {
    for (var t = 0; t < _tracks; t++) {
      final r = _rec[t];
      if (r != null && r.length > 0 && r.start + r.length <= now + _eps) _finishRecording(t, r);
      final p = _pending[t];
      if (p == null || p.target > now + _eps) continue;
      _pending[t] = null;
      final cur = _playingSlot[t];
      switch (p.kind) {
        case _Kind.launch:
          if (cur >= 0 && cur != p.slot) _state[t][cur] = LeClipState.LE_CLIP_STOPPED;
          _state[t][p.slot] = LeClipState.LE_CLIP_PLAYING;
          _playingSlot[t] = p.slot;
          _launchBeat[t] = p.target;
        case _Kind.stop:
          if (cur >= 0) _state[t][cur] = LeClipState.LE_CLIP_STOPPED;
          _playingSlot[t] = -1;
        case _Kind.record:
          if (cur >= 0) _state[t][cur] = LeClipState.LE_CLIP_STOPPED;
          _playingSlot[t] = -1;
          _state[t][p.slot] = LeClipState.LE_CLIP_RECORDING;
          _rec[t] = _Recording(p.slot, p.target, p.bars * beatsPerBar.toDouble());
      }
    }
  }

  void _finishRecording(int t, _Recording r) {
    _rec[t] = null;
    _len[t][r.slot] = r.length;
    final midi = trackKind[t] == 'instrument';
    _kind[t][r.slot] = midi ? 'midi' : 'audio';
    _notes[t][r.slot] = [
      if (midi)
        for (final n in r.notes)
          if (n['s']! < r.length) {'p': n['p']!, 'v': n['v']!, 's': n['s']!, 'd': n['d'] ?? (r.length - n['s']!)},
    ];
    if (midi && recordQuantize > 0) quantizeNotes(t, r.slot, recordQuantize);
    _gainDb[t][r.slot] = 0;
    _warp[t][r.slot] = 'stretch';
    _clipId[t][r.slot] = 'c_fake${(++_recCounter).toString().padLeft(4, '0')}';
    _state[t][r.slot] = LeClipState.LE_CLIP_PLAYING; // thu xong thì phát loop luôn (04 §3.1)
    _playingSlot[t] = r.slot;
    _launchBeat[t] = r.start;
    final frames = (r.length * 60 / bpm * sampleRate).round();
    _emit(RecordingFinished(track: t, slot: r.slot, frames: frames));
  }

  // ───────── Lệnh cấu trúc ─────────

  /// `project.open/close`: reset toàn bộ state RT của project về mặc định (05 §3). Thiết lập toàn cục
  /// ([recordQuantize]) giữ nguyên.
  void reset() {
    _stopTransport();
    tempoMode = 'fixed';
    _hasTempo = true;
    _firstLoopBeats = null;
    bpm = 120;
    beatsPerBar = 4;
    quantize = LeQuantize.LE_Q_1_BAR;
    countInBars = 0;
    for (var t = 0; t < _tracks; t++) {
      for (var s = 0; s < _scenes; s++) {
        _len[t][s] = null;
        _notes[t][s] = [];
        _state[t][s] = LeClipState.LE_CLIP_EMPTY;
        _undo[t][s] = false;
      }
      mute[t] = solo[t] = armed[t] = false;
      trackKind[t] = 'audio';
    }
  }

  void setClip(
    int t,
    int s, {
    required String clipId,
    required String kind,
    required double lengthBeats,
    List<Map<String, num>> notes = const [],
    double gainDb = 0,
    String warp = 'stretch',
  }) {
    _gainDb[t][s] = gainDb;
    _warp[t][s] = warp;
    _notes[t][s] = [for (final n in notes) Map.of(n)];
    _undo[t][s] = false;
    _len[t][s] = lengthBeats > 0 ? lengthBeats : beatsPerBar.toDouble();
    if (tempoMode == 'firstLoop') _hasTempo = true; // model đã có clip → có tempo
    _kind[t][s] = kind;
    _clipId[t][s] = clipId;
    if (_state[t][s] == LeClipState.LE_CLIP_EMPTY) _state[t][s] = LeClipState.LE_CLIP_STOPPED;
  }

  void clearClip(int t, int s) {
    if (_playingSlot[t] == s) _playingSlot[t] = -1;
    if (_pending[t]?.slot == s) _pending[t] = null;
    _len[t][s] = null;
    _state[t][s] = LeClipState.LE_CLIP_EMPTY;
    _undo[t][s] = false;
    _maybeLoseTempo();
  }

  /// Nốt của clip MIDI (05 §3 `clip.getMidi`).
  List<Map<String, num>> midiNotes(int t, int s) => [for (final n in _notes[t][s]) Map.of(n)];

  /// `midiClip.quantize`: đưa điểm bắt đầu nốt về lưới gần nhất (beat), giữ độ dài.
  void quantizeNotes(int t, int s, double grid) {
    final len = _len[t][s] ?? double.infinity;
    for (final n in _notes[t][s]) {
      final q = (n['s']! / grid).round() * grid;
      n['s'] = q >= len - _eps ? 0.0 : q; // tròn tới cuối vòng → về 0 (như engine)
    }
  }

  String kindOf(int t, int s) => _len[t][s] == null ? 'empty' : _kind[t][s];
  double? lengthOf(int t, int s) => _len[t][s];

  /// Tìm clip theo id (Fake dùng để sinh peaks).
  ({int track, int slot, double lengthBeats, String kind})? findClip(String clipId) {
    for (var t = 0; t < _tracks; t++) {
      for (var s = 0; s < _scenes; s++) {
        final len = _len[t][s];
        if (len != null && _clipId[t][s] == clipId) return (track: t, slot: s, lengthBeats: len, kind: _kind[t][s]);
      }
    }
    return null;
  }

  /// `clip.setParams`. Trả false nếu ô không có clip audio.
  bool setParams(int t, int s, {double? gainDb, String? warp}) {
    if (_len[t][s] == null) return false;
    if (warp != null && _kind[t][s] != 'audio') return false; // warp chỉ có nghĩa với audio
    if (gainDb != null) _gainDb[t][s] = gainDb;
    if (warp != null) _warp[t][s] = warp;
    return true;
  }

  Map<String, dynamic>? clipInfo(int t, int s) {
    final len = _len[t][s];
    if (len == null) return null;
    final audio = _kind[t][s] == 'audio';
    return {
      'clipId': _clipId[t][s],
      'kind': _kind[t][s],
      if (audio) 'file': 'audio/${_clipId[t][s]}.caf',
      'lengthBeats': len,
      if (audio) 'originalBpm': bpm,
      if (audio) 'warp': _warp[t][s],
      // 0 = đang Re-Pitch; > 0 = đã có bản giữ cao độ ở BPM đó (Fake: stretch thì coi như render xong ngay).
      if (audio) 'stretchedBpm': _warp[t][s] == 'stretch' ? bpm : 0,
      'gainDb': _gainDb[t][s],
      'hasUndo': _undo[t][s],
    };
  }

  /// Hết một lượt overdub (toggle hoặc TRANSPORT_STOP): có lớp undo + phát RECORDING_FINISHED (04 §5.4) để
  /// app đọc lại clip (MIDI: clip.getMidi; audio: clip.info + peaks).
  void _endOverdub(int t, int s) {
    _odOpen[t].clear();
    _odNotesBefore[t] = null;
    if (_kind[t][s] == 'audio') _undo[t][s] = true; // như engine: chỉ clip audio có lớp undo
    _emit(RecordingFinished(track: t, slot: s, frames: ((_len[t][s] ?? 0) * 60 / bpm * sampleRate).round()));
  }

  /// `clip.undoOverdub`: bỏ lớp overdub audio gần nhất. false nếu ô không có lớp undo.
  bool undoOverdub(int t, int s) {
    if (!_undo[t][s]) return false;
    _undo[t][s] = false;
    return true;
  }

  // ───────── Publish vào LeState ─────────

  void publish(LeState s, {required bool meters}) {
    s
      ..playing = playing ? 1 : 0
      ..anyRecording = anyRecording ? 1 : 0
      ..beat = beat
      ..bpm = bpm
      ..beatsPerBar = beatsPerBar
      ..quantize = quantize;
    final anySolo = solo.any((x) => x);
    for (var t = 0; t < _tracks; t++) {
      final row = s.clipState[t];
      for (var c = 0; c < _scenes; c++) {
        row[c] = _state[t][c];
      }
      final r = _rec[t];
      final p = _playingSlot[t];
      if (r != null) {
        s.trackPlayingSlot[t] = r.slot;
        final bar = beatsPerBar.toDouble();
        s.trackClipProgress[t] = r.length > 0
            ? ((beat - r.start) / r.length).clamp(0.0, 1.0)
            : (((beat - r.start) % bar) / bar).clamp(0.0, 1.0);
      } else if (p >= 0) {
        s.trackPlayingSlot[t] = p;
        final len = _len[t][p]!;
        final rel = beat - _launchBeat[t];
        s.trackClipProgress[t] = rel <= 0 ? 0 : (rel % len) / len;
      } else {
        s.trackPlayingSlot[t] = -1;
        s.trackClipProgress[t] = 0;
      }
      if (meters) {
        final audible = (p >= 0 || r != null) && !mute[t] && (!anySolo || solo[t]);
        final ph = beat % 1.0;
        final env = audible ? 0.12 + 0.65 * (1 - ph) * (1 - ph) * (0.8 + 0.2 * ((t % 3) / 2)) : 0.0;
        s.trackPeak[t][0] = env;
        s.trackPeak[t][1] = env * 0.94;
      }
    }
  }
}
