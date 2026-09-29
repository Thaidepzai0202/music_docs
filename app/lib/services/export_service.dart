import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';

/// Kết quả `export.jamStop` `{file, seconds, droppedFrames}` / job `export.scene`
/// `{file, seconds, frames, clippedSamples, stems?}` (05 §3).
@immutable
final class ExportResult {
  const ExportResult({
    required this.file,
    required this.seconds,
    this.stems = const [],
    this.clippedSamples = 0,
    this.droppedFrames = 0,
  });

  factory ExportResult.fromJson(Map<String, dynamic> j, {required String fallbackPath}) => ExportResult(
    file: j['file'] as String? ?? fallbackPath,
    seconds: (j['seconds'] as num?)?.toDouble() ?? 0,
    stems: [for (final s in (j['stems'] as List? ?? const [])) '$s'],
    clippedSamples: (j['clippedSamples'] as num?)?.toInt() ?? 0,
    droppedFrames: (j['droppedFrames'] as num?)?.toInt() ?? 0,
  );

  final String file;
  final double seconds;
  final List<String> stems;

  /// Export scene: số mẫu bị clip ở master → gợi ý giảm gain master.
  final int clippedSamples;

  /// Ghi jam: số frame bị rớt (bộ nhớ ghi không kịp).
  final int droppedFrames;

  /// [droppedFrames] đổi ra ms theo [sampleRate].
  int droppedMs(double sampleRate) => (droppedFrames * 1000 / (sampleRate > 0 ? sampleRate : 48000)).round();

  List<String> get allFiles => [file, ...stems];
  String get fileName => file.split('/').last;
}

/// Export (04 §12): ghi buổi jam (real-time) và export scene (job offline). Lỗi → [EngineCallException].
class ExportService {
  ExportService(this._engine);

  final EngineApi _engine;

  void jamStart(String path) => _engine.callOk('export.jamStart', {'path': path});

  ExportResult jamStop({required String fallbackPath}) =>
      ExportResult.fromJson(_engine.callOk('export.jamStop'), fallbackPath: fallbackPath);

  /// Job render scene [scene] trong [bars] bar ra [path] (+ stems `<base>_t<n>` nếu [stems]).
  int exportScene({
    required int scene,
    required int bars,
    required String path,
    required String format,
    required bool stems,
  }) => _engine.callJob('export.scene', {'scene': scene, 'bars': bars, 'path': path, 'format': format, 'stems': stems});
}
