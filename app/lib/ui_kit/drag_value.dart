import 'package:flutter/gestures.dart';
import 'package:flutter/widgets.dart';

import 'frame_throttle.dart';

/// Logic kéo chung cho Fader và Knob (07 §3.3), dùng `Listener` thô thay vì GestureDetector:
/// - Không đi qua gesture arena → nhiều ngón kéo nhiều control cùng lúc, không tranh nhau.
/// - Kéo dọc để đổi giá trị. Tinh chỉnh (×0.1): giữ thêm một ngón trên control, hoặc kéo ra xa
///   theo chiều ngang > [fineDistance].
/// - Chạm đúp → [defaultValue].
/// - [onChanged] bị gom tối đa 1 lần/frame; [value] (để vẽ) đổi ngay, không rebuild widget.
class DragValue {
  DragValue({
    required double initial,
    required this.defaultValue,
    required this.onChanged,
    this.onChangeEnd,
    this.fineDistance = 60,
    this.doubleTapWindow = const Duration(milliseconds: 300),
  }) : value = ValueNotifier(initial.clamp(0.0, 1.0)) {
    _throttle = FrameThrottle<double>((v) => onChanged(v));
  }

  /// Giá trị chuẩn hoá 0..1. Painter nghe trực tiếp (repaint), không qua setState.
  final ValueNotifier<double> value;
  final double defaultValue;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;
  final double fineDistance;
  final Duration doubleTapWindow;

  late final FrameThrottle<double> _throttle;
  int? _pointer;
  double _lastY = 0;
  double _startX = 0;
  Offset _downPos = Offset.zero;
  bool _moved = false;
  int _extraFingers = 0;
  Duration? _lastTapUp;

  bool get isDragging => _pointer != null;

  /// Đồng bộ từ model khi không kéo (ví dụ undo, mở project).
  void syncFrom(double v) {
    if (!isDragging) value.value = v.clamp(0.0, 1.0);
  }

  /// [pixelsForFullRange]: quãng kéo (px) đi hết 0 → 1 ở tốc độ thường.
  Widget wrap({required Widget child, required double pixelsForFullRange}) {
    return Listener(
      behavior: HitTestBehavior.opaque,
      onPointerDown: (e) => _down(e),
      onPointerMove: (e) => _move(e, pixelsForFullRange),
      onPointerUp: (e) => _up(e, cancelled: false),
      onPointerCancel: (e) => _up(e, cancelled: true),
      child: child,
    );
  }

  void _down(PointerDownEvent e) {
    if (_pointer != null) {
      _extraFingers++; // ngón thứ hai trên cùng control → tinh chỉnh
      return;
    }
    final last = _lastTapUp;
    if (last != null && e.timeStamp - last <= doubleTapWindow) {
      _lastTapUp = null;
      _set(defaultValue);
      onChangeEnd?.call(defaultValue);
      return; // chạm đúp: không bắt đầu kéo
    }
    _pointer = e.pointer;
    _lastY = e.localPosition.dy;
    _startX = e.localPosition.dx;
    _downPos = e.localPosition;
    _moved = false;
  }

  void _move(PointerMoveEvent e, double pixelsForFullRange) {
    if (e.pointer != _pointer) return;
    final dy = e.localPosition.dy - _lastY;
    _lastY = e.localPosition.dy;
    if ((e.localPosition - _downPos).distance > kTouchSlop) _moved = true;
    final fine = _extraFingers > 0 || (e.localPosition.dx - _startX).abs() > fineDistance;
    final delta = -dy / pixelsForFullRange * (fine ? 0.1 : 1.0); // kéo lên = tăng
    if (delta != 0) _set(value.value + delta);
  }

  void _up(PointerEvent e, {required bool cancelled}) {
    if (e.pointer != _pointer) {
      if (_extraFingers > 0) _extraFingers--;
      return;
    }
    _pointer = null;
    _extraFingers = 0;
    if (_moved) {
      _lastTapUp = null;
      onChangeEnd?.call(value.value);
    } else if (!cancelled) {
      _lastTapUp = e.timeStamp;
    }
  }

  void _set(double v) {
    final c = v.clamp(0.0, 1.0);
    if (c == value.value) return;
    _throttle.add(c); // lệnh trước…
    value.value = c; // …rồi mới vẽ
  }

  void dispose() {
    _throttle.dispose();
    value.dispose();
  }
}
