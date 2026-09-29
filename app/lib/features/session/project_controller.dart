import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../engine/app_error.dart';
import '../../engine/engine_providers.dart';
import '../../engine/job_tracker.dart';
import '../../engine/project_replay.dart';
import '../../model/ids.dart';
import '../../model/names.dart';
import '../../model/project.dart';
import '../fx/fx_specs.dart';
import '../settings/app_settings.dart';
import '../../l10n/l10n.dart';

/// Vị trí ô (track, slot) cho thao tác kéo/copy.
typedef CellPos = ({int track, int slot});

/// Tiến độ mở project: "Đang mở project… 7/12" (06 §6).
@immutable
final class OpenProgress {
  const OpenProgress(this.done, this.total);
  final int done;
  final int total;

  @override
  bool operator ==(Object other) => other is OpenProgress && other.done == done && other.total == total;
  @override
  int get hashCode => Object.hash(done, total);
  @override
  String toString() => 'OpenProgress($done/$total)';
}

@immutable
final class ProjectSession {
  const ProjectSession({
    required this.project,
    required this.dir,
    this.readOnly = false,
    this.progress,
    this.errors = const [],
    this.armed = const {},
    this.missingClipIds = const {},
    this.clipRevisions = const {},
    this.hasTempo = true,
  });

  final Project project;

  /// Thư mục `.loopproj` (engine ghi audio thu âm vào đây).
  final String dir;
  final bool readOnly;

  /// Khác null khi đang đợi job lúc mở project.
  final OpenProgress? progress;

  /// Lỗi (theo mã) khi dựng engine / thao tác — hiện dạng banner, không chặn app (07 §4.3, 07 §8).
  final List<AppError> errors;

  /// Track đang arm (trạng thái phiên chơi, không lưu vào file).
  final Set<int> armed;

  /// Clip audio thiếu file (06 §5.4) → ô vẽ xám + ⚠.
  final Set<String> missingClipIds;

  /// Số lần nội dung audio của clip đổi trong phiên (sau mỗi lượt overdub) → waveform lấy lại peaks.
  /// Không lưu vào file.
  final Map<String, int> clipRevisions;

  int revisionOf(String clipId) => clipRevisions[clipId] ?? 0;

  /// `engine.info.tempoState.hasTempo` (05 §3): false khi pedal mode đang chờ vòng đầu. Không lưu vào file.
  final bool hasTempo;

  /// Pedal mode chờ vòng đầu: top bar hiện "— BPM · chờ vòng đầu", metronome tắt, thu luôn tự do (07 §3.1b).
  bool get waitingFirstLoop => project.transport.tempoMode == TempoMode.firstLoop && !hasTempo;

  bool get isLoading => progress != null;

  ProjectSession copyWith({
    Project? project,
    OpenProgress? progress,
    bool clearProgress = false,
    List<AppError>? errors,
    Set<int>? armed,
    Map<String, int>? clipRevisions,
    bool? hasTempo,
  }) => ProjectSession(
    project: project ?? this.project,
    dir: dir,
    readOnly: readOnly,
    progress: clearProgress ? null : (progress ?? this.progress),
    errors: errors ?? this.errors,
    armed: armed ?? this.armed,
    missingClipIds: missingClipIds,
    clipRevisions: clipRevisions ?? this.clipRevisions,
    hasTempo: hasTempo ?? this.hasTempo,
  );
}

/// Mọi thay đổi project đi qua đây (07 §5): gửi lệnh engine TRƯỚC, rồi cập nhật model bất biến.
/// Autosave (06 §5.2) nghe state của controller này (xem `autosave.dart`).
class ProjectController extends Notifier<ProjectSession?> {
  int _generation = 0;
  StreamSubscription<EngineEvent>? _events;

  EngineApi get _engine => ref.read(engineProvider);
  JobTracker get _jobs => ref.read(jobTrackerProvider);

  @override
  ProjectSession? build() {
    _events = ref.read(engineProvider).events.listen(_onEngineEvent);
    ref.onDispose(() => _events?.cancel());
    return null;
  }

  /// Đọc lại `engine.info.tempoState` (05 §3) sau TEMPO_CHANGED / RECORDING_FINISHED / project.open / xoá clip,
  /// và khi transport vừa dừng (màn Session gọi — pedal mode: dừng + hết clip → chờ vòng đầu, 04 §2.5).
  /// Engine kiểm "về chưa có tempo" trong pump 30 Hz (chậm ≤ ~40 ms sau clip.clear / TRANSPORT_STOP) và phát
  /// TEMPO_CHANGED(value = 0) đúng lúc đó (05 §2) → [_onEngineEvent] đọc lại; lần đọc ngay ở đây chỉ để khớp sớm.
  void refreshTempo() => _refreshTempo();

  /// [fallbackFirstLoop]: độ dài clip vừa thu — dùng làm `firstLoopBeats` nếu engine chưa trả trường này.
  void _refreshTempo({double? fallbackFirstLoop}) {
    final s = state;
    if (s == null) return;
    final r = _engine.call({'op': 'engine.info'});
    final result = r['ok'] == true ? r['result'] : null;
    final ts = result is Map ? result['tempoState'] : null;
    final has = ts is Map ? ts['hasTempo'] != false : true;
    final t = s.project.transport;
    var flb = t.firstLoopBeats;
    if (t.tempoMode == TempoMode.firstLoop) {
      // Có tempo → lấy độ dài vòng đầu từ engine (hoặc clip vòng đầu); về "chưa có tempo" → xoá.
      // Engine báo "chưa có vòng đầu" bằng 0 (hoặc thiếu trường) → coi như null.
      final raw = ts is Map ? (ts['firstLoopBeats'] as num?)?.toDouble() : null;
      final fromEngine = raw != null && raw > 0 ? raw : null;
      flb = has ? (fromEngine ?? flb ?? fallbackFirstLoop) : null;
    }
    if (has == s.hasTempo && flb == t.firstLoopBeats) return;
    state = s.copyWith(
      hasTempo: has,
      project: s.project.copyWith(transport: t.copyWith(firstLoopBeats: flb)),
    );
  }

  /// Phát lại chuỗi lệnh 06 §6 rồi đợi mọi job. Mở project khác giữa chừng → kết quả cũ bị bỏ.
  Future<void> open(
    Project project, {
    required String dir,
    bool readOnly = false,
    Set<String> missingClipIds = const {},
  }) async {
    final gen = ++_generation;
    final jobs = _jobs; // tạo tracker trước khi job kịp phát event
    state = ProjectSession(
      project: project,
      dir: dir,
      readOnly: readOnly,
      progress: const OpenProgress(0, 0),
      missingClipIds: missingClipIds,
    );

    final replay = ProjectReplay(_engine).run(project, dir: dir);
    final errors = [...replay.errors];
    final total = replay.jobIds.length;
    var done = 0;
    state = state!.copyWith(progress: OpenProgress(0, total), errors: List.unmodifiable(errors));

    await Future.wait(
      replay.jobIds.map(
        (id) => jobs
            .awaitJob(id)
            .then<void>(
              (_) {},
              onError: (Object e) {
                errors.add(
                  e is EngineJobException ? AppError(e.code, op: 'job ${e.jobId}') : AppError('INTERNAL', op: '$e'),
                );
              },
            )
            .whenComplete(() {
              done++;
              if (gen == _generation) state = state!.copyWith(progress: OpenProgress(done, total));
            }),
      ),
    );
    if (gen != _generation) return;
    state = state!.copyWith(clearProgress: true, errors: List.unmodifiable(errors));
    _refreshTempo();
  }

  /// Đóng banner lỗi mở project.
  void clearErrors() {
    final s = state;
    if (s != null && s.errors.isNotEmpty) state = s.copyWith(errors: const []);
  }

  void close() {
    if (state == null) return;
    _generation++;
    _engine.call({'op': 'project.close'});
    state = null;
  }

  // ───────── Transport ─────────

  void setBpm(double bpm) {
    final s = _editable();
    if (s == null) return;
    final v = bpm.clamp(20.0, 300.0);
    _engine.send(LeCommandType.LE_CMD_SET_BPM, d0: v);
    state = s.copyWith(
      project: s.project.copyWith(transport: s.project.transport.copyWith(bpm: v)),
    );
  }

  void setQuantize(QuantizeGrid q) {
    final s = _editable();
    if (s == null) return;
    _engine.send(LeCommandType.LE_CMD_SET_QUANTIZE, i0: q.leValue);
    state = s.copyWith(
      project: s.project.copyWith(transport: s.project.transport.copyWith(quantize: q)),
    );
  }

  void setMetronome(MetronomeMode mode, {double? volume}) {
    final s = _editable();
    if (s == null) return;
    final m = s.project.transport.metronome.copyWith(
      mode: mode,
      volume: (volume ?? s.project.transport.metronome.volume).clamp(0.0, 1.0),
    );
    _engine.send(LeCommandType.LE_CMD_METRONOME, i0: m.mode.leValue, f0: m.volume);
    state = s.copyWith(
      project: s.project.copyWith(transport: s.project.transport.copyWith(metronome: m)),
    );
  }

  /// Âm lượng metronome lúc kéo slider (≤ 1 lần/frame): chỉ gửi lệnh; thả tay → [setMetronome] với volume.
  void previewMetronomeVolume(double volume) {
    final s = _editable();
    if (s == null) return;
    _engine.send(
      LeCommandType.LE_CMD_METRONOME,
      i0: s.project.transport.metronome.mode.leValue,
      f0: volume.clamp(0.0, 1.0),
    );
  }

  /// Chế độ tempo (07 §3.1b, 05 §3 `transport.setTempoMode`): BPM cố định hoặc vòng đầu quyết định BPM.
  void setTempoMode(TempoMode mode) {
    final s = _editable();
    if (s == null || s.project.transport.tempoMode == mode) return;
    final Map<String, dynamic> r;
    try {
      r = _engine.callOk('transport.setTempoMode', {
        'mode': mode.name,
        if (s.project.transport.firstLoopBeats != null) 'firstLoopBeats': s.project.transport.firstLoopBeats,
      });
    } on EngineCallException catch (e) {
      state = s.copyWith(errors: List.unmodifiable([...s.errors, AppError(e.code, op: 'transport.setTempoMode')]));
      return;
    }
    final rawFlb = (r['firstLoopBeats'] as num?)?.toDouble();
    final flb = rawFlb != null && rawFlb > 0 ? rawFlb : null;
    state = s.copyWith(
      project: s.project.copyWith(
        transport: s.project.transport.copyWith(
          tempoMode: mode,
          firstLoopBeats: r.containsKey('firstLoopBeats') ? flb : s.project.transport.firstLoopBeats,
        ),
      ),
      hasTempo: r['hasTempo'] != false,
    );
  }

  /// Nhịp (06 §2 `transport.timeSignature`): 4/4, 3/4, 6/8 (07 §2 menu ♩).
  void setTimeSignature(int num, int den) {
    final s = _editable();
    if (s == null) return;
    if (!_callOk('transport.setTimeSignature', {'num': num, 'den': den})) return;
    state = state!.copyWith(
      project: state!.project.copyWith(transport: state!.project.transport.copyWith(timeSignature: [num, den])),
    );
  }

  void setCountIn(int bars) {
    final s = _editable();
    if (s == null) return;
    final v = bars.clamp(0, 2);
    _engine.send(LeCommandType.LE_CMD_SET_COUNT_IN, i0: v);
    state = s.copyWith(
      project: s.project.copyWith(transport: s.project.transport.copyWith(countInBars: v)),
    );
  }

  void setMasterGain(double gainDb) {
    final s = _editable();
    if (s == null) return;
    final v = gainDb.clamp(-120.0, 6.0);
    _engine.send(LeCommandType.LE_CMD_MASTER_GAIN, f0: v);
    state = s.copyWith(
      project: s.project.copyWith(master: s.project.master.copyWith(gainDb: v)),
    );
  }

  // ───────── Track ─────────

  /// Arm/bỏ arm. Arm không lưu vào file; engine cấp phát buffer thu trước khi xếp lệnh (05 §2).
  void setArm(int track, bool armed) {
    final s = _editable();
    if (s == null) return;
    _ensureTrack(track);
    _engine.send(LeCommandType.LE_CMD_TRACK_ARM, track: track, i0: armed ? 1 : 0);
    final next = {...state!.armed};
    armed ? next.add(track) : next.remove(track);
    state = state!.copyWith(armed: Set.unmodifiable(next));
  }

  /// Arm để thu. Track audio mà chưa có quyền mic → hỏi lần đầu (07 §4.0); từ chối → không arm, trả false.
  Future<bool> armForRecording(int track) async {
    if (_editable() == null) return false;
    final kind = state!.project.trackAt(track)?.kind ?? TrackKind.audio;
    final audio = ref.read(engineAudioProvider);
    if (kind == TrackKind.audio && !audio.canRecordNow && !await audio.ensureMic()) return false;
    if (!(state?.armed.contains(track) ?? true)) setArm(track, true);
    return state != null;
  }

  void setMute(int track, bool mute) {
    if (_editable() == null) return;
    _ensureTrack(track);
    _engine.send(LeCommandType.LE_CMD_TRACK_MUTE, track: track, i0: mute ? 1 : 0);
    _updateMixer(track, (m) => m.copyWith(mute: mute));
  }

  void setSolo(int track, bool solo) {
    if (_editable() == null) return;
    _ensureTrack(track);
    _engine.send(LeCommandType.LE_CMD_TRACK_SOLO, track: track, i0: solo ? 1 : 0);
    _updateMixer(track, (m) => m.copyWith(solo: solo));
  }

  /// Lúc kéo fader/knob (≤ 1 lần/frame): chỉ gửi lệnh, KHÔNG ghi model → strip không rebuild mỗi frame
  /// (test/perf/rebuild_budget_test.dart). Thả tay mới gọi [setGain]/[setPan]/[setMasterGain].
  void previewGain(int track, double gainDb) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_TRACK_GAIN, track: track, f0: gainDb.clamp(-120.0, 6.0));
  }

  void previewPan(int track, double pan) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_TRACK_PAN, track: track, f0: pan.clamp(-1.0, 1.0));
  }

  void previewMasterGain(double gainDb) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_MASTER_GAIN, f0: gainDb.clamp(-120.0, 6.0));
  }

  /// Gain track: gửi lệnh rồi ghi model (thả fader, chạm đúp, MIDI learn…).
  void setGain(int track, double gainDb) {
    if (_editable() == null) return;
    _ensureTrack(track);
    final v = gainDb.clamp(-120.0, 6.0);
    _engine.send(LeCommandType.LE_CMD_TRACK_GAIN, track: track, f0: v);
    _updateMixer(track, (m) => m.copyWith(gainDb: v));
  }

  void setPan(int track, double pan) {
    if (_editable() == null) return;
    _ensureTrack(track);
    final v = pan.clamp(-1.0, 1.0);
    _engine.send(LeCommandType.LE_CMD_TRACK_PAN, track: track, f0: v);
    _updateMixer(track, (m) => m.copyWith(pan: v));
  }

  /// Monitor input của track (06 §2 `tracks[].monitor`, `TRACK_MONITOR`).
  void setMonitor(int track, MonitorMode mode) {
    if (_editable() == null) return;
    _ensureTrack(track);
    _engine.send(LeCommandType.LE_CMD_TRACK_MONITOR, track: track, i0: mode.leValue);
    _updateTrack(track, (t) => t.copyWith(monitor: mode));
  }

  /// Tên track (`track.configure` kèm kind hiện tại).
  void renameTrack(int track, String name) {
    final clean = cleanName(name); // 05 §1: không gửi NUL / ký tự điều khiển xuống engine
    if (_editable() == null || clean.isEmpty) return;
    _ensureTrack(track);
    final t = state!.project.trackAt(track)!;
    if (!_callOk('track.configure', trackConfigure(track, t.copyWith(name: clean)))) return;
    _updateTrack(track, (x) => x.copyWith(name: clean));
  }

  /// Đổi loại track audio ↔ instrument — chỉ khi track TRỐNG (không còn clip). Sang audio thì bỏ nhạc cụ.
  bool setTrackKind(int track, TrackKind kind) {
    if (_editable() == null) return false;
    _ensureTrack(track);
    final t = state!.project.trackAt(track)!;
    if (t.kind == kind || t.clips.isNotEmpty) return false;
    if (!_callOk('track.configure', trackConfigure(track, t.copyWith(kind: kind)))) return false;
    _updateTrack(
      track,
      (x) => kind == TrackKind.audio ? x.copyWith(kind: kind, instrument: null) : x.copyWith(kind: kind),
    );
    return true;
  }

  /// Tên scene (chỉ model — engine không dùng tên scene).
  void renameScene(int scene, String name) {
    final s = _editable();
    final clean = cleanName(name);
    if (s == null || clean.isEmpty) return;
    final scenes = [
      for (final x in s.project.scenes) x.index == scene ? x.copyWith(name: clean) : x,
      if (!s.project.scenes.any((x) => x.index == scene)) Scene(index: scene, name: clean),
    ]..sort((a, b) => a.index.compareTo(b.index));
    state = s.copyWith(project: s.project.copyWith(scenes: scenes));
  }

  // ───────── Clip ─────────

  /// Màu track (chỉ model — engine không dùng màu).
  void setTrackColor(int track, String hex) {
    if (_editable() == null) return;
    _ensureTrack(track);
    final t = state!.project.trackAt(track)!.copyWith(color: hex);
    // Màu gửi kèm track.configure → engine chọn màu LED Launchpad gần nhất (05 §3, P4-05).
    _engine.call({'op': 'track.configure', ...trackConfigure(track, t)});
    _updateTrack(track, (x) => x.copyWith(color: hex));
  }

  /// Tên clip (chỉ model).
  void renameClip(int track, int slot, String name) {
    final clean = cleanName(name);
    if (_editable() == null || clean.isEmpty) return; // rỗng / toàn ký tự điều khiển → giữ tên cũ
    _updateTrack(
      track,
      (t) => t.copyWith(clips: [for (final c in t.clips) c.slot == slot ? c.copyWith(name: clean) : c]),
    );
  }

  /// Xoá clip: `clip.clear` trước, rồi model.
  void deleteClip(int track, int slot) {
    final s = _editable();
    if (s?.project.trackAt(track)?.clipAt(slot) == null) return;
    _engine.call({'op': 'clip.clear', 'track': track, 'slot': slot});
    _updateTrack(track, (t) => t.copyWith(clips: [...t.clips.where((c) => c.slot != slot)]));
    _refreshTempo(); // xoá clip cuối khi transport dừng → chờ vòng đầu; engine xác nhận bằng TEMPO_CHANGED(0)
  }

  /// Đặt [clip] (bản copy, id mới) vào ô trống ([track], [slot]): gửi engine rồi cập nhật model.
  /// Trả false nếu ô đã có clip.
  bool placeClip(Clip clip, int track, int slot) {
    if (_editable() == null) return false;
    _ensureTrack(track);
    if (state!.project.trackAt(track)!.clipAt(slot) != null) return false;
    final copy = clip.copyWith(slot: slot, id: newId('c'));
    switch (copy) {
      case MidiClip():
        _engine.call({
          'op': 'clip.setMidi',
          'track': track,
          'slot': slot,
          'clipId': copy.id,
          'lengthBeats': copy.lengthBeats,
          'notes': [for (final n in copy.notes) n.toJson()],
        });
      case AudioClip():
        // Hai clip dùng chung 1 file audio là hợp lệ (dọn rác 06 §5.5 đếm tham chiếu).
        _engine.call({
          'op': 'clip.setAudio',
          'track': track,
          'slot': slot,
          'clipId': copy.id,
          'file': copy.file,
          'lengthBeats': copy.lengthBeats,
          'originalBpm': copy.originalBpm,
          'warp': copy.warp.name,
          'gainDb': copy.gainDb,
        });
    }
    _updateTrack(track, (t) => t.copyWith(clips: [...t.clips, copy]));
    return true;
  }

  /// Nhân bản sang ô trống kế tiếp bên dưới trong cùng track. Trả slot mới, -1 nếu hết chỗ.
  int duplicateClip(int track, int slot) {
    final t = _editable()?.project.trackAt(track);
    final c = t?.clipAt(slot);
    if (t == null || c == null) return -1;
    for (var s = slot + 1; s < 8; s++) {
      if (t.clipAt(s) == null) return placeClip(c, track, s) ? s : -1;
    }
    return -1;
  }

  /// 07 §4.1 bước 5: thu xong → `clip.info` → thêm clip vào model (autosave lưu ngay sau đó).
  void _onEngineEvent(EngineEvent e) {
    if (e is TempoChanged) {
      // Pedal mode (vòng đầu suy ra BPM) hoặc Link: engine là nguồn — chỉ cập nhật model, không gửi lại SET_BPM.
      final s = state;
      if (s != null && e.bpm >= 20 && e.bpm <= 300 && s.project.transport.bpm != e.bpm) {
        state = s.copyWith(
          project: s.project.copyWith(transport: s.project.transport.copyWith(bpm: e.bpm)),
        );
      }
      _refreshTempo();
      return;
    }
    if (e is! RecordingFinished || e.track < 0 || e.slot < 0) return; // track -1 = spike P0
    _onRecordingFinished(e);
    _refreshTempo(fallbackFirstLoop: state?.project.trackAt(e.track)?.clipAt(e.slot)?.lengthBeats);
  }

  void _onRecordingFinished(RecordingFinished e) {
    final s = state;
    if (s == null) return;
    final Map<String, dynamic> info;
    try {
      info = _engine.callOk('clip.info', {'track': e.track, 'slot': e.slot});
    } on EngineCallException catch (ex) {
      state = s.copyWith(errors: List.unmodifiable([...s.errors, AppError(ex.code, op: 'clip.info')]));
      return;
    }
    if (info['kind'] == 'empty') {
      state = s.copyWith(
        errors: List.unmodifiable([...s.errors, AppError('CELL_EMPTY', op: 'clip.info ${e.track},${e.slot}')]),
      );
      return;
    }
    _ensureTrack(e.track);
    final existing = state!.project.trackAt(e.track)?.clipAt(e.slot);
    if (existing != null && existing.id == info['clipId']) {
      _refreshAfterOverdub(e.track, existing, info); // hết một lượt overdub (04 §5.4)
      return;
    }
    var clip = clipFromInfo(e.slot, info);
    if (clip is MidiClip) {
      // P2-18: engine đã quantize lúc thu (midi.setRecordQuantize); chỉ cần lấy nốt về model.
      try {
        clip = clip.copyWith(notes: _fetchNotes(e.track, e.slot));
      } on EngineCallException catch (ex) {
        final cur = state!;
        state = cur.copyWith(errors: List.unmodifiable([...cur.errors, AppError(ex.code, op: 'clip.getMidi')]));
      }
    }
    _updateTrack(e.track, (t) => t.copyWith(clips: [...t.clips.where((c) => c.slot != e.slot), clip]));
  }

  /// Overdub xong trên clip đã có: giữ tên / id, đọc lại nội dung. MIDI → `clip.getMidi`; audio → file/độ dài từ
  /// `clip.info` + tăng revision để waveform lấy lại peaks. Autosave nghe thay đổi model như thu mới.
  void _refreshAfterOverdub(int track, Clip existing, Map<String, dynamic> info) {
    final len = (info['lengthBeats'] as num?)?.toDouble();
    switch (existing) {
      case MidiClip():
        try {
          _replaceClip(
            track,
            existing.slot,
            existing.copyWith(notes: _fetchNotes(track, existing.slot), lengthBeats: len ?? existing.lengthBeats),
          );
        } on EngineCallException catch (ex) {
          final cur = state!;
          state = cur.copyWith(errors: List.unmodifiable([...cur.errors, AppError(ex.code, op: 'clip.getMidi')]));
        }
      case AudioClip():
        _replaceClip(
          track,
          existing.slot,
          existing.copyWith(file: info['file'] as String? ?? existing.file, lengthBeats: len ?? existing.lengthBeats),
        );
        final s = state!;
        state = s.copyWith(clipRevisions: {...s.clipRevisions, existing.id: s.revisionOf(existing.id) + 1});
    }
  }

  /// `clip.info` (05 §3) → model Clip.
  static Clip clipFromInfo(int slot, Map<String, dynamic> info) {
    final id = (info['clipId'] as String?)?.isNotEmpty == true ? info['clipId'] as String : newId('c');
    final len = (info['lengthBeats'] as num?)?.toDouble() ?? 4;
    if (info['kind'] == 'audio') {
      return Clip.audio(
        slot: slot,
        id: id,
        name: S.sessionBanThu(slot + 1),
        file: info['file'] as String? ?? 'audio/$id.caf',
        lengthBeats: len,
        originalBpm: (info['originalBpm'] as num?)?.toDouble() ?? 120,
        warp: info['warp'] == 'repitch' ? WarpMode.repitch : WarpMode.stretch,
        gainDb: (info['gainDb'] as num?)?.toDouble() ?? 0,
      );
    }
    return Clip.midi(slot: slot, id: id, name: S.sessionMidiClipName(slot + 1), lengthBeats: len);
  }

  // ───────── MIDI clip (P2-18/19) ─────────

  List<Note> _fetchNotes(int track, int slot) {
    final r = _engine.callOk('clip.getMidi', {'track': track, 'slot': slot});
    return [for (final n in (r['notes'] as List? ?? const [])) Note.fromJson((n as Map).cast<String, dynamic>())];
  }

  /// `midiClip.quantize` rồi đọc lại nốt từ engine (nguồn sự thật của phép quantize).
  /// Ô có lớp overdub hoàn tác được (`clip.info.hasUndo`, 05 §3). Engine chưa có trường → false.
  bool hasUndo(int track, int slot) {
    final r = _engine.call({'op': 'clip.info', 'track': track, 'slot': slot});
    return r['ok'] == true && (r['result'] as Map)['hasUndo'] == true;
  }

  /// Bỏ lớp overdub gần nhất (`clip.undoOverdub`). Clip MIDI: lấy lại nốt từ engine để model khớp.
  bool undoOverdub(int track, int slot) {
    if (_editable() == null) return false;
    if (!_callOk('clip.undoOverdub', {'track': track, 'slot': slot})) return false;
    final c = state!.project.trackAt(track)?.clipAt(slot);
    if (c is MidiClip) _replaceClip(track, slot, c.copyWith(notes: _fetchNotes(track, slot)));
    return true;
  }

  void quantizeClip(int track, int slot, double gridBeats) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! MidiClip) return;
    try {
      _engine.callOk('midiClip.quantize', {'track': track, 'slot': slot, 'grid': gridBeats});
      final notes = _fetchNotes(track, slot);
      _replaceClip(track, slot, c.copyWith(notes: notes));
    } on EngineCallException catch (ex) {
      final s = state!;
      state = s.copyWith(errors: List.unmodifiable([...s.errors, AppError(ex.code, op: 'midiClip.quantize')]));
    }
  }

  /// Xoá các nốt theo chỉ số: engine nhận lại toàn bộ nốt còn lại qua `clip.setMidi`.
  void deleteNotes(int track, int slot, Set<int> indices) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! MidiClip || indices.isEmpty) return;
    final keep = [
      for (var i = 0; i < c.notes.length; i++)
        if (!indices.contains(i)) c.notes[i],
    ];
    _setMidi(track, slot, c.copyWith(notes: keep));
  }

  /// Xoá hết nốt, giữ clip (xoá hẳn clip là `deleteClip`).
  void clearNotes(int track, int slot) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! MidiClip) return;
    _setMidi(track, slot, c.copyWith(notes: const []));
  }

  /// Piano roll (07 §4.1b): thay toàn bộ nốt (và độ dài, nếu có) của clip MIDI — `clip.setMidi` rồi model.
  void setMidiNotes(int track, int slot, List<Note> notes, {double? lengthBeats}) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! MidiClip) return;
    _setMidi(track, slot, c.copyWith(notes: notes, lengthBeats: lengthBeats ?? c.lengthBeats));
  }

  /// Clip MIDI trống [bars] bar ở ô trống của track instrument (Edit → nhấn giữ ô trống, 07 §4.1b).
  bool createMidiClip(int track, int slot, int bars) {
    final s = _editable();
    final t = s?.project.trackAt(track);
    if (s == null || t == null || t.kind != TrackKind.instrument || t.clipAt(slot) != null) return false;
    final c =
        Clip.midi(
              slot: slot,
              id: newId('c'),
              name: S.sessionMidiClipName(slot + 1),
              lengthBeats: bars * s.project.transport.timeSignature.first.toDouble(),
            )
            as MidiClip;
    _engine.call({
      'op': 'clip.setMidi',
      'track': track,
      'slot': slot,
      'clipId': c.id,
      'lengthBeats': c.lengthBeats,
      'notes': const <Object>[],
    });
    _updateTrack(track, (x) => x.copyWith(clips: [...x.clips, c]));
    return true;
  }

  void _setMidi(int track, int slot, MidiClip c) {
    _engine.call({
      'op': 'clip.setMidi',
      'track': track,
      'slot': slot,
      'clipId': c.id,
      'lengthBeats': c.lengthBeats,
      'notes': [for (final n in c.notes) n.toJson()],
    });
    _replaceClip(track, slot, c);
  }

  // ───────── Audio clip (P2-21) ─────────

  /// Vùng loop: `clip.setLoopRegion` rồi model (`loop.startSample`, `lengthBeats`).
  void setLoopRegion(int track, int slot, {required int startSample, required double lengthBeats}) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! AudioClip) return;
    final start = startSample < 0 ? 0 : startSample;
    final len = lengthBeats < 0.25 ? 0.25 : lengthBeats;
    _engine.call({'op': 'clip.setLoopRegion', 'track': track, 'slot': slot, 'startSample': start, 'lengthBeats': len});
    _replaceClip(
      track,
      slot,
      c.copyWith(
        loop: AudioLoop(startSample: start),
        lengthBeats: len,
      ),
    );
  }

  /// Gain / warp của clip audio (`clip.setParams`, engine dùng ramp để không click).
  void setClipAudioParams(int track, int slot, {double? gainDb, WarpMode? warp}) {
    final c = _editable()?.project.trackAt(track)?.clipAt(slot);
    if (c is! AudioClip) return;
    final next = c.copyWith(gainDb: (gainDb ?? c.gainDb).clamp(-60.0, 12.0), warp: warp ?? c.warp);
    // Chỉ đổi tham số, engine không decode lại (05 §3 clip.setParams).
    _engine.call({
      'op': 'clip.setParams',
      'track': track,
      'slot': slot,
      if (gainDb != null) 'gainDb': next.gainDb,
      if (warp != null) 'warp': next.warp.name,
    });
    _replaceClip(track, slot, next);
  }

  void _sendAudio(int track, int slot, AudioClip c) => _engine.call({
    'op': 'clip.setAudio',
    'track': track,
    'slot': slot,
    'clipId': c.id,
    'file': c.file,
    'lengthBeats': c.lengthBeats,
    'originalBpm': c.originalBpm,
    'warp': c.warp.name,
    'gainDb': c.gainDb,
  });

  /// Đặt clip audio mới (ví dụ loop từ Browser, file đã chép vào project; `clip.tags` chép từ Library).
  bool addAudioClip(int track, int slot, AudioClip clip) {
    if (_editable() == null) return false;
    _ensureTrack(track);
    if (state!.project.trackAt(track)!.clipAt(slot) != null) return false;
    final c = clip.copyWith(slot: slot);
    _sendAudio(track, slot, c);
    _updateTrack(track, (t) => t.copyWith(clips: [...t.clips, c]));
    return true;
  }

  // ───────── Kéo / copy clip ở chế độ Edit (P2-21) ─────────

  /// Di chuyển clip sang ô trống (giữ id — file audio và cache peaks đi theo). Trả false nếu không được.
  bool moveClip(CellPos from, CellPos to) {
    final s = _editable();
    final c = s?.project.trackAt(from.track)?.clipAt(from.slot);
    if (c == null || from == to) return false;
    _ensureTrack(to.track);
    if (state!.project.trackAt(to.track)!.clipAt(to.slot) != null) return false;
    _engine.call({'op': 'clip.clear', 'track': from.track, 'slot': from.slot});
    final moved = c.copyWith(slot: to.slot);
    switch (moved) {
      case MidiClip():
        _engine.call({
          'op': 'clip.setMidi',
          'track': to.track,
          'slot': to.slot,
          'clipId': moved.id,
          'lengthBeats': moved.lengthBeats,
          'notes': [for (final n in moved.notes) n.toJson()],
        });
      case AudioClip():
        _sendAudio(to.track, to.slot, moved);
    }
    _updateTrack(from.track, (t) => t.copyWith(clips: [...t.clips.where((x) => x.slot != from.slot)]));
    _updateTrack(to.track, (t) => t.copyWith(clips: [...t.clips, moved]));
    return true;
  }

  // ───────── Nhạc cụ (P2-22) ─────────

  /// Gán nhạc cụ SFZ từ thư viện. Track audio được chuyển thành instrument (`track.configure`).
  void setInstrument(int track, InstrumentRef instrument) {
    if (_editable() == null) return;
    _ensureTrack(track);
    final t = state!.project.trackAt(track)!;
    if (t.kind != TrackKind.instrument) {
      _engine.call({'op': 'track.configure', ...trackConfigure(track, t.copyWith(kind: TrackKind.instrument))});
    }
    try {
      _engine.callJob('track.setInstrument', {'track': track, 'instrument': instrument.toJson()});
    } on EngineCallException catch (ex) {
      final s = state!;
      state = s.copyWith(errors: List.unmodifiable([...s.errors, AppError(ex.code, op: 'track.setInstrument')]));
    }
    _updateTrack(track, (x) => x.copyWith(kind: TrackKind.instrument, instrument: instrument));
  }

  /// Gắn nhạc cụ tự thu (P3-03): thêm vào `userInstruments` (06 §2) rồi gán cho track.
  /// Engine đã tạo nhạc cụ bằng `instrument.createFromRecording` trước đó.
  void attachUserInstrument(int track, UserInstrument instrument) {
    final s = _editable();
    if (s == null) return;
    state = s.copyWith(
      project: s.project.copyWith(
        userInstruments: [...s.project.userInstruments.where((u) => u.id != instrument.id), instrument],
      ),
    );
    setInstrument(track, InstrumentRef.user(id: instrument.id));
  }

  // ───────── MIDI learn + Link (P4-04/09, Settings) ─────────

  /// Thay toàn bộ mapping MIDI của project (06 §2 `midiMappings`) và báo engine (`midi.setMappings`).
  bool setMidiMappings(List<MidiMapping> mappings) {
    final s = _editable();
    if (s == null) return false;
    final ok = _callOk('midi.setMappings', {
      'mappings': [for (final m in mappings) m.toJson()],
    });
    if (!ok) return false;
    state = state!.copyWith(project: state!.project.copyWith(midiMappings: List.unmodifiable(mappings)));
    return true;
  }

  /// Ableton Link (06 §2 `link`): `link.enable` rồi model. Engine chưa có Link vẫn lưu lựa chọn.
  String? setLink({required bool enabled, required bool startStopSync}) {
    final s = _editable();
    if (s == null) return null;
    final r = _engine.call({'op': 'link.enable', 'enabled': enabled, 'startStopSync': startStopSync});
    state = s.copyWith(
      project: s.project.copyWith(
        link: LinkSettings(enabled: enabled, startStopSync: startStopSync),
      ),
    );
    return r['ok'] == true ? null : '${(r['error'] as Map?)?['code']}';
  }

  // ───────── Nhạc cụ tự thu: mode + ADSR (P3-07) ─────────

  /// Natural/Classic (05 §3 `instrument.setMode`).
  void setInstrumentMode(String instrumentId, InstrumentMode mode) {
    if (_editable() == null || _userInstrument(instrumentId) == null) return;
    if (!_callOk('instrument.setMode', {'instrumentId': instrumentId, 'mode': mode.name})) return;
    _updateUserInstrument(instrumentId, (u) => u.copyWith(mode: mode));
  }

  /// Gửi envelope lúc đang kéo knob (≤ 1 lần/frame), chưa ghi model — [setEnvelope] khi thả tay.
  void previewEnvelope(String instrumentId, Envelope e) {
    if (_editable() == null) return;
    _engine.call({'op': 'instrument.setEnvelope', ...envelopeParams(instrumentId, e)});
  }

  /// Envelope mới cho nhạc cụ tự thu: nốt mới dùng ngay, nốt đang kêu giữ envelope cũ (05 §3).
  void setEnvelope(String instrumentId, Envelope e) {
    if (_editable() == null || _userInstrument(instrumentId) == null) return;
    if (!_callOk('instrument.setEnvelope', envelopeParams(instrumentId, e))) return;
    _updateUserInstrument(instrumentId, (u) => u.copyWith(envelope: e));
  }

  /// Tham số `instrument.setEnvelope` (giây, s 0..1) — dùng chung với ProjectReplay.
  static Map<String, dynamic> envelopeParams(String id, Envelope e) => {
    'instrumentId': id,
    'a': e.a,
    'd': e.d,
    's': e.s,
    'r': e.r,
  };

  UserInstrument? _userInstrument(String id) {
    for (final u in state!.project.userInstruments) {
      if (u.id == id) return u;
    }
    return null;
  }

  void _updateUserInstrument(String id, UserInstrument Function(UserInstrument) edit) {
    final s = state!;
    state = s.copyWith(
      project: s.project.copyWith(
        userInstruments: [for (final u in s.project.userInstruments) u.id == id ? edit(u) : u],
      ),
    );
  }

  // ───────── FX (P3-16) ─────────
  // Slot cố định trong engine (04 §9); model giữ `tracks[].fx` liền nhau → chỉ số trong list = slot.

  /// Đặt FX [type] vào slot [index] (thêm vào slot trống kế tiếp, hoặc đổi loại) với tham số mặc định.
  bool setFx(int track, int index, FxType type) {
    if (_editable() == null) return false;
    _ensureTrack(track);
    final fx = state!.project.trackAt(track)!.fx;
    if (index < 0 || index > fx.length || index >= fxSlotsPerTrack) return false;
    final slot = FxSlot(type: type, params: fxDefaultParams(type));
    if (!_sendFxSet(track, index, slot)) return false;
    _updateTrack(track, (t) => t.copyWith(fx: [...t.fx.take(index), slot, ...t.fx.skip(index + 1)]));
    return true;
  }

  /// Xoá FX ở [index]. Các FX phía sau dồn lên để giữ thứ tự chuỗi: engine nhận fx.set lại cho từng slot
  /// dồn lên rồi fx.remove slot cuối (không dựa vào engine tự dồn).
  void removeFx(int track, int index) {
    if (_editable() == null) return;
    final fx = state!.project.trackAt(track)?.fx ?? const <FxSlot>[];
    if (index < 0 || index >= fx.length) return;
    final next = [...fx]..removeAt(index);
    for (var i = index; i < next.length; i++) {
      _sendFxSet(track, i, next[i]);
    }
    _callOk('fx.remove', {'track': track, 'index': fx.length - 1});
    _updateTrack(track, (t) => t.copyWith(fx: next));
  }

  /// Tham số lúc đang kéo (≤ 1 lần/frame): chỉ gửi `FX_PARAM`, model ghi ở [setFxParam] khi thả tay.
  void previewFxParam(int track, int index, int paramId, double value) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, track: track, slot: index, i0: paramId, f0: value);
  }

  void setFxParam(int track, int index, int paramId, double value) {
    if (_editable() == null) return;
    final fx = state!.project.trackAt(track)?.fx ?? const <FxSlot>[];
    if (index < 0 || index >= fx.length) return;
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, track: track, slot: index, i0: paramId, f0: value);
    _updateFx(track, index, (f) => f.copyWith(params: {...f.params, '$paramId': value}));
  }

  /// Bật/tắt nhanh lúc đang chơi (`LE_CMD_FX_BYPASS`; engine crossfade nên không click).
  void setFxBypass(int track, int index, bool bypass) {
    if (_editable() == null) return;
    final fx = state!.project.trackAt(track)?.fx ?? const <FxSlot>[];
    if (index < 0 || index >= fx.length) return;
    _engine.send(LeCommandType.LE_CMD_FX_BYPASS, track: track, slot: index, i0: bypass ? 1 : 0);
    _updateFx(track, index, (f) => f.copyWith(bypass: bypass));
  }

  /// EQ3 cố định của master (`master.eq3`): lúc kéo chỉ gửi lệnh.
  void previewMasterEq(int band, double db) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, slot: masterEqSlot, i0: band, f0: db);
  }

  void setMasterEq(int band, double db) {
    final s = _editable();
    if (s == null || band < 0 || band > 2) return;
    final v = db.clamp(-15.0, 15.0);
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, slot: masterEqSlot, i0: band, f0: v);
    final eq = [...s.project.master.eq3];
    while (eq.length < 3) {
      eq.add(0);
    }
    eq[band] = v;
    state = s.copyWith(
      project: s.project.copyWith(master: s.project.master.copyWith(eq3: eq)),
    );
  }

  /// Bypass EQ3 master (`FX_BYPASS track −1 slot 0`, 06 §2 `master.eq3Bypass`).
  void setMasterEqBypass(bool bypass) {
    final s = _editable();
    if (s == null) return;
    _engine.send(LeCommandType.LE_CMD_FX_BYPASS, slot: masterEqSlot, i0: bypass ? 1 : 0);
    state = s.copyWith(
      project: s.project.copyWith(master: s.project.master.copyWith(eq3Bypass: bypass)),
    );
  }

  /// Trần limiter master: lúc kéo chỉ gửi lệnh.
  void previewLimiterCeiling(double db) {
    if (_editable() == null) return;
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, slot: masterLimiterSlot, i0: 0, f0: db);
  }

  void setLimiterCeiling(double db) {
    final s = _editable();
    if (s == null) return;
    final v = limiterCeilingSpec.clamp(db);
    _engine.send(LeCommandType.LE_CMD_FX_PARAM, slot: masterLimiterSlot, i0: 0, f0: v);
    state = s.copyWith(
      project: s.project.copyWith(master: s.project.master.copyWith(limiterCeilingDb: v)),
    );
  }

  bool _sendFxSet(int track, int index, FxSlot f) =>
      _callOk('fx.set', {'track': track, 'index': index, 'type': f.type.name, 'params': f.params, 'bypass': f.bypass});

  void _updateFx(int track, int index, FxSlot Function(FxSlot) edit) => _updateTrack(
    track,
    (t) => t.copyWith(fx: [for (var i = 0; i < t.fx.length; i++) i == index ? edit(t.fx[i]) : t.fx[i]]),
  );

  /// Gọi op; lỗi → banner (không chặn app). Trả true nếu engine nhận.
  bool _callOk(String op, Map<String, dynamic> params) {
    try {
      _engine.callOk(op, params);
      return true;
    } on EngineCallException catch (ex) {
      final s = state!;
      state = s.copyWith(errors: List.unmodifiable([...s.errors, AppError(ex.code, op: op)]));
      return false;
    }
  }

  void _replaceClip(int track, int slot, Clip c) =>
      _updateTrack(track, (t) => t.copyWith(clips: [for (final x in t.clips) x.slot == slot ? c : x]));

  // ───────── Nội bộ ─────────

  ProjectSession? _editable() {
    final s = state;
    return s == null || s.readOnly ? null : s;
  }

  /// Cột chưa có track trong model → tạo track audio mặc định (monitor theo Settings) và báo engine.
  void _ensureTrack(int index) {
    final s = state!;
    if (s.project.trackAt(index) != null) return;
    final t = defaultTrack(index, monitor: ref.read(settingsProvider).defaultMonitor, name: S.sessionTrackName);
    _engine.call({'op': 'track.configure', ...trackConfigure(index, t)});
    if (t.monitor != MonitorMode.off) {
      _engine.send(LeCommandType.LE_CMD_TRACK_MONITOR, track: index, i0: t.monitor.leValue);
    }
    final tracks = [...s.project.tracks, t]..sort((a, b) => a.index.compareTo(b.index));
    state = s.copyWith(project: s.project.copyWith(tracks: tracks));
  }

  void _updateTrack(int index, Track Function(Track) edit) {
    final s = state!;
    state = s.copyWith(
      project: s.project.copyWith(tracks: [for (final t in s.project.tracks) t.index == index ? edit(t) : t]),
    );
  }

  void _updateMixer(int index, Mixer Function(Mixer) edit) =>
      _updateTrack(index, (t) => t.copyWith(mixer: edit(t.mixer)));
}

final projectControllerProvider = NotifierProvider<ProjectController, ProjectSession?>(ProjectController.new);
