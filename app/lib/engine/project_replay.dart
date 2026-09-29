import 'package:engine_ffi/engine_ffi.dart';

import '../model/project.dart';

final class ReplayResult {
  const ReplayResult({required this.jobIds, required this.errors});

  /// Job bất đồng bộ cần đợi (nạp instrument, decode audio, render nhạc cụ tự thu).
  final List<int> jobIds;

  /// Lệnh bị engine từ chối. Không dừng giữa chừng: mở được phần nào hay phần đó (07 §4.3).
  final List<String> errors;
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
    final errors = <String>[];

    void call(String op, Map<String, dynamic> params) {
      try {
        _engine.callOk(op, params);
      } on EngineCallException catch (e) {
        errors.add('$op: ${e.code} ${e.message}'.trim());
      }
    }

    void job(String op, Map<String, dynamic> params) {
      try {
        jobs.add(_engine.callJob(op, params));
      } on EngineCallException catch (e) {
        errors.add('$op: ${e.code} ${e.message}'.trim());
      }
    }

    void send(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double d0 = 0}) {
      if (!_engine.send(type, track: track, slot: slot, i0: i0, f0: f0, d0: d0)) {
        errors.add('le_send ${leCommandName(type)} bị từ chối');
      }
    }

    // Transport + master.
    final t = p.transport;
    call('project.open', {'dir': dir});
    send(LeCommandType.LE_CMD_SET_BPM, d0: t.bpm);
    call('transport.setTimeSignature', {'num': t.timeSignature[0], 'den': t.timeSignature[1]});
    send(LeCommandType.LE_CMD_SET_QUANTIZE, i0: t.quantize.leValue);
    send(LeCommandType.LE_CMD_METRONOME, i0: t.metronome.mode.leValue, f0: t.metronome.volume);
    send(LeCommandType.LE_CMD_SET_COUNT_IN, i0: t.countInBars);
    send(LeCommandType.LE_CMD_MASTER_GAIN, f0: p.master.gainDb);

    // Track theo thứ tự cột, clip theo thứ tự ô.
    final tracks = [...p.tracks]..sort((a, b) => a.index.compareTo(b.index));
    for (final tr in tracks) {
      final ti = tr.index;
      call('track.configure', {'track': ti, 'kind': tr.kind.name, 'name': tr.name});
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
        call('fx.set', {'track': ti, 'index': i, 'type': fx.type.name, 'params': fx.params});
        if (fx.bypass) send(LeCommandType.LE_CMD_FX_BYPASS, track: ti, slot: i, i0: 1);
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
              'gain': c.gainDb,
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

    for (final ui in p.userInstruments) {
      job('instrument.createFromRecording', {
        'instrumentId': ui.id,
        'file': ui.source,
        'rootNote': ui.rootNote,
        'mode': ui.mode.name,
      });
    }
    call('midi.setMappings', {
      'mappings': [for (final m in p.midiMappings) m.toJson()],
    });
    call('link.enable', {'enabled': p.link.enabled, 'startStopSync': p.link.startStopSync});

    return ReplayResult(jobIds: jobs, errors: errors);
  }
}
