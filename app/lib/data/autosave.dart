import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';

import '../features/session/project_controller.dart';
import '../model/project.dart';

/// Autosave (06 §5.2, P2-25):
/// - 2 giây sau thay đổi cuối cùng (debounce),
/// - ngay khi thu xong (sau khi controller đã thêm clip vào model),
/// - ngay khi app xuống nền ([flush] do EngineLifecycleScope gọi),
/// - khi đóng project mà còn thay đổi chưa lưu.
///
/// Không lưu khi vừa mở project, khi đang mở, hoặc khi project chỉ đọc. Các lần lưu chạy nối tiếp nhau.
class Autosave {
  Autosave({required this.save, this.delay = const Duration(seconds: 2), DateTime Function()? now})
    : _now = now ?? DateTime.now;

  final Future<void> Function(Project project, String dir) save;
  final Duration delay;
  final DateTime Function() _now;

  /// Mã lỗi lần lưu gần nhất (null = ổn): `DISK_FULL` hoặc `FILE_WRITE` — Session hiện banner đã dịch (07 §8).
  final lastError = ValueNotifier<String?>(null);
  int saveCount = 0;

  Timer? _timer;
  ProjectSession? _pending;
  Future<void> _chain = Future.value();

  bool get hasPending => _pending != null;

  /// Gắn vào `ref.listen(projectControllerProvider, …)`.
  void onSessionChanged(ProjectSession? prev, ProjectSession? next) {
    if (next == null) {
      flush(); // đóng project: lưu nốt thay đổi còn chờ
      return;
    }
    if (next.readOnly || next.isLoading) return;
    if (prev == null || prev.dir != next.dir || prev.isLoading) return; // vừa mở xong: không có gì mới
    if (identical(prev.project, next.project) || prev.project == next.project) return; // chỉ đổi arm/lỗi…
    _pending = next;
    _timer?.cancel();
    _timer = Timer(delay, flush);
  }

  /// Engine báo thu xong → lưu ngay. Đợi 1 microtask để controller kịp thêm clip vào model trước.
  void onRecordingFinished() => scheduleMicrotask(() {
    if (_pending != null) flush();
  });

  /// Lưu ngay thay đổi đang chờ (nếu có).
  Future<void> flush() {
    _timer?.cancel();
    _timer = null;
    final p = _pending;
    if (p == null) return _chain;
    _pending = null;
    return _chain = _chain.then((_) async {
      try {
        await save(p.project.copyWith(modifiedAt: _now().toUtc()), p.dir);
        saveCount++;
        lastError.value = null;
      } catch (e) {
        lastError.value = errorCode(e);
      }
    });
  }

  /// ENOSPC (28 trên iOS/macOS/Linux) → hết bộ nhớ; còn lại → không ghi được file.
  static String errorCode(Object e) =>
      e is FileSystemException && e.osError?.errorCode == 28 ? 'DISK_FULL' : 'FILE_WRITE';

  void dispose() {
    _timer?.cancel();
    lastError.dispose();
  }
}
