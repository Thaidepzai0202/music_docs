import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';

import '../model/project.dart';

/// Một thiết bị MIDI (`midi.listDevices`).
@immutable
final class MidiDevice {
  const MidiDevice({required this.id, required this.name, required this.enabled, this.open = false});

  factory MidiDevice.fromJson(Map<dynamic, dynamic> j) =>
      MidiDevice(id: '${j['id']}', name: '${j['name']}', enabled: j['enabled'] == true, open: j['open'] == true);

  final String id;
  final String name;
  final bool enabled;

  /// Cổng đang mở (thiết bị cắm + enabled). Output không có trường này.
  final bool open;
}

/// Op MIDI không đổi project (07 §5): thiết bị, learn, quantize khi thu, ghép Bluetooth MIDI.
/// Mapping của project đi qua `ProjectController.setMidiMappings`. Lỗi engine → [EngineCallException].
class MidiService {
  MidiService(this._engine, this._platform);

  final EngineApi _engine;
  final EnginePlatform _platform;

  ({List<MidiDevice> inputs, List<MidiDevice> outputs}) listDevices() {
    final r = _engine.callOk('midi.listDevices');
    List<MidiDevice> list(Object? v) => [for (final d in (v as List? ?? const [])) MidiDevice.fromJson(d as Map)];
    return (inputs: list(r['inputs']), outputs: list(r['outputs']));
  }

  void enableDevice(String id, bool enabled) => _engine.callOk('midi.enableDevice', {'id': id, 'enabled': enabled});

  /// Thiết bị cắm/rút (`MIDI_DEVICES`) → gọi lại [listDevices].
  Stream<void> get devicesChanged => _engine.events.where((e) => e is MidiDevicesChanged);

  void learnStart(Map<String, dynamic> target) => _engine.callOk('midi.learnStart', {'target': target});

  void learnCancel() => _engine.call({'op': 'midi.learnCancel'});

  /// `MIDI_LEARNED` (chỉ có kind + number; nguồn đầy đủ lấy bằng [learnResult]).
  Stream<MidiLearned> get learned => _engine.events.where((e) => e is MidiLearned).cast<MidiLearned>();

  /// Nguồn của lần learn gần nhất (05 §3 `midi.learnResult`). Engine không trả được → nguồn "mọi thiết bị,
  /// mọi kênh" dựng từ [fallback] (event).
  MidiSource learnResult(MidiLearned fallback) {
    final r = _engine.call({'op': 'midi.learnResult'});
    final src = r['ok'] == true ? (r['result'] as Map).cast<String, dynamic>() : const <String, dynamic>{};
    return MidiSource(
      device: src['deviceName'] as String? ?? '',
      kind: src['kind'] as String? ?? (fallback.isCc ? 'cc' : 'note'),
      channel: (src['channel'] as num?)?.toInt() ?? -1,
      number: (src['number'] as num?)?.toInt() ?? fallback.number,
    );
  }

  /// Quantize nốt khi thu MIDI (P1-30) — thiết lập toàn cục của engine.
  void setRecordQuantize(double gridBeats) => _engine.callOk('midi.setRecordQuantize', {'grid': gridBeats});

  /// Mở màn ghép Bluetooth MIDI của iOS (`CABTMIDICentralViewController`, P4-03). Xong thì gọi lại [listDevices].
  Future<void> pairBluetooth() => _platform.showBluetoothMidi();
}
