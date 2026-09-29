import 'dart:typed_data';

import 'package:engine_ffi/engine_ffi.dart';

/// Peaks waveform của clip audio (P2-20, `le_get_peaks`): 3 mức 256 / 2048 / 16384 sample mỗi cặp (04 §5.7).
class PeaksService {
  PeaksService(this._engine);

  final EngineApi _engine;

  /// Cặp [min, max] xen kẽ; null nếu clip chưa decode / không phải audio.
  Float32List? peaks(String clipId, int level, int maxPairs) => _engine.getPeaks(clipId, level, maxPairs);
}
