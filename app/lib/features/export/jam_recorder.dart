import 'dart:io';

import 'package:clock/clock.dart';
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../engine/engine_providers.dart';
import '../../services/export_service.dart';
import '../../services/service_providers.dart';
import '../session/project_controller.dart';

export '../../services/export_service.dart' show ExportResult;

/// `<Documents>/Exports` (P3-20). Bật `UIFileSharingEnabled` trong Info.plist → thấy trong app Files.
final exportsDirProvider = Provider<Directory>(
  (ref) => Directory('${ref.watch(engineBootstrapProvider).dataDir}/Exports'),
);

/// "1:05" từ số giây.
String formatSeconds(double s) {
  final t = s.round();
  return '${t ~/ 60}:${(t % 60).toString().padLeft(2, '0')}';
}

/// Tên file an toàn: bỏ ký tự cấm trong tên file, khoảng trắng → "_".
String safeFileName(String s) {
  final cleaned = s.trim().replaceAll(RegExp(r'[\\/:*?"<>|]'), '').replaceAll(RegExp(r'\s+'), '_');
  return cleaned.isEmpty ? 'export' : cleaned;
}

/// `20260929_143012` — đặt sau tên file để không ghi đè bản cũ.
String fileStamp(DateTime t) {
  String two(int v) => v.toString().padLeft(2, '0');
  return '${t.year}${two(t.month)}${two(t.day)}_${two(t.hour)}${two(t.minute)}${two(t.second)}';
}

@immutable
final class JamState {
  const JamState({this.path, this.startedAt, this.last});

  /// Khác null khi đang ghi.
  final String? path;
  final DateTime? startedAt;

  /// Bản ghi xong gần nhất.
  final ExportResult? last;

  bool get recording => path != null;
}

/// Ghi buổi jam (04 §12, 05 §3 `export.jamStart` / `export.jamStop`): ghi đầu ra master theo thời gian thực
/// vào `Exports/jam_<thời điểm>.wav`. Đóng project khi đang ghi → tự dừng (file vẫn giữ).
class JamRecorder extends Notifier<JamState> {
  @override
  JamState build() {
    ref.listen(projectControllerProvider.select((s) => s == null), (_, closed) {
      if (closed && state.recording) stop();
    });
    return const JamState();
  }

  /// Bắt đầu ghi. Trả mã lỗi engine (null = ok).
  String? start() {
    if (state.recording) return null;
    final dir = ref.read(exportsDirProvider);
    final t = clock.now(); // package:clock → test (FakeAsync) có đồng hồ giả
    final path = '${dir.path}/jam_${fileStamp(t)}.wav';
    try {
      dir.createSync(recursive: true);
      ref.read(exportServiceProvider).jamStart(path);
    } on EngineCallException catch (e) {
      return e.code;
    } on FileSystemException {
      return 'FILE_NOT_FOUND'; // không tạo được thư mục Exports (mã, không hiện message hệ thống)
    }
    state = JamState(path: path, startedAt: t, last: state.last);
    return null;
  }

  /// Dừng ghi → kết quả (null nếu chưa ghi hoặc engine lỗi).
  ExportResult? stop() {
    final path = state.path;
    if (path == null) return null;
    ExportResult? result;
    try {
      result = ref.read(exportServiceProvider).jamStop(fallbackPath: path);
    } on EngineCallException {
      result = null;
    }
    state = JamState(last: result ?? state.last);
    return result;
  }
}

final jamRecorderProvider = NotifierProvider<JamRecorder, JamState>(JamRecorder.new);
