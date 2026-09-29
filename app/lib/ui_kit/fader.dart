import 'dart:math' as math;

import 'package:flutter/widgets.dart';

import '../app/theme.dart';
import 'drag_value.dart';

/// Thang gain: vị trí fader 0..1 ↔ dB. 0 → -inf (-120 dB), 1 → +6 dB, 0 dB ở ~0.71.
abstract final class GainScale {
  static const minDb = -120.0;
  static const maxDb = 6.0;

  static double toDb(double norm) => norm <= 0.0005 ? minDb : math.max(minDb, 40 * math.log(norm) / math.ln10 + maxDb);

  static double toNorm(double db) => db <= minDb ? 0 : math.pow(10, (db - maxDb) / 40).toDouble().clamp(0.0, 1.0);

  static String label(double db) => db <= minDb ? '-∞' : '${db >= 0 ? '+' : ''}${db.toStringAsFixed(1)}';
}

/// Fader dọc (P2-15). [value]/[defaultValue] chuẩn hoá 0..1; [onChanged] tối đa 1 lần/frame.
/// Kéo chỉ repaint painter trong RepaintBoundary riêng, không rebuild (07 §6).
class Fader extends StatefulWidget {
  const Fader({
    super.key,
    required this.value,
    required this.onChanged,
    this.defaultValue = 0.708, // 0 dB theo GainScale
    this.onChangeEnd,
    this.color = AppColors.play,
    this.width = 44,
  });

  final double value;
  final double defaultValue;
  final ValueChanged<double> onChanged;
  final ValueChanged<double>? onChangeEnd;
  final Color color;
  final double width;

  @override
  State<Fader> createState() => _FaderState();
}

class _FaderState extends State<Fader> {
  late final DragValue _drag = DragValue(
    initial: widget.value,
    defaultValue: widget.defaultValue,
    onChanged: (v) => widget.onChanged(v),
    onChangeEnd: (v) => widget.onChangeEnd?.call(v),
  );
  late _FaderPainter _painter = _FaderPainter(_drag.value, widget.color);

  @override
  void didUpdateWidget(Fader old) {
    super.didUpdateWidget(old);
    _drag.syncFrom(widget.value);
    if (old.color != widget.color) _painter = _FaderPainter(_drag.value, widget.color);
  }

  @override
  void dispose() {
    _drag.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) => _drag.wrap(
        pixelsForFullRange: math.max(80, c.maxHeight - 24),
        child: RepaintBoundary(
          child: CustomPaint(size: Size(widget.width, c.maxHeight), painter: _painter),
        ),
      ),
    );
  }
}

class _FaderPainter extends CustomPainter {
  _FaderPainter(this.value, Color color) : _fill = Paint()..color = color, super(repaint: value);

  final ValueNotifier<double> value;
  final Paint _fill;
  static final _slot = Paint()..color = AppColors.border;
  static final _thumb = Paint()..color = AppColors.textPrimary;
  static final _tick = Paint()
    ..color = AppColors.textSecondary
    ..strokeWidth = 1;

  static const _thumbH = 24.0;

  @override
  void paint(Canvas canvas, Size size) {
    final cx = size.width / 2;
    final top = _thumbH / 2;
    final h = size.height - _thumbH;
    final slot = RRect.fromRectAndRadius(Rect.fromLTWH(cx - 3, top, 6, h), const Radius.circular(3));
    canvas.drawRRect(slot, _slot);
    final y = top + h * (1 - value.value);
    canvas.drawRRect(
      RRect.fromRectAndRadius(Rect.fromLTRB(cx - 3, y, cx + 3, top + h), const Radius.circular(3)),
      _fill,
    );
    // Vạch 0 dB.
    final y0 = top + h * (1 - GainScale.toNorm(0));
    canvas.drawLine(Offset(cx - 12, y0), Offset(cx - 6, y0), _tick);
    canvas.drawRRect(
      RRect.fromRectAndRadius(
        Rect.fromCenter(center: Offset(cx, y), width: size.width - 8, height: _thumbH - 8),
        const Radius.circular(4),
      ),
      _thumb,
    );
  }

  @override
  bool shouldRepaint(_FaderPainter old) => old.value != value || old._fill.color != _fill.color;
}
