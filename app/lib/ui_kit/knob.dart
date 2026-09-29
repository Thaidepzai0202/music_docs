import 'dart:math' as math;

import 'package:flutter/widgets.dart';

import '../app/theme.dart';
import 'drag_value.dart';

/// Knob xoay (P2-15): kéo dọc để đổi, chạm đúp về mặc định, [onChanged] tối đa 1 lần/frame.
/// [bipolar]: vẽ cung từ giữa (pan), không thì từ trái (send, mix…).
class Knob extends StatefulWidget {
  const Knob({
    super.key,
    required this.value,
    required this.onChanged,
    this.defaultValue = 0.5,
    this.onChangeEnd,
    this.bipolar = false,
    this.color = AppColors.queued,
    this.size = 40,
  });

  final double value;
  final double defaultValue;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;
  final bool bipolar;
  final Color color;
  final double size;

  @override
  State<Knob> createState() => _KnobState();
}

class _KnobState extends State<Knob> {
  late final DragValue _drag = DragValue(
    initial: widget.value,
    defaultValue: widget.defaultValue,
    onChanged: (v) => widget.onChanged(v),
    onChangeEnd: (v) => widget.onChangeEnd?.call(v),
  );
  late _KnobPainter _painter = _KnobPainter(_drag.value, widget.color, widget.bipolar);

  @override
  void didUpdateWidget(Knob old) {
    super.didUpdateWidget(old);
    _drag.syncFrom(widget.value);
    if (old.color != widget.color || old.bipolar != widget.bipolar) {
      _painter = _KnobPainter(_drag.value, widget.color, widget.bipolar);
    }
  }

  @override
  void dispose() {
    _drag.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return _drag.wrap(
      pixelsForFullRange: 200,
      child: RepaintBoundary(
        child: CustomPaint(size: Size.square(widget.size), painter: _painter),
      ),
    );
  }
}

class _KnobPainter extends CustomPainter {
  _KnobPainter(this.value, Color color, this.bipolar)
    : _arc = Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeWidth = 4
        ..strokeCap = StrokeCap.round,
      super(repaint: value);

  final ValueNotifier<double> value;
  final bool bipolar;
  final Paint _arc;
  static final _track = Paint()
    ..color = AppColors.border
    ..style = PaintingStyle.stroke
    ..strokeWidth = 4;
  static final _body = Paint()..color = AppColors.surface;
  static final _needle = Paint()
    ..color = AppColors.textPrimary
    ..strokeWidth = 2
    ..strokeCap = StrokeCap.round;

  static const _start = math.pi * 0.75; // 7h30
  static const _sweep = math.pi * 1.5; // tới 4h30

  @override
  void paint(Canvas canvas, Size size) {
    final c = size.center(Offset.zero);
    final r = size.shortestSide / 2 - 3;
    final rect = Rect.fromCircle(center: c, radius: r);
    canvas.drawCircle(c, r - 4, _body);
    canvas.drawArc(rect, _start, _sweep, false, _track);
    final v = value.value;
    final from = bipolar ? 0.5 : 0.0;
    canvas.drawArc(rect, _start + _sweep * math.min(from, v), _sweep * (v - from).abs(), false, _arc);
    final a = _start + _sweep * v;
    canvas.drawLine(c, c + Offset(math.cos(a), math.sin(a)) * (r - 6), _needle);
  }

  @override
  bool shouldRepaint(_KnobPainter old) => old.value != value || old.bipolar != bipolar || old._arc.color != _arc.color;
}
