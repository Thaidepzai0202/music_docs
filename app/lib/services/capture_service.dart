import 'package:engine_ffi/engine_ffi.dart';

/// Thu một mẫu cho sampler rồi tạo nhạc cụ (P3-02/03/05): `capture.*`, `instrument.createFromRecording`.
/// Việc gắn nhạc cụ vào project đi qua `ProjectController.attachUserInstrument`. Lỗi → [EngineCallException].
class CaptureService {
  CaptureService(this._engine);

  final EngineApi _engine;

  void start(String path, double maxSeconds) =>
      _engine.callOk('capture.start', {'path': path, 'maxSeconds': maxSeconds});

  /// Dừng thu (idempotent) → file + số giây.
  ({String? file, double seconds}) stop() {
    final r = _engine.callOk('capture.stop');
    return (file: r['file'] as String?, seconds: (r['seconds'] as num?)?.toDouble() ?? 0);
  }

  /// Dừng khi huỷ: không ném lỗi.
  void abort() => _engine.call({'op': 'capture.stop'});

  /// Tự dừng ở maxSeconds (`RECORDING_FINISHED(-2, -2)`).
  Stream<RecordingFinished> get finished =>
      _engine.events.where((e) => e is RecordingFinished && e.track == -2).cast<RecordingFinished>();

  /// Job phân tích: trim + nốt gốc + peaks.
  int analyze(String file) => _engine.callJob('capture.analyze', {'file': file});

  /// Job tạo nhạc cụ 13 zone.
  int createInstrument({
    required String instrumentId,
    required String file,
    required int trimStartSample,
    required int trimEndSample,
    int? rootNote,
    required String mode,
  }) => _engine.callJob('instrument.createFromRecording', {
    'instrumentId': instrumentId,
    'file': file,
    'trimStartSample': trimStartSample,
    'trimEndSample': trimEndSample,
    'rootNote': ?rootNote,
    'mode': mode,
  });
}
