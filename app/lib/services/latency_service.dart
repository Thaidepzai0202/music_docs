import 'package:engine_ffi/engine_ffi.dart';

/// Kết quả `latency.calibrate` (05 §3). Engine đã áp dụng [offsetSamples] ngay khi đo xong.
typedef CalibrationResult = ({
  int measuredSamples,
  int reportedSamples,
  int offsetSamples,
  int spreadSamples,
  double confidence,
});

/// Hiệu chỉnh độ trễ (P4-12). Lỗi → [EngineCallException].
class LatencyService {
  LatencyService(this._engine);

  final EngineApi _engine;

  /// L thực tế engine đang dùng (`LeState.latencyRoundTripSamples`).
  int get roundTripSamples => _engine.readState().latencyRoundTripSamples;

  double get sampleRate {
    final sr = _engine.readState().sampleRate;
    return sr > 0 ? sr.toDouble() : 48000;
  }

  /// Job đo loopback (cần audio chạy + micro).
  int calibrate() => _engine.callJob('latency.calibrate');

  /// Lý do đo thất bại: job failed `AUDIO_DEVICE`, `message` là một trong các mã này (05 §3).
  static const failReasons = {'NO_SIGNAL', 'TOO_NOISY', 'INCONSISTENT', 'DEVICE_CHANGED', 'TIMEOUT'};

  static CalibrationResult? parse(Map<String, dynamic> r) {
    int? i(String k) => (r[k] as num?)?.toInt();
    final measured = i('measuredSamples'), reported = i('reportedSamples'), offset = i('offsetSamples');
    if (measured == null || reported == null || offset == null) return null;
    return (
      measuredSamples: measured,
      reportedSamples: reported,
      offsetSamples: offset,
      spreadSamples: i('spreadSamples') ?? 0,
      confidence: (r['confidence'] as num?)?.toDouble() ?? 0,
    );
  }

  /// Bù độ trễ chỉnh tay (dương = take dời sớm lên).
  void setOffset(int samples) => _engine.callOk('latency.setOffset', {'samples': samples});
}
