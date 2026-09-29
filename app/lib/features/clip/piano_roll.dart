import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import '../../app/theme.dart';
import '../../engine/engine_state_ticker.dart';
import '../../model/project.dart';
import '../../ui_kit/keyboard_view.dart';
import 'note_edit.dart';

/// Một hàng của piano roll (trên → dưới): phím đàn, hoặc một pad của kit (07 §4.1b).
@immutable
class PianoRow {
  const PianoRow(this.pitch, this.label, {this.black = false});
  final int pitch;
  final String label;
  final bool black;
}

bool _isBlack(int p) => const {1, 3, 6, 8, 10}.contains(p % 12);

/// Hàng của piano roll. Kit ([pads] khác rỗng): mỗi pad một hàng (kèm cao độ nốt đang có mà không có pad, để không
/// giấu nốt), cao ở trên. Nhạc cụ: phím C1..C8, cuộn dọc.
List<PianoRow> pianoRollRows({Map<int, String>? pads, required List<Note> notes}) {
  if (pads != null && pads.isNotEmpty) {
    final keys = {...pads.keys, ...notes.map((n) => n.p)}.toList()..sort((a, b) => b.compareTo(a));
    return [for (final k in keys) PianoRow(k, pads[k] ?? noteName(k))];
  }
  return [for (var p = 108; p >= 24; p--) PianoRow(p, p % 12 == 0 ? noteName(p) : '', black: _isBlack(p))];
}

/// Hình học lưới: trục ngang = beat (0..lengthBeats trên [width] px, đã nhân zoom), trục dọc = [rows].
class PianoRollGeometry {
  PianoRollGeometry({required this.rows, required this.rowH, required this.lengthBeats, required this.width})
    : _index = {for (var i = 0; i < rows.length; i++) rows[i]: i};

  /// Cao độ theo hàng, trên → dưới.
  final List<int> rows;
  final double rowH;
  final double lengthBeats;
  final double width;
  final Map<int, int> _index;

  double get height => rows.length * rowH;
  double get pxPerBeat => width / lengthBeats;
  double xOf(double beat) => beat * pxPerBeat;
  double beatAt(double x) => (x / pxPerBeat).clamp(0.0, lengthBeats).toDouble();
  int rowAt(double y) => (y / rowH).floor().clamp(0, rows.length - 1);
  int pitchAt(double y) => rows[rowAt(y)];

  /// null nếu cao độ không có hàng (kit: phím không có pad).
  Rect? rectOf(Note n) {
    final i = _index[n.p];
    if (i == null) return null;
    final x = xOf(n.s);
    return Rect.fromLTWH(x, i * rowH, math.max(6.0, xOf(n.s + n.d) - x), rowH);
  }

  /// Nốt tại [p] (ưu tiên nốt vẽ sau cùng), null nếu không trúng.
  int? hit(List<Note> notes, Offset p) {
    for (var i = notes.length - 1; i >= 0; i--) {
      final r = rectOf(notes[i]);
      if (r != null && r.inflate(2).contains(p)) return i;
    }
    return null;
  }
}

enum PianoRollMode { draw, select }

/// Piano roll (07 §4.1b). Chế độ Vẽ: chạm ô trống → thêm nốt (snap theo [grid]); chạm nốt → xoá; kéo thân →
/// dời (thời gian + cao độ); kéo mép phải → đổi độ dài. Chế độ Chọn: chạm nốt để chọn (xoá / quantize ở thanh công
/// cụ). Mỗi thao tác xong (thả tay) mới [onCommit] — kéo liên tục không gửi engine.
///
/// Hiệu năng: lớp nốt chỉ vẽ lại khi nốt đổi (và lúc bắt đầu / kết thúc kéo); trong lúc kéo chỉ lớp nốt-đang-kéo vẽ
/// lại; playhead 60 Hz ở lớp riêng. Không widget nào rebuild theo playhead hay theo ngón kéo.
class PianoRollEditor extends StatefulWidget {
  const PianoRollEditor({
    super.key,
    required this.clip,
    required this.rows,
    required this.kit,
    required this.mode,
    required this.grid,
    required this.zoom,
    required this.selected,
    required this.onToggle,
    required this.onCommit,
    required this.onAudition,
    required this.ticker,
    required this.track,
    required this.color,
    required this.verticalController,
    this.beatsPerBar = 4,
  });

  final MidiClip clip;
  final List<PianoRow> rows;
  final bool kit;
  final PianoRollMode mode;
  final double grid;
  final int zoom;
  final Set<int> selected;
  final ValueChanged<int> onToggle;
  final ValueChanged<List<Note>> onCommit;
  final ValueChanged<int> onAudition;
  final EngineStateTicker ticker;
  final int track;
  final Color color;
  final ScrollController verticalController;
  final int beatsPerBar;

  static const keysWidth = 76.0;
  static const pianoRowH = 18.0;
  static const minKitRowH = 22.0;
  static const edgeGrab = 12.0;

  @override
  State<PianoRollEditor> createState() => _PianoRollEditorState();
}

class _PianoRollEditorState extends State<PianoRollEditor> {
  final _h = ScrollController();
  final _ghost = ValueNotifier<int?>(null);
  final _drag = ValueNotifier<Note?>(null);
  PianoRollGeometry? _geo;
  String? _centeredFor;

  int? _dragIndex;
  bool _resize = false;
  Offset _dragStart = Offset.zero;
  bool _moved = false;
  List<Note>? _pending;

  List<Note> get _notes => widget.clip.notes;

  @override
  void dispose() {
    _h.dispose();
    _ghost.dispose();
    _drag.dispose();
    super.dispose();
  }

  /// Nhạc cụ: lần đầu mở clip → cuộn dọc tới giữa các nốt (hoặc C4).
  void _centerOnce(double rowH, double viewport) {
    if (widget.kit || _centeredFor == widget.clip.id) return;
    _centeredFor = widget.clip.id;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final v = widget.verticalController;
      if (!mounted || !v.hasClients) return;
      final target = _notes.isEmpty ? 60 : (_notes.map((n) => n.p).reduce((a, b) => a + b) / _notes.length).round();
      final idx = widget.rows.indexWhere((r) => r.pitch == target);
      if (idx < 0) return;
      v.jumpTo((idx * rowH + rowH / 2 - viewport / 2).clamp(0.0, v.position.maxScrollExtent));
    });
  }

  void _tapUp(TapUpDetails d) {
    final geo = _geo;
    if (geo == null) return;
    final hit = geo.hit(_notes, d.localPosition);
    if (widget.mode == PianoRollMode.select) {
      if (hit != null) widget.onToggle(hit);
      return;
    }
    if (hit != null) return; // chạm nốt ở chế độ Vẽ do bộ nhận kéo xử lý (xoá)
    final pitch = geo.pitchAt(d.localPosition.dy);
    widget.onCommit(
      NoteEdit.add(
        _notes,
        beat: geo.beatAt(d.localPosition.dx),
        pitch: pitch,
        grid: widget.grid,
        length: widget.clip.lengthBeats,
      ),
    );
    widget.onAudition(pitch);
  }

  void _panStart(DragStartDetails d) {
    final geo = _geo;
    final i = geo?.hit(_notes, d.localPosition);
    if (geo == null || i == null) return;
    final r = geo.rectOf(_notes[i])!;
    _dragIndex = i;
    _resize = d.localPosition.dx > r.right - PianoRollEditor.edgeGrab;
    _dragStart = d.localPosition;
    _moved = false;
    _pending = null;
    _ghost.value = i;
    _drag.value = _notes[i];
  }

  void _panUpdate(DragUpdateDetails d) {
    final geo = _geo;
    final i = _dragIndex;
    if (geo == null || i == null) return;
    final delta = d.localPosition - _dragStart;
    if (delta.distance > 4) _moved = true;
    final len = widget.clip.lengthBeats;
    final next = _resize
        ? NoteEdit.resize(_notes, i, endBeat: geo.beatAt(d.localPosition.dx), grid: widget.grid, length: len)
        : NoteEdit.move(
            _notes,
            i,
            dBeats: delta.dx / geo.pxPerBeat,
            dPitch: geo.pitchAt(d.localPosition.dy) - _notes[i].p,
            grid: widget.grid,
            length: len,
          );
    _pending = next;
    _drag.value = next[i]; // chỉ lớp nốt-đang-kéo vẽ lại
  }

  void _panEnd(DragEndDetails _) {
    final i = _dragIndex;
    if (i == null) return;
    final pending = _pending;
    final moved = _moved;
    _endDrag();
    if (!moved) {
      widget.onCommit(NoteEdit.removeAt(_notes, i)); // chạm (không kéo) vào nốt ở chế độ Vẽ → xoá
    } else if (pending != null) {
      widget.onCommit(pending);
    }
  }

  void _endDrag() {
    _dragIndex = null;
    _pending = null;
    _ghost.value = null;
    _drag.value = null;
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        final rows = widget.rows;
        final rowH = widget.kit
            ? math.max(PianoRollEditor.minKitRowH, c.maxHeight / math.max(1, rows.length))
            : PianoRollEditor.pianoRowH;
        final gridW = math.max(1.0, (c.maxWidth - PianoRollEditor.keysWidth) * widget.zoom);
        final geo = _geo = PianoRollGeometry(
          rows: [for (final r in rows) r.pitch],
          rowH: rowH,
          lengthBeats: widget.clip.lengthBeats,
          width: gridW,
        );
        _centerOnce(rowH, c.maxHeight);
        final size = Size(gridW, geo.height);
        final grid = SizedBox(
          key: const Key('pianoRoll'),
          width: gridW,
          height: geo.height,
          child: Stack(
            children: [
              RepaintBoundary(
                child: CustomPaint(
                  size: size,
                  painter: PianoRollPainter(
                    geo,
                    _notes,
                    widget.selected,
                    widget.color,
                    grid: widget.grid,
                    beatsPerBar: widget.beatsPerBar,
                    blackRows: {
                      for (final r in rows)
                        if (r.black) r.pitch,
                    },
                    ghost: _ghost,
                  ),
                ),
              ),
              RepaintBoundary(
                child: CustomPaint(size: size, painter: NoteDragPainter(geo, _drag, widget.color)),
              ),
              // Playhead 60Hz ở lớp riêng — không làm vẽ lại lớp nốt (07 §6.3).
              RepaintBoundary(
                child: CustomPaint(
                  size: size,
                  painter: ClipPlayheadPainter(ticker: widget.ticker, track: widget.track, slot: widget.clip.slot),
                ),
              ),
            ],
          ),
        );
        final gestures = <Type, GestureRecognizerFactory>{
          TapGestureRecognizer: GestureRecognizerFactoryWithHandlers<TapGestureRecognizer>(
            TapGestureRecognizer.new,
            (r) => r.onTapUp = _tapUp,
          ),
          if (widget.mode == PianoRollMode.draw)
            _NoteDragRecognizer: GestureRecognizerFactoryWithHandlers<_NoteDragRecognizer>(
              () => _NoteDragRecognizer((p) => _geo?.hit(_notes, p) != null),
              (r) => r
                ..onStart = _panStart
                ..onUpdate = _panUpdate
                ..onEnd = _panEnd
                ..onCancel = _endDrag,
            ),
        };
        return SingleChildScrollView(
          controller: widget.verticalController,
          physics: widget.kit && geo.height <= c.maxHeight ? const NeverScrollableScrollPhysics() : null,
          child: SizedBox(
            height: geo.height,
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                SizedBox(
                  width: PianoRollEditor.keysWidth,
                  height: geo.height,
                  child: RepaintBoundary(
                    child: CustomPaint(
                      painter: PianoKeysPainter(rows, rowH, kit: widget.kit, font: painterFont(context)),
                    ),
                  ),
                ),
                Expanded(
                  child: SingleChildScrollView(
                    controller: _h,
                    scrollDirection: Axis.horizontal,
                    physics: widget.zoom == 1 ? const NeverScrollableScrollPhysics() : null,
                    child: RawGestureDetector(behavior: HitTestBehavior.opaque, gestures: gestures, child: grid),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }
}

/// Kéo nốt: chỉ nhận ngón bắt đầu TRÊN một nốt, và thắng ngay (trước bộ cuộn) — ngón bắt đầu ở ô trống thì để
/// cuộn / chạm thêm nốt.
class _NoteDragRecognizer extends PanGestureRecognizer {
  _NoteDragRecognizer(this.hitsNote);

  final bool Function(Offset local) hitsNote;

  @override
  bool isPointerAllowed(PointerEvent event) => hitsNote(event.localPosition) && super.isPointerAllowed(event);

  @override
  void addAllowedPointer(PointerDownEvent event) {
    super.addAllowedPointer(event);
    resolvePointer(event.pointer, GestureDisposition.accepted);
  }
}

/// Lưới + nốt. Vẽ lại khi nốt / lựa chọn / lưới đổi, và khi [ghost] (nốt đang kéo) đổi — không theo từng frame kéo.
class PianoRollPainter extends CustomPainter {
  PianoRollPainter(
    this.geo,
    this.notes,
    this.selected,
    Color color, {
    required this.grid,
    required this.beatsPerBar,
    required this.blackRows,
    required this.ghost,
  }) : _note = Paint()..color = color,
       _noteGhost = Paint()..color = color.withValues(alpha: 0.3),
       _noteSel = Paint()..color = Color.lerp(color, Colors.white, 0.55)!,
       super(repaint: ghost);

  final PianoRollGeometry geo;
  final List<Note> notes;
  final Set<int> selected;
  final double grid;
  final int beatsPerBar;
  final Set<int> blackRows;
  final ValueListenable<int?> ghost;
  final Paint _note;
  final Paint _noteGhost;
  final Paint _noteSel;
  static final _bg = Paint()..color = AppColors.background;
  static final _black = Paint()..color = const Color(0xFF14161A);
  static final _sub = Paint()
    ..color = AppColors.border.withValues(alpha: 0.45)
    ..strokeWidth = 1;
  static final _beat = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;
  static final _bar = Paint()
    ..color = AppColors.textSecondary.withValues(alpha: 0.5)
    ..strokeWidth = 1;
  static final _rowLine = Paint()
    ..color = AppColors.border.withValues(alpha: 0.35)
    ..strokeWidth = 1;
  static final _outline = Paint()
    ..color = Colors.white
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, _bg);
    for (var i = 0; i < geo.rows.length; i++) {
      final y = i * geo.rowH;
      if (blackRows.contains(geo.rows[i])) canvas.drawRect(Rect.fromLTWH(0, y, size.width, geo.rowH), _black);
      canvas.drawLine(Offset(0, y), Offset(size.width, y), _rowLine);
    }
    // Vạch lưới (chỉ khi đủ thưa để nhìn được), beat, bar.
    final drawSub = geo.xOf(grid) >= 6;
    final steps = (geo.lengthBeats / grid).round();
    for (var k = 0; k <= steps; k++) {
      final b = k * grid;
      final onBeat = (b - b.roundToDouble()).abs() < 1e-9;
      if (!onBeat && !drawSub) continue;
      final x = geo.xOf(b);
      final paint = !onBeat ? _sub : (b.round() % beatsPerBar == 0 ? _bar : _beat);
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), paint);
    }
    final g = ghost.value;
    for (var i = 0; i < notes.length; i++) {
      final rect = geo.rectOf(notes[i]);
      if (rect == null) continue;
      final r = RRect.fromRectAndRadius(rect.deflate(1), const Radius.circular(2));
      if (i == g) {
        canvas.drawRRect(r, _noteGhost); // vị trí cũ, mờ — lớp kéo vẽ vị trí mới
        continue;
      }
      final sel = selected.contains(i);
      canvas.drawRRect(r, sel ? _noteSel : _note);
      if (sel) canvas.drawRRect(r, _outline);
    }
  }

  @override
  bool shouldRepaint(PianoRollPainter old) =>
      !identical(old.notes, notes) ||
      old.geo.width != geo.width ||
      old.geo.rowH != geo.rowH ||
      !listEquals(old.geo.rows, geo.rows) ||
      old.geo.lengthBeats != geo.lengthBeats ||
      old.grid != grid ||
      !setEquals(old.selected, selected) ||
      old._note.color != _note.color ||
      old.ghost != ghost;
}

/// Lớp nốt đang kéo: vẽ lại mỗi frame kéo, chỉ một nốt.
class NoteDragPainter extends CustomPainter {
  NoteDragPainter(this.geo, this.drag, Color color)
    : _fill = Paint()..color = Color.lerp(color, Colors.white, 0.35)!,
      super(repaint: drag);

  final PianoRollGeometry geo;
  final ValueListenable<Note?> drag;
  final Paint _fill;
  static final _outline = Paint()
    ..color = Colors.white
    ..style = PaintingStyle.stroke
    ..strokeWidth = 1.5;

  @override
  void paint(Canvas canvas, Size size) {
    final n = drag.value;
    final rect = n == null ? null : geo.rectOf(n);
    if (rect == null) return;
    final r = RRect.fromRectAndRadius(rect.deflate(1), const Radius.circular(2));
    canvas.drawRRect(r, _fill);
    canvas.drawRRect(r, _outline);
  }

  @override
  bool shouldRepaint(NoteDragPainter old) => old.drag != drag || old.geo.width != geo.width || old.geo.rowH != geo.rowH;
}

/// Cột trái: phím đàn (nhãn ở C) hoặc tên pad của kit.
class PianoKeysPainter extends CustomPainter {
  PianoKeysPainter(this.rows, this.rowH, {required this.kit, TextStyle font = const TextStyle()})
    : _labels = [
        for (final r in rows)
          r.label.isEmpty
              ? null
              : (TextPainter(
                  text: TextSpan(
                    text: r.label,
                    style: font.merge(
                      TextStyle(fontSize: kit ? 11 : 10, color: kit ? AppColors.textPrimary : const Color(0xFF5F6368)),
                    ),
                  ),
                  maxLines: 1,
                  ellipsis: '…',
                  textDirection: TextDirection.ltr,
                )..layout(maxWidth: PianoRollEditor.keysWidth - 8)),
      ];

  final List<PianoRow> rows;
  final double rowH;
  final bool kit;
  final List<TextPainter?> _labels;
  static final _white = Paint()..color = const Color(0xFFE8EAED);
  static final _black = Paint()..color = const Color(0xFF1C1E22);
  static final _pad = Paint()..color = AppColors.surface;
  static final _line = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;

  @override
  void paint(Canvas canvas, Size size) {
    for (var i = 0; i < rows.length; i++) {
      final r = Rect.fromLTWH(0, i * rowH, size.width, rowH);
      canvas.drawRect(r, kit ? _pad : (rows[i].black ? _black : _white));
      canvas.drawLine(r.bottomLeft, r.bottomRight, _line);
      final tp = _labels[i];
      if (tp != null) tp.paint(canvas, Offset(4, r.top + (rowH - tp.height) / 2));
    }
  }

  @override
  bool shouldRepaint(PianoKeysPainter old) => old.rows != rows || old.rowH != rowH || old.kit != kit;
}

/// Vạch playhead của clip đang phát trên track (dùng chung cho piano roll và waveform).
class ClipPlayheadPainter extends CustomPainter {
  ClipPlayheadPainter({required this.ticker, required this.track, required this.slot}) : super(repaint: ticker);

  final EngineStateTicker ticker;
  final int track;
  final int slot;
  static final _line = Paint()
    ..color = AppColors.play
    ..strokeWidth = 2;

  @override
  void paint(Canvas canvas, Size size) {
    final s = ticker.state;
    if (s.trackPlayingSlot[track] != slot) return;
    final x = s.trackClipProgress[track].clamp(0.0, 1.0) * size.width;
    canvas.drawLine(Offset(x, 0), Offset(x, size.height), _line);
  }

  @override
  bool shouldRepaint(ClipPlayheadPainter old) => old.ticker != ticker || old.track != track || old.slot != slot;
}
