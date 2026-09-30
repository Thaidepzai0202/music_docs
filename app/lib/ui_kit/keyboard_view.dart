import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';

import '../app/theme.dart';

/// Hình học bàn phím [octaves] quãng tám bắt đầu từ [baseNote] (nên là nốt C). Dùng cho cả vẽ
/// và hit-test, để một `Listener` duy nhất xử lý mọi ngón (07 §3.2: không GestureDetector từng phím).
class KeyboardLayout {
  const KeyboardLayout({required this.baseNote, this.octaves = 2});

  final int baseNote;
  final int octaves;

  static const _whiteOffsets = [0, 2, 4, 5, 7, 9, 11];

  /// Phím đen nằm bên phải phím trắng thứ i trong quãng (null = không có: sau E và B).
  static const _blackAfter = [1, 3, null, 6, 8, 10, null];
  static const blackHeightRatio = 0.62;
  static const blackWidthRatio = 0.6;

  int get whiteCount => 7 * octaves;

  int whiteNote(int i) => baseNote + 12 * (i ~/ 7) + _whiteOffsets[i % 7];

  /// Nốt tại [p] trong vùng [size], null nếu ngoài bàn phím.
  int? noteAt(Offset p, Size size) {
    if (p.dx < 0 || p.dy < 0 || p.dx >= size.width || p.dy >= size.height) return null;
    final w = size.width / whiteCount;
    if (p.dy < size.height * blackHeightRatio) {
      final bw = w * blackWidthRatio;
      final i = (p.dx / w).floor();
      // Phím đen giữa phím trắng i và i+1 (bên phải) hoặc i-1 và i (bên trái).
      for (final k in [i, i - 1]) {
        if (k < 0 || k >= whiteCount - 1) continue;
        final off = _blackAfter[k % 7];
        if (off == null) continue;
        final cx = (k + 1) * w;
        if ((p.dx - cx).abs() <= bw / 2) return baseNote + 12 * (k ~/ 7) + off;
      }
    }
    return whiteNote((p.dx / w).floor().clamp(0, whiteCount - 1));
  }

  /// Velocity theo vị trí dọc trên phím: chạm càng thấp càng mạnh (0.35..1).
  static double velocityAt(Offset p, Size size) => (0.35 + 0.65 * (p.dy / size.height)).clamp(0.35, 1.0);

  Iterable<({int note, Rect rect})> blackKeys(Size size) sync* {
    final w = size.width / whiteCount;
    final bw = w * blackWidthRatio;
    for (var k = 0; k < whiteCount - 1; k++) {
      final off = _blackAfter[k % 7];
      if (off == null) continue;
      yield (
        note: baseNote + 12 * (k ~/ 7) + off,
        rect: Rect.fromLTWH((k + 1) * w - bw / 2, 0, bw, size.height * blackHeightRatio),
      );
    }
  }
}

String noteName(int note) {
  const names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  return '${names[note % 12]}${note ~/ 12 - 1}';
}

/// Vẽ bàn phím; phím đang giữ tô màu [accent]. Repaint theo [held], không rebuild.
class KeyboardPainter extends CustomPainter {
  KeyboardPainter({
    required this.layout,
    required this.held,
    required Color accent,
    this.range,
    this.font = const TextStyle(),
  }) : _accent = Paint()..color = accent,
       _mark = Paint()..color = accent.withValues(alpha: 0.85),
       super(repaint: held);

  final KeyboardLayout layout;

  /// Dải phím tự nhiên của nhạc cụ (manifest `range`): phím trong dải có vạch accent mảnh ở đầu phím, phím ngoài dải
  /// nhạt hơn (vẫn bấm được, vẫn kêu). null = không đánh dấu.
  final (int, int)? range;

  /// Font của theme (`painterFont`) cho nhãn nốt.
  final TextStyle font;
  final ValueListenable<Set<int>> held;
  final Paint _accent;
  final Paint _mark;
  static final _white = Paint()..color = const Color(0xFFE8EAED);
  static final _black = Paint()..color = const Color(0xFF1C1E22);
  static final _whiteOut = Paint()..color = const Color(0xFFB4B8BF);
  static final _blackOut = Paint()..color = const Color(0xFF3C4047);
  static const markHeight = 3.0;

  bool inRange(int note) => range == null || (note >= range!.$1 && note <= range!.$2);
  static final _line = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;
  final _labels = <int, TextPainter>{};

  @override
  void paint(Canvas canvas, Size size) {
    final h = held.value;
    final w = size.width / layout.whiteCount;
    for (var i = 0; i < layout.whiteCount; i++) {
      final n = layout.whiteNote(i);
      final r = Rect.fromLTWH(i * w, 0, w, size.height);
      final inside = inRange(n);
      canvas.drawRect(r.deflate(1), h.contains(n) ? _accent : (inside ? _white : _whiteOut));
      if (range != null && inside) canvas.drawRect(Rect.fromLTWH(r.left + 1, 1, w - 2, markHeight), _mark);
      canvas.drawLine(r.topRight, r.bottomRight, _line);
      if (n % 12 == 0) {
        final tp = _labels.putIfAbsent(
          n,
          () => TextPainter(
            text: TextSpan(
              text: noteName(n),
              style: font.merge(const TextStyle(color: Color(0xFF5F6368), fontSize: 11)),
            ),
            textDirection: TextDirection.ltr,
          )..layout(),
        );
        tp.paint(canvas, Offset(r.left + (w - tp.width) / 2, size.height - tp.height - 6));
      }
    }
    for (final k in layout.blackKeys(size)) {
      final inside = inRange(k.note);
      canvas.drawRect(k.rect, h.contains(k.note) ? _accent : (inside ? _black : _blackOut));
      if (range != null && inside) {
        canvas.drawRect(Rect.fromLTWH(k.rect.left + 1, k.rect.top, k.rect.width - 2, markHeight), _mark);
      }
    }
  }

  @override
  bool shouldRepaint(KeyboardPainter old) =>
      old.layout.baseNote != layout.baseNote ||
      old.held != held ||
      old.range != range ||
      old._accent.color != _accent.color;
}

/// Lưới pad 4×4 kiểu MPC: pad dưới-trái = [baseNote], tăng dần trái→phải, dưới→trên.
class PadLayout {
  const PadLayout({this.baseNote = 36, this.rows = 4, this.cols = 4});

  final int baseNote;
  final int rows;
  final int cols;

  int noteOf(int row, int col) => baseNote + (rows - 1 - row) * cols + col;

  int? noteAt(Offset p, Size size) {
    if (p.dx < 0 || p.dy < 0 || p.dx >= size.width || p.dy >= size.height) return null;
    final col = (p.dx / (size.width / cols)).floor();
    final row = (p.dy / (size.height / rows)).floor();
    return noteOf(row, col);
  }

  Rect rectOf(int row, int col, Size size) {
    final w = size.width / cols, h = size.height / rows;
    return Rect.fromLTWH(col * w, row * h, w, h).deflate(4);
  }
}

class PadPainter extends CustomPainter {
  PadPainter({
    required this.layout,
    required this.held,
    required Color accent,
    this.names = const {},
    this.font = const TextStyle(),
  }) : _accent = Paint()..color = accent,
       _idle = Paint()..color = accent.withValues(alpha: 0.25),
       super(repaint: held);

  final PadLayout layout;

  /// Tên pad theo nốt (06 §4: `region_label` của kit), hiện dưới số nốt, tối đa 2 dòng.
  final Map<int, String> names;

  /// Font của theme (`painterFont`) cho số nốt.
  final TextStyle font;
  final ValueListenable<Set<int>> held;
  final Paint _accent;
  final Paint _idle;
  final _labels = <int, TextPainter>{};
  final _names = <int, TextPainter>{};
  double _namesWidth = -1;

  @override
  void paint(Canvas canvas, Size size) {
    final h = held.value;
    for (var r = 0; r < layout.rows; r++) {
      for (var c = 0; c < layout.cols; c++) {
        final n = layout.noteOf(r, c);
        final rect = layout.rectOf(r, c, size);
        canvas.drawRRect(RRect.fromRectAndRadius(rect, const Radius.circular(6)), h.contains(n) ? _accent : _idle);
        final tp = _labels.putIfAbsent(
          n,
          () => TextPainter(
            text: TextSpan(
              text: '$n',
              style: font.merge(const TextStyle(color: AppColors.textPrimary, fontSize: 11)),
            ),
            textDirection: TextDirection.ltr,
          )..layout(),
        );
        tp.paint(canvas, rect.topLeft + const Offset(6, 4));
        _nameOf(n, rect.width - 12)?.paint(canvas, rect.topLeft + Offset(6, 6 + tp.height));
      }
    }
  }

  /// Chữ tên pad, chỉ dựng lại khi bề rộng pad đổi (không theo từng lần vẽ lúc bấm pad).
  TextPainter? _nameOf(int note, double maxWidth) {
    final text = names[note];
    if (text == null || maxWidth <= 0) return null;
    if (maxWidth != _namesWidth) {
      _names.clear();
      _namesWidth = maxWidth;
    }
    return _names.putIfAbsent(
      note,
      () => TextPainter(
        text: TextSpan(
          text: text,
          style: font.merge(const TextStyle(color: AppColors.textPrimary, fontSize: 10, height: 1.15)),
        ),
        maxLines: 2,
        ellipsis: '…',
        textDirection: TextDirection.ltr,
      )..layout(maxWidth: maxWidth),
    );
  }

  @override
  bool shouldRepaint(PadPainter old) =>
      old.held != held || old._accent.color != _accent.color || !mapEquals(old.names, names);
}
