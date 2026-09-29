import 'package:engine_ffi/engine_ffi.dart';

import '../features/settings/app_settings.dart';
import 'audio_device_service.dart';
import 'latency_service.dart';
import 'midi_service.dart';

/// Áp dụng phần cài đặt nằm ở ENGINE lúc mở app (trước khi dựng UI). Giá trị bằng mặc định engine thì bỏ qua.
/// Trả các lệnh bị từ chối (log, không chặn app).
List<String> applyEngineSettings(EngineApi engine, AppSettings s) {
  final errors = <String>[];
  void run(String op, void Function() f) {
    try {
      f();
    } on EngineCallException catch (e) {
      errors.add('$op: ${e.code}');
    }
  }

  // Quantize khi thu MIDI nằm ở engine (P1-30); là thiết lập toàn cục, project.open không reset.
  if (s.midiRecordQuantize != RecordQuantize.off) {
    run(
      'midi.setRecordQuantize',
      () => MidiService(engine, const EnginePlatform()).setRecordQuantize(s.midiRecordQuantize.gridBeats),
    );
  }
  if (s.bufferSize != 128) run('spike.setBufferSize', () => AudioDeviceService(engine).setBufferSize(s.bufferSize));
  if (s.latencyOffsetSamples != 0) {
    run('latency.setOffset', () => LatencyService(engine).setOffset(s.latencyOffsetSamples));
  }
  return errors;
}
