import 'package:flutter/scheduler.dart';

/// Gọi [sink] tối đa 1 lần mỗi frame với giá trị mới nhất (07 §3.3: engine có smoothing sẵn).
///
/// Giá trị đầu tiên trong một frame được gửi NGAY (lệnh đi trước UI, 07 §6.6); các giá trị sau
/// trong cùng frame chỉ giữ lại bản mới nhất và gửi ở đầu frame kế tiếp.
class FrameThrottle<T> {
  FrameThrottle(this.sink);

  final void Function(T value) sink;
  bool _sentThisFrame = false;
  bool _scheduled = false;
  bool _hasPending = false;
  bool _disposed = false;
  T? _pending;

  void add(T value) {
    if (_disposed) return;
    if (!_sentThisFrame) {
      _sentThisFrame = true;
      sink(value);
    } else {
      _pending = value;
      _hasPending = true;
    }
    _schedule();
  }

  void _schedule() {
    if (_scheduled) return;
    _scheduled = true;
    SchedulerBinding.instance.scheduleFrameCallback(_onFrame);
  }

  void _onFrame(Duration _) {
    _scheduled = false;
    if (_disposed) return;
    if (_hasPending) {
      _hasPending = false;
      sink(_pending as T);
      _sentThisFrame = true; // frame này đã dùng suất gửi
      _schedule();
    } else {
      _sentThisFrame = false;
    }
  }

  void dispose() => _disposed = true;
}
