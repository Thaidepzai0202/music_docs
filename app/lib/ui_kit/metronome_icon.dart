import 'package:flutter/widgets.dart';

/// Icon metronome vẽ bằng Path (Material Icons không có). Thay ký tự "♩": ký tự phụ thuộc font hệ thống có glyph
/// hay không; icon vẽ thì chắc chắn hiện, cả trên iPad lẫn golden test.
class MetronomeIcon extends StatelessWidget {
  const MetronomeIcon({super.key, this.size = 14, this.color});

  final double size;

  /// Mặc định: màu chữ hiện tại (DefaultTextStyle).
  final Color? color;

  @override
  Widget build(BuildContext context) => SizedBox.square(
    dimension: size,
    child: CustomPaint(
      painter: _MetronomePainter(color ?? DefaultTextStyle.of(context).style.color ?? const Color(0xFFE8EAED)),
    ),
  );
}

class _MetronomePainter extends CustomPainter {
  _MetronomePainter(this.color)
    : _stroke = Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeJoin = StrokeJoin.round
        ..strokeCap = StrokeCap.round,
      _fill = Paint()..color = color;

  final Color color;
  final Paint _stroke;
  final Paint _fill;
  final _body = Path();
  double _s = -1;

  @override
  void paint(Canvas canvas, Size size) {
    final s = size.width;
    if (s != _s) {
      // Dựng lại hình chỉ khi kích thước đổi (07 §6.5: không tạo Paint/Path trong paint()).
      _s = s;
      _stroke.strokeWidth = s * 0.09;
      _body
        ..reset()
        ..moveTo(s * 0.40, s * 0.08)
        ..lineTo(s * 0.60, s * 0.08)
        ..lineTo(s * 0.80, s * 0.92)
        ..lineTo(s * 0.20, s * 0.92)
        ..close();
    }
    canvas.drawPath(_body, _stroke); // thân hình thang + đế
    canvas.drawLine(Offset(s * 0.50, s * 0.74), Offset(s * 0.76, s * 0.22), _stroke); // con lắc
    canvas.drawCircle(Offset(s * 0.66, s * 0.42), s * 0.07, _fill); // quả nặng
  }

  @override
  bool shouldRepaint(_MetronomePainter old) => old.color != color;
}
