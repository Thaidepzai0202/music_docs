import 'dart:math' as math;

import 'package:flutter/widgets.dart';

import '../app/theme.dart';

/// Meter peak nhiều kênh (P2-15), thang dB -60..0: xanh tới -6 dB, vàng tới -1 dB, đỏ trên -1 dB.
///
/// Vẽ lại mỗi khi [repaint] báo (thường là `EngineStateTicker`), trong RepaintBoundary riêng;
/// không rebuild widget (07 §6.1). [level] đọc giá trị tuyến tính 0..1 của kênh `ch` ngay lúc vẽ.
class LevelMeter extends StatelessWidget {
  const LevelMeter({
    super.key,
    required this.repaint,
    required this.level,
    this.channels = 2,
    this.axis = Axis.vertical,
  });

  final Listenable repaint;
  final double Function(int ch) level;
  final int channels;
  final Axis axis;

  @override
  Widget build(BuildContext context) {
    return RepaintBoundary(
      child: CustomPaint(
        painter: MeterPainter(repaint: repaint, level: level, channels: channels, axis: axis),
        child: const SizedBox.expand(),
      ),
    );
  }
}

class MeterPainter extends CustomPainter {
  MeterPainter({required Listenable repaint, required this.level, required this.channels, required this.axis})
    : _repaint = repaint,
      super(repaint: repaint);

  final Listenable _repaint;
  final double Function(int ch) level;
  final int channels;
  final Axis axis;

  static final _slot = Paint()..color = AppColors.border;
  static final _green = Paint()..color = AppColors.play;
  static final _yellow = Paint()..color = AppColors.queued;
  static final _red = Paint()..color = AppColors.record;
  static final _n6 = norm(0.5012); // -6 dB
  static final _n1 = norm(0.8913); // -1 dB

  /// Peak tuyến tính → 0..1 trên thang -60..0 dB.
  static double norm(double linear) {
    if (linear <= 0.001) return 0;
    return ((20 * math.log(linear) / math.ln10 + 60) / 60).clamp(0.0, 1.0);
  }

  @override
  void paint(Canvas canvas, Size size) {
    const gap = 2.0;
    final vertical = axis == Axis.vertical;
    final lane = ((vertical ? size.width : size.height) - gap * (channels - 1)) / channels;
    final len = vertical ? size.height : size.width;
    for (var ch = 0; ch < channels; ch++) {
      final o = ch * (lane + gap);
      Rect seg(double a, double b) => vertical
          ? Rect.fromLTRB(o, len * (1 - b), o + lane, len * (1 - a))
          : Rect.fromLTRB(len * a, o, len * b, o + lane);
      canvas.drawRect(seg(0, 1), _slot);
      final v = norm(level(ch));
      if (v <= 0) continue;
      canvas.drawRect(seg(0, math.min(v, _n6)), _green);
      if (v > _n6) canvas.drawRect(seg(_n6, math.min(v, _n1)), _yellow);
      if (v > _n1) canvas.drawRect(seg(_n1, v), _red);
    }
  }

  @override
  bool shouldRepaint(MeterPainter old) => old._repaint != _repaint || old.channels != channels || old.axis != axis;
}
