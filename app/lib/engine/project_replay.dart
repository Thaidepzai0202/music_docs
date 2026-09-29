import 'package:engine_ffi/engine_ffi.dart';

import '../features/fx/fx_specs.dart';
import '../model/project.dart';
import 'app_error.dart';

final class ReplayResult {
  const ReplayResult({required this.jobIds, required this.errors});

  /// Job bất đồng bộ cần đợi (nạp instrument, decode audio, render nhạc cụ tự thu).
  final List<int> jobIds;

  /// Lệnh bị engine từ chối (theo mã). Không dừng giữa chừng: mở được phần nào hay phần đó (07 §4.3).
  final List<AppError> errors;
}

/// Dựng lại engine từ [Project] bằng chuỗi lệnh 06 §6. Tên op theo đúng 05 §3.
///
/// Chuỗi này cũng là định dạng kịch bản của harness (08 §3.2) → giữ thứ tự ổn định;
/// đổi thứ tự thì cập nhật snapshot test `open_commands` và báo phía engine.
class ProjectReplay {
  ProjectReplay(this._engine);

  final EngineApi _engine;

  ReplayResult run(Project p, {required String dir}) {
    final jobs = <int>[];
    final errors = <AppError>[];

    void call(String op, Map<String, dynamic> params) {
      try {
        _engine.callOk(op, params);
      } on EngineCallException catch (e) {
        errors.add(AppError(e.code, op: op));
      }
    }

    void job(String op, Map<String, dynamic> params) {
      try {
        jobs.add(_engine.callJob(op, params));
      } on EngineCallException catch (e) {
        errors.add(AppError(e.code, op: op));
      }
    }

    void send(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double d0 = 0}) {
      if (!_engine.send(type, track: track, slot: slot, i0: i0, f0: f0, d0: d0)) {
        errors.add(AppError('QUEUE_FULL', op: 'le_send ${leCommandName(type)}'));
      }
    }

    // Transport + master.
    final t = p.transport;
    call('project.open', {'dir': dir});
    send(LeCommandType.LE_CMD_SET_BPM, d0: t.bpm);
    call('transport.setTimeSignature', {'num': t.timeSignature[0], 'den': t.timeSignature[1]});
    // 05 §3: ngay sau SET_BPM / timeSignature; firstLoopBeats để các vòng sau vẫn làm tròn đúng sau khi mở lại.
    call('transport.setTempoMode', {
      'mode': t.tempoMode.name,
      if (t.firstLoopBeats != null) 'firstLoopBeats': t.firstLoopBeats,
    });
    send(LeCommandType.LE_CMD_SET_QUANTIZE, i0: t.quantize.leValue);
    send(LeCommandType.LE_CMD_METRONOME, i0: t.metronome.mode.leValue, f0: t.metronome.volume);
    send(LeCommandType.LE_CMD_SET_COUNT_IN, i0: t.countInBars);
    send(LeCommandType.LE_CMD_MASTER_GAIN, f0: p.master.gainDb);
    // Chuỗi master cố định (track −1): project.open đưa về mặc định → chỉ gửi phần khác mặc định (06 §6).
    final eq = p.master.eq3;
    if (eq.any((db) => db != 0)) {
      for (var band = 0; band < 3 && band < eq.length; band++) {
        send(LeCommandType.LE_CMD_FX_PARAM, slot: masterEqSlot, i0: band, f0: eq[band]);
      }
    }
    if (p.master.eq3Bypass) send(LeCommandType.LE_CMD_FX_BYPASS, slot: masterEqSlot, i0: 1);
    if (p.master.limiterCeilingDb != limiterCeilingSpec.defaultValue) {
      send(LeCommandType.LE_CMD_FX_PARAM, slot: masterLimiterSlot, i0: 0, f0: p.master.limiterCeilingDb);
    }

    // Nhạc cụ tự thu TRƯỚC vòng track: track.setInstrument {kind:"user"} cần nhạc cụ đã tồn tại (06 §6).
    for (final ui in p.userInstruments) {
      job('instrument.createFromRecording', {
        'instrumentId': ui.id,
        'file': ui.source,
        'rootNote': ui.rootNote,
        'mode': ui.mode.name,
      });
      // Envelope gửi ngay sau (engine đăng ký id khi nhận lệnh, không đợi job xong).
      final e = ui.envelope;
      call('instrument.setEnvelope', {'instrumentId': ui.id, 'a': e.a, 'd': e.d, 's': e.s, 'r': e.r});
    }
    // Track theo thứ tự cột, clip theo thứ tự ô.
    final tracks = [...p.tracks]..sort((a, b) => a.index.compareTo(b.index));
    for (final tr in tracks) {
      final ti = tr.index;
      call('track.configure', trackConfigure(ti, tr));
      if (tr.instrument case final inst?) {
        job('track.setInstrument', {'track': ti, 'instrument': inst.toJson()});
      }
      final m = tr.mixer;
      send(LeCommandType.LE_CMD_TRACK_GAIN, track: ti, f0: m.gainDb);
      send(LeCommandType.LE_CMD_TRACK_PAN, track: ti, f0: m.pan);
      send(LeCommandType.LE_CMD_TRACK_MUTE, track: ti, i0: m.mute ? 1 : 0);
      send(LeCommandType.LE_CMD_TRACK_SOLO, track: ti, i0: m.solo ? 1 : 0);
      send(LeCommandType.LE_CMD_TRACK_MONITOR, track: ti, i0: tr.monitor.leValue);
      for (var i = 0; i < tr.fx.length; i++) {
        final fx = tr.fx[i];
        // bypass đi kèm fx.set; LE_CMD_FX_BYPASS chỉ dùng bật/tắt nhanh lúc đang chơi (05 §3).
        call('fx.set', {'track': ti, 'index': i, 'type': fx.type.name, 'params': fx.params, 'bypass': fx.bypass});
      }
      final clips = [...tr.clips]..sort((a, b) => a.slot.compareTo(b.slot));
      for (final c in clips) {
        switch (c) {
          case AudioClip():
            job('clip.setAudio', {
              'track': ti,
              'slot': c.slot,
              'clipId': c.id,
              'file': c.file,
              'lengthBeats': c.lengthBeats,
              'originalBpm': c.originalBpm,
              'warp': c.warp.name,
              'gainDb': c.gainDb,
            });
          case MidiClip():
            call('clip.setMidi', {
              'track': ti,
              'slot': c.slot,
              'clipId': c.id,
              'lengthBeats': c.lengthBeats,
              'notes': [for (final n in c.notes) n.toJson()],
            });
        }
      }
    }

    call('midi.setMappings', {
      'mappings': [for (final m in p.midiMappings) m.toJson()],
    });
    call('link.enable', {'enabled': p.link.enabled, 'startStopSync': p.link.startStopSync});

    return ReplayResult(jobIds: jobs, errors: errors);
  }
}

/// Tham số `track.configure` (05 §3): kind, name, và `color` "#RRGGBB" để engine chọn màu LED Launchpad (P4-05).
/// Màu model không đúng dạng thì không gửi (engine trả INVALID_ARG với màu sai).
Map<String, dynamic> trackConfigure(int track, Track t) => {
  'track': track,
  'kind': t.kind.name,
  'name': t.name,
  if (RegExp(r'^#[0-9A-Fa-f]{6}$').hasMatch(t.color)) 'color': t.color.toUpperCase(),
};
