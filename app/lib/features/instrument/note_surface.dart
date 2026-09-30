import 'package:flutter/widgets.dart';

import 'note_player.dart';

/// Mặt chơi pad / bàn phím (07 §3.2): 1 Listener cho mọi ngón, hit-test bằng hình học (không widget từng phím).
/// Painter nghe [NotePlayer.held] → chỉ vẽ lại, 0 rebuild khi chơi. Dùng ở tab Instrument và tab Clip (07 §4.1d).
class NoteSurface extends StatelessWidget {
  const NoteSurface({
    super.key,
    required this.player,
    required this.noteAt,
    required this.velocityAt,
    required this.painter,
  });

  final NotePlayer player;
  final int? Function(Offset, Size) noteAt;
  final double Function(Offset, Size) velocityAt;
  final CustomPainter painter;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        final size = c.biggest;
        return Listener(
          behavior: HitTestBehavior.opaque,
          onPointerDown: (e) =>
              player.down(e.pointer, noteAt(e.localPosition, size), velocityAt(e.localPosition, size)),
          onPointerMove: (e) =>
              player.move(e.pointer, noteAt(e.localPosition, size), velocityAt(e.localPosition, size)),
          onPointerUp: (e) => player.up(e.pointer),
          onPointerCancel: (e) => player.up(e.pointer),
          child: RepaintBoundary(
            child: CustomPaint(size: size, painter: painter),
          ),
        );
      },
    );
  }
}
