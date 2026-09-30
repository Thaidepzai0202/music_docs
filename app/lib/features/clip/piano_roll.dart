import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import '../../app/theme.dart';
import '../../engine/engine_state_ticker.dart';
import '../../l10n/l10n.dart';
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

  /// Vùng bắt tay nắm ở cuối nốt (07 §4.1b: rộng 32pt).
  static const handleWidth = 32.0;

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

  /// Vùng tay nắm: rộng [handleWidth], sát cuối nốt. Nốt ngắn thì chừa nửa đầu (tối đa 16pt) cho thân để còn kéo dời
  /// được, phần tay nắm còn lại lòi ra ngoài cuối nốt.
  Rect? handleOf(Note n) {
    final r = rectOf(n);
    if (r == null) return null;
    final left = math.max(r.left + math.min(r.width / 2, handleWidth / 2), r.right - handleWidth);
    return Rect.fromLTWH(left, r.top, handleWidth, r.height);
  }

  /// Nốt dưới điểm [p]. Thân nốt được ưu tiên (nốt vẽ sau cùng trước); điểm nằm trong vùng tay nắm của chính nốt đó
  /// → [handle]. Ngoài mọi thân nốt → tay nắm lòi ra của một nốt ngắn ([outside]).
  ({int index, bool handle, bool outside})? target(List<Note> notes, Offset p) {
    for (var i = notes.length - 1; i >= 0; i--) {
      final r = rectOf(notes[i]);
      if (r != null && r.inflate(2).contains(p)) {
        return (index: i, handle: handleOf(notes[i])!.contains(p), outside: false);
      }
    }
    for (var i = notes.length - 1; i >= 0; i--) {
      final h = handleOf(notes[i]);
      if (h != null && h.inflate(2).contains(p)) return (index: i, handle: true, outside: true);
    }
    return null;
  }

  /// Nốt có hình chạm khung [box] (kéo khung ở chế độ Chọn).
  Set<int> notesIn(List<Note> notes, Rect box) => {
    for (var i = 0; i < notes.length; i++)
      if (rectOf(notes[i])?.overlaps(box) ?? false) i,
  };
}

enum PianoRollMode { draw, select }

/// Một thao tác sửa nốt xong (thả tay): danh sách nốt mới + lựa chọn sau thao tác.
typedef NotesCommit = void Function(List<Note> notes, Set<int> selection);

/// Nội dung lớp kéo: nốt ở vị trí mới (dời / đổi độ dài / đang vẽ), khung chọn và các nốt đang nằm trong khung.
@immutable
class DragOverlay {
  const DragOverlay({this.notes = const [], this.box, this.outlined = const []});
  static const none = DragOverlay();
  final List<Note> notes;
  final Rect? box;
  final List<Note> outlined;
}

/// Velocity nháp khi kéo cột trên thanh velocity: chỉ số nốt → giá trị.
typedef VelocityDraft = Map<int, int>;

/// Piano roll (07 §4.1b, bản 29/09). Lưới nhận thao tác 1 ngón, 2 ngón để cuộn:
/// - Chế độ Vẽ: chạm ô trống → nốt dài bằng nốt vừa vẽ gần nhất (mặc định 1 ô); chạm, giữ rồi kéo sang phải → nốt dài
///   theo ngón. Chạm nốt → xoá. Kéo thân → dời; kéo tay nắm (cuối nốt, 32pt) → đổi độ dài.
/// - Chế độ Chọn: chạm nốt → chọn / bỏ chọn; chạm nền trống → bỏ chọn; kéo nền trống → khung chọn. Kéo một nốt đã chọn
///   → dời cả nhóm tự do (thời gian + cao độ, snap); kéo tay nắm của nốt đã chọn → cả nhóm đổi độ dài cùng một lượng.
/// - Thước bar ở trên: kéo → chọn mọi nốt bắt đầu trong đoạn. Thanh velocity ở dưới: kéo cột lên / xuống; nốt đang
///   chọn thì cả nhóm đổi theo cùng tỉ lệ.
/// Mỗi thao tác xong (thả tay) mới [onCommit] — kéo liên tục không gửi engine.
///
/// Hiệu năng: lớp nốt chỉ vẽ lại khi nốt / lựa chọn đổi và lúc bắt đầu / kết thúc kéo (bản mờ); trong lúc kéo chỉ lớp
/// kéo vẽ lại; kéo velocity chỉ vẽ lại thanh velocity; playhead 60 Hz ở lớp riêng. Không widget nào rebuild theo ngón
/// kéo hay playhead.
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
    required this.onSelect,
    required this.onCommit,
    required this.onAudition,
    required this.ticker,
    required this.track,
    required this.color,
    required this.verticalController,
    this.beatsPerBar = 4,
    this.stepCursor,
    this.onStepCursor,
    this.liveNotes,
  });

  final MidiClip clip;
  final List<PianoRow> rows;
  final bool kit;
  final PianoRollMode mode;
  final double grid;
  final int zoom;
  final Set<int> selected;

  /// Chạm một nốt ở chế độ Chọn: bật / tắt chọn nốt đó (widget cha giữ lựa chọn mới nhất).
  final ValueChanged<int> onToggle;

  /// Thay cả lựa chọn (khung, thước, chạm nền trống).
  final ValueChanged<Set<int>> onSelect;
  final NotesCommit onCommit;
  final ValueChanged<int> onAudition;
  final EngineStateTicker ticker;
  final int track;
  final Color color;
  final ScrollController verticalController;
  final int beatsPerBar;

  /// ⇥ Step (07 §4.1d): con trỏ (beat) vẽ trên lớp kéo; khác null thì chạm lưới = đặt con trỏ ([onStepCursor]).
  final ValueListenable<double>? stepCursor;
  final ValueChanged<double>? onStepCursor;

  /// Nốt đang ghi chồng (LE_EVT_CLIP_CHANGED): khác null thì lớp nốt vẽ theo nó — chỉ vẽ lại, không rebuild.
  final ValueListenable<List<Note>?>? liveNotes;

  static const keysWidth = 76.0;
  static const pianoRowH = 18.0;
  static const minKitRowH = 22.0;
  static const rulerHeight = 20.0;

  /// Ngón đi quá ngưỡng này mới tính là kéo; dưới ngưỡng là chạm.
  static const dragSlop = 8.0;

  /// Thanh velocity: ~22% chiều cao piano roll, 32..72pt (panel thường còn chỗ cho lưới, ⤢ thì cao hơn).
  static double velocityLaneHeight(double maxHeight) => (maxHeight * 0.22).clamp(32.0, 72.0);

  @override
  State<PianoRollEditor> createState() => _PianoRollEditorState();
}

enum _Op { draw, body, handle, empty }

class _PianoRollEditorState extends State<PianoRollEditor> {
  final _h = ScrollController();

  /// Nốt đang kéo: lớp nốt vẽ chúng mờ ở vị trí cũ. Chỉ đổi lúc bắt đầu / kết thúc kéo.
  final _ghost = ValueNotifier<Set<int>>(const {});
  final _overlay = ValueNotifier<DragOverlay>(DragOverlay.none);

  /// Đoạn thời gian đang kéo trên thước (beat).
  final _range = ValueNotifier<(double, double)?>(null);
  final _velocity = ValueNotifier<VelocityDraft?>(null);
  PianoRollGeometry? _geo;
  double _laneH = 48;
  String? _centeredFor;

  /// Độ dài nốt vừa vẽ gần nhất (chạm ô trống → nốt dài bằng nó); null = 1 ô. Đổi lưới → về 1 ô.
  double? _lastLength;

  // Con trỏ đang chạm lưới (toạ độ màn hình): 1 ngón = thao tác, từ 2 ngón = cuộn.
  final _pointers = <int, Offset>{};
  bool _scrolling = false;
  Offset _focal = Offset.zero;

  // Thao tác 1 ngón đang diễn ra.
  _Op? _op;
  Offset _start = Offset.zero;
  bool _moved = false;
  bool _outside = false;
  int _anchor = -1;
  Set<int> _group = const {};
  Note? _drawing;
  List<Note>? _pending;
  Set<int> _boxed = const {};

  double _rangeFrom = 0;
  List<int> _velColumn = const [];
  double _velDy = 0;

  List<Note> get _notes => widget.clip.notes;
  bool get _selectMode => widget.mode == PianoRollMode.select;

  @override
  void didUpdateWidget(PianoRollEditor old) {
    super.didUpdateWidget(old);
    if (old.grid != widget.grid) _lastLength = null;
    // Nốt đổi từ ngoài (undo, thanh công cụ) giữa lúc kéo → chỉ số không còn đúng: bỏ thao tác dở.
    if (!listEquals(old.clip.notes, widget.clip.notes) || old.mode != widget.mode) {
      _clearOp();
      _velColumn = const [];
      _velocity.value = null;
      _range.value = null;
    }
  }

  @override
  void dispose() {
    _h.dispose();
    _ghost.dispose();
    _overlay.dispose();
    _range.dispose();
    _velocity.dispose();
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

  // ---- Con trỏ trên lưới ----

  void _pointerDown(PointerDownEvent e) {
    if (e.kind == PointerDeviceKind.mouse && e.buttons != kPrimaryMouseButton) return;
    _pointers[e.pointer] = e.position;
    if (_pointers.length == 1 && !_scrolling) {
      _begin(e.localPosition);
    } else {
      _clearOp(); // ngón thứ hai: bỏ thao tác dở (chưa gửi gì) → cuộn bằng 2 ngón
      _scrolling = true;
      _focal = _centroid();
    }
  }

  void _pointerMove(PointerMoveEvent e) {
    if (!_pointers.containsKey(e.pointer)) return;
    _pointers[e.pointer] = e.position;
    if (_scrolling) {
      final c = _centroid();
      _scrollBy(_focal - c);
      _focal = c;
    } else {
      _update(e.localPosition);
    }
  }

  void _pointerUp(PointerUpEvent e) {
    if (_pointers.remove(e.pointer) == null) return;
    if (_scrolling) {
      if (_pointers.isEmpty) {
        _scrolling = false;
      } else {
        _focal = _centroid();
      }
    } else {
      _end();
    }
  }

  void _pointerCancel(PointerCancelEvent e) {
    if (_pointers.remove(e.pointer) == null) return;
    if (_pointers.isEmpty) _scrolling = false;
    _clearOp();
  }

  void _pointerSignal(PointerSignalEvent e) {
    if (e is PointerScrollEvent) _scrollBy(e.scrollDelta);
  }

  Offset _centroid() => _pointers.values.reduce((a, b) => a + b) / _pointers.length.toDouble();

  /// Cuộn thêm [d] (dương = nội dung đi lên / sang trái), kẹp trong biên.
  void _scrollBy(Offset d) {
    void by(ScrollController c, double delta) {
      if (delta == 0 || !c.hasClients) return;
      final p = c.position;
      c.jumpTo((p.pixels + delta).clamp(p.minScrollExtent, p.maxScrollExtent));
    }

    by(_h, d.dx);
    by(widget.verticalController, d.dy);
  }

  void _begin(Offset p) {
    final geo = _geo;
    if (geo == null) return;
    _start = p;
    _moved = false;
    _pending = null;
    final t = geo.target(_notes, p);
    if (t == null) {
      if (_selectMode) {
        _op = _Op.empty;
      } else {
        _op = _Op.draw;
        _drawing = _newNote(geo, p);
        _overlay.value = DragOverlay(notes: [_drawing!]);
      }
      return;
    }
    _op = t.handle ? _Op.handle : _Op.body;
    _outside = t.outside;
    _anchor = t.index;
    _group = _selectMode && widget.selected.contains(t.index) ? widget.selected : {t.index};
  }

  Note _newNote(PianoRollGeometry geo, Offset p) => NoteEdit.add(
    const [],
    beat: geo.beatAt(p.dx),
    pitch: geo.pitchAt(p.dy),
    grid: widget.grid,
    length: widget.clip.lengthBeats,
    d: _lastLength,
  ).single;

  void _update(Offset p) {
    final geo = _geo;
    final op = _op;
    if (geo == null || op == null) return;
    final delta = p - _start;
    if (!_moved) {
      if (delta.distance < PianoRollEditor.dragSlop) return;
      _moved = true;
      if (op == _Op.body || op == _Op.handle) _ghost.value = _group; // bản mờ ở vị trí cũ: lớp nốt vẽ lại một lần
    }
    final len = widget.clip.lengthBeats;
    switch (op) {
      case _Op.draw:
        final n = _drawing!;
        _drawing = n.copyWith(
          d: NoteEdit.drawnLength(n.s, geo.beatAt(p.dx), grid: widget.grid, length: len),
        );
        _overlay.value = DragOverlay(notes: [_drawing!]);
      case _Op.body:
        final next = NoteEdit.moveGroup(
          _notes,
          _group,
          anchor: _anchor,
          dBeats: delta.dx / geo.pxPerBeat,
          dRows: geo.rowAt(p.dy) - geo.rowAt(_start.dy),
          rows: geo.rows,
          grid: widget.grid,
          length: len,
        );
        _pending = next;
        _overlay.value = DragOverlay(notes: [for (final i in _group) next[i]]);
      case _Op.handle:
        final a = _notes[_anchor];
        final next = NoteEdit.resizeGroup(
          _notes,
          _group,
          anchor: _anchor,
          endBeat: a.s + a.d + delta.dx / geo.pxPerBeat,
          grid: widget.grid,
          length: len,
        );
        _pending = next;
        _overlay.value = DragOverlay(notes: [for (final i in _group) next[i]]);
      case _Op.empty:
        final box = Rect.fromPoints(_start, p);
        _boxed = geo.notesIn(_notes, box);
        _overlay.value = DragOverlay(box: box, outlined: [for (final i in _boxed) _notes[i]]);
    }
  }

  void _end() {
    final op = _op;
    final geo = _geo;
    if (op == null || geo == null) return;
    final (moved, anchor, group, pending, drawing, boxed, outside) = (
      _moved,
      _anchor,
      _group,
      _pending,
      _drawing,
      _boxed,
      _outside,
    );
    _clearOp();
    final onStep = widget.onStepCursor;
    if (widget.stepCursor != null && onStep != null && !moved) {
      final len = widget.clip.lengthBeats;
      onStep(NoteEdit.snapDown(geo.beatAt(_start.dx), widget.grid).clamp(0.0, len - widget.grid).toDouble());
      return; // Step: chạm lưới chỉ đặt con trỏ
    }
    switch (op) {
      case _Op.draw:
        _addNote(drawing!);
      case _Op.empty:
        widget.onSelect(moved ? boxed : const {});
      case _Op.handle when !moved && outside && !_selectMode:
        _addNote(_newNote(geo, _start)); // chạm ô trống ngay sau nốt ngắn (vùng tay nắm lòi ra) → vẽ nốt mới
      case _Op.body || _Op.handle when !moved:
        if (_selectMode) {
          widget.onToggle(anchor);
        } else {
          widget.onCommit(NoteEdit.removeAt(_notes, anchor), const {});
        }
      case _Op.body || _Op.handle:
        if (pending != null && !listEquals(pending, _notes)) {
          widget.onCommit(pending, _selectMode ? group : const {});
        }
    }
  }

  void _addNote(Note n) {
    _lastLength = n.d;
    widget.onCommit([..._notes, n], const {});
    widget.onAudition(n.p);
  }

  void _clearOp() {
    _op = null;
    _drawing = null;
    _pending = null;
    _boxed = const {};
    _group = const {};
    if (_ghost.value.isNotEmpty) _ghost.value = const {};
    _overlay.value = DragOverlay.none;
  }

  // ---- Thước bar: kéo → chọn theo đoạn thời gian ----

  double _contentX(double localDx) => localDx + (_h.hasClients ? _h.offset : 0);

  void _rulerStart(DragStartDetails d) {
    final geo = _geo;
    if (geo == null) return;
    _rangeFrom = geo.beatAt(_contentX(d.localPosition.dx));
    _rulerMove(d.localPosition.dx);
  }

  void _rulerMove(double localDx) {
    final geo = _geo;
    if (geo == null) return;
    final b = geo.beatAt(_contentX(localDx));
    final g = widget.grid;
    final len = widget.clip.lengthBeats;
    final from = NoteEdit.snapDown(math.min(_rangeFrom, b), g).clamp(0.0, len).toDouble();
    final to = (NoteEdit.snapDown(math.max(_rangeFrom, b), g) + g).clamp(0.0, len).toDouble();
    _range.value = (from, to);
  }

  void _rulerEnd() {
    final r = _range.value;
    _range.value = null;
    if (r != null) widget.onSelect(NoteEdit.inRange(_notes, r.$1, r.$2));
  }

  // ---- Thanh velocity ----

  void _velDown(DragDownDetails d) {
    _velDy = 0;
    _velColumn = VelocityLanePainter.hit(
      _geo,
      _notes,
      widget.selected,
      Offset(_contentX(d.localPosition.dx), d.localPosition.dy),
      _laneH,
    );
  }

  void _velUpdate(DragUpdateDetails d) {
    final col = _velColumn;
    if (col.isEmpty || col.any((i) => i >= _notes.length)) return;
    _velDy += d.delta.dy;
    final targets = NoteEdit.velocityTargets(col, widget.selected);
    // Mốc tỉ lệ: nốt to nhất của cột trong nhóm đổi (thân cột vẽ theo velocity lớn nhất).
    final anchor = col.where(targets.contains).reduce((a, b) => _notes[b].v > _notes[a].v ? b : a);
    final v = (_notes[anchor].v - _velDy * 127 / VelocityLanePainter.usable(_laneH)).round();
    final next = NoteEdit.velocity(_notes, targets, anchor: anchor, v: v);
    _velocity.value = {for (final i in targets) i: next[i].v};
  }

  void _velEnd(DragEndDetails _) {
    final draft = _velocity.value;
    _velColumn = const [];
    _velocity.value = null;
    if (draft == null) return;
    final next = [for (var i = 0; i < _notes.length; i++) _notes[i].copyWith(v: draft[i] ?? _notes[i].v)];
    if (!listEquals(next, _notes)) widget.onCommit(next, widget.selected);
  }

  void _velCancel() {
    _velColumn = const [];
    _velocity.value = null;
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        final rows = widget.rows;
        final laneH = _laneH = PianoRollEditor.velocityLaneHeight(c.maxHeight);
        final viewH = math.max(0.0, c.maxHeight - PianoRollEditor.rulerHeight - laneH);
        final rowH = widget.kit
            ? math.max(PianoRollEditor.minKitRowH, viewH / math.max(1, rows.length))
            : PianoRollEditor.pianoRowH;
        final gridW = math.max(1.0, (c.maxWidth - PianoRollEditor.keysWidth) * widget.zoom);
        final geo = _geo = PianoRollGeometry(
          rows: [for (final r in rows) r.pitch],
          rowH: rowH,
          lengthBeats: widget.clip.lengthBeats,
          width: gridW,
        );
        _centerOnce(rowH, viewH);
        final size = Size(gridW, geo.height);
        final font = painterFont(context);
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
                    live: widget.liveNotes,
                  ),
                ),
              ),
              RepaintBoundary(
                child: CustomPaint(
                  size: size,
                  painter: NoteDragPainter(
                    geo,
                    _overlay,
                    _range,
                    widget.color,
                    cursor: widget.stepCursor,
                    grid: widget.grid,
                  ),
                ),
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
        const never = NeverScrollableScrollPhysics(); // cuộn bằng 2 ngón (Listener) để 1 ngón vẽ / kéo khung
        return Column(
          children: [
            SizedBox(
              height: PianoRollEditor.rulerHeight,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  SizedBox(
                    width: PianoRollEditor.keysWidth,
                    child: Tooltip(
                      message: S.clipCuonHaiNgon,
                      child: const Icon(Icons.swipe, size: 14, color: AppColors.textSecondary),
                    ),
                  ),
                  Expanded(
                    child: GestureDetector(
                      key: const Key('pianoRoll.ruler'),
                      behavior: HitTestBehavior.opaque,
                      dragStartBehavior: DragStartBehavior.down,
                      onHorizontalDragStart: _rulerStart,
                      onHorizontalDragUpdate: (d) => _rulerMove(d.localPosition.dx),
                      onHorizontalDragEnd: (_) => _rulerEnd(),
                      onHorizontalDragCancel: () => _range.value = null,
                      child: RepaintBoundary(
                        child: CustomPaint(
                          painter: RulerPainter(geo, h: _h, range: _range, beatsPerBar: widget.beatsPerBar, font: font),
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
            Expanded(
              child: SingleChildScrollView(
                controller: widget.verticalController,
                physics: never,
                child: SizedBox(
                  height: geo.height,
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      GestureDetector(
                        onVerticalDragUpdate: (d) => _scrollBy(Offset(0, -d.delta.dy)),
                        child: SizedBox(
                          width: PianoRollEditor.keysWidth,
                          height: geo.height,
                          child: RepaintBoundary(
                            child: CustomPaint(
                              painter: PianoKeysPainter(rows, rowH, kit: widget.kit, font: font),
                            ),
                          ),
                        ),
                      ),
                      Expanded(
                        child: SingleChildScrollView(
                          controller: _h,
                          scrollDirection: Axis.horizontal,
                          physics: never,
                          child: Listener(
                            behavior: HitTestBehavior.opaque,
                            onPointerDown: _pointerDown,
                            onPointerMove: _pointerMove,
                            onPointerUp: _pointerUp,
                            onPointerCancel: _pointerCancel,
                            onPointerSignal: _pointerSignal,
                            child: grid,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
            SizedBox(
              height: laneH,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  SizedBox(
                    width: PianoRollEditor.keysWidth,
                    child: Padding(
                      padding: const EdgeInsets.only(left: 6),
                      child: Align(
                        alignment: Alignment.centerLeft,
                        child: Text(
                          S.clipVelocity,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: Theme.of(context).textTheme.bodySmall,
                        ),
                      ),
                    ),
                  ),
                  Expanded(
                    child: GestureDetector(
                      key: const Key('pianoRoll.velocity'),
                      behavior: HitTestBehavior.opaque,
                      dragStartBehavior: DragStartBehavior.down,
                      onVerticalDragDown: _velDown,
                      onVerticalDragUpdate: _velUpdate,
                      onVerticalDragEnd: _velEnd,
                      onVerticalDragCancel: _velCancel,
                      child: RepaintBoundary(
                        child: CustomPaint(
                          painter: VelocityLanePainter(
                            geo,
                            _notes,
                            widget.selected,
                            widget.color,
                            h: _h,
                            draft: _velocity,
                            font: font,
                          ),
                        ),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ],
        );
      },
    );
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
    this.live,
  }) : _note = Paint()..color = color,
       _noteGhost = Paint()..color = color.withValues(alpha: 0.3),
       _noteSel = Paint()..color = Color.lerp(color, Colors.white, 0.55)!,
       super(repaint: Listenable.merge([ghost, ?live]));

  final PianoRollGeometry geo;
  final List<Note> notes;
  final Set<int> selected;
  final double grid;
  final int beatsPerBar;
  final Set<int> blackRows;
  final ValueListenable<Set<int>> ghost;

  /// Nốt đang ghi chồng (thay [notes] khi khác null; bỏ tô chọn / bản mờ vì chỉ số không khớp).
  final ValueListenable<List<Note>?>? live;
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
  static final _grip = Paint()
    ..color = const Color(0x8C000000)
    ..strokeWidth = 2;
  static final _gripOut = Paint()..color = const Color(0x66FFFFFF);

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
    final liveNotes = live?.value;
    final notes = liveNotes ?? this.notes;
    final g = liveNotes == null ? ghost.value : const <int>{};
    final selected = liveNotes == null ? this.selected : const <int>{};
    for (var i = 0; i < notes.length; i++) {
      final rect = geo.rectOf(notes[i]);
      if (rect == null) continue;
      final r = RRect.fromRectAndRadius(rect.deflate(1), const Radius.circular(2));
      if (g.contains(i)) {
        canvas.drawRRect(r, _noteGhost); // vị trí cũ, mờ — lớp kéo vẽ vị trí mới
        continue;
      }
      final sel = selected.contains(i);
      canvas.drawRRect(r, sel ? _noteSel : _note);
      if (sel) canvas.drawRRect(r, _outline);
      // Tay nắm ở cuối nốt; nốt đã chọn mà ngắn thì vẽ cả phần tay nắm lòi ra.
      if (rect.width >= 10) {
        canvas.drawLine(Offset(rect.right - 4, rect.top + 4), Offset(rect.right - 4, rect.bottom - 4), _grip);
      }
      final h = geo.handleOf(notes[i])!;
      if (sel && h.right > rect.right + 1) {
        canvas.drawRRect(
          RRect.fromLTRBR(rect.right, rect.center.dy - 3, h.right - 6, rect.center.dy + 3, const Radius.circular(3)),
          _gripOut,
        );
      }
    }
  }

  @override
  bool shouldRepaint(PianoRollPainter old) =>
      !listEquals(old.notes, notes) ||
      old.geo.width != geo.width ||
      old.geo.rowH != geo.rowH ||
      !listEquals(old.geo.rows, geo.rows) ||
      old.geo.lengthBeats != geo.lengthBeats ||
      old.grid != grid ||
      old.beatsPerBar != beatsPerBar ||
      !setEquals(old.selected, selected) ||
      old._note.color != _note.color ||
      old.ghost != ghost ||
      old.live != live;
}

/// Lớp kéo: nốt ở vị trí mới / nốt đang vẽ, khung chọn (kèm viền các nốt trong khung), đoạn đang chọn trên thước và
/// con trỏ ⇥ Step. Vẽ lại mỗi frame kéo / mỗi lần con trỏ tiến — chỉ lớp này.
class NoteDragPainter extends CustomPainter {
  NoteDragPainter(this.geo, this.overlay, this.range, Color color, {this.cursor, this.grid = NoteEdit.defaultGrid})
    : _fill = Paint()..color = Color.lerp(color, Colors.white, 0.35)!,
      _band = Paint()..color = color.withValues(alpha: 0.14),
      super(repaint: Listenable.merge([overlay, range, ?cursor]));

  final PianoRollGeometry geo;
  final ValueListenable<DragOverlay> overlay;
  final ValueListenable<(double, double)?> range;
  final ValueListenable<double>? cursor;
  final double grid;
  final Paint _fill;
  final Paint _band;
  static final _outline = Paint()
    ..color = Colors.white
    ..style = PaintingStyle.stroke
    ..strokeWidth = 1.5;
  static final _boxFill = Paint()..color = const Color(0x1FFFFFFF);
  static final _boxLine = Paint()
    ..color = const Color(0xCCFFFFFF)
    ..style = PaintingStyle.stroke
    ..strokeWidth = 1;
  static final _cursorCell = Paint()..color = AppColors.queued.withValues(alpha: 0.16);
  static final _cursorLine = Paint()
    ..color = AppColors.queued
    ..strokeWidth = 2;

  @override
  void paint(Canvas canvas, Size size) {
    final c = cursor?.value;
    if (c != null) {
      final x = geo.xOf(c);
      canvas.drawRect(Rect.fromLTRB(x, 0, geo.xOf(c + grid), size.height), _cursorCell);
      canvas.drawLine(Offset(x, 0), Offset(x, size.height), _cursorLine);
    }
    final r = range.value;
    if (r != null) canvas.drawRect(Rect.fromLTRB(geo.xOf(r.$1), 0, geo.xOf(r.$2), size.height), _band);
    final o = overlay.value;
    for (final n in o.outlined) {
      final rect = geo.rectOf(n);
      if (rect != null) canvas.drawRRect(RRect.fromRectAndRadius(rect.deflate(1), const Radius.circular(2)), _outline);
    }
    final box = o.box;
    if (box != null) {
      canvas.drawRect(box, _boxFill);
      canvas.drawRect(box, _boxLine);
    }
    for (final n in o.notes) {
      final rect = geo.rectOf(n);
      if (rect == null) continue;
      final rr = RRect.fromRectAndRadius(rect.deflate(1), const Radius.circular(2));
      canvas.drawRRect(rr, _fill);
      canvas.drawRRect(rr, _outline);
    }
  }

  @override
  bool shouldRepaint(NoteDragPainter old) =>
      old.cursor != cursor ||
      old.grid != grid ||
      old.overlay != overlay ||
      old.range != range ||
      old.geo.width != geo.width ||
      old.geo.rowH != geo.rowH ||
      old._fill.color != _fill.color;
}

/// Thước bar trên lưới (số bar + vạch beat), dịch theo cuộn ngang [h]; đoạn đang kéo tô màu. Vẽ lại khi cuộn ngang
/// hoặc khi kéo trên thước.
class RulerPainter extends CustomPainter {
  RulerPainter(
    this.geo, {
    required this.h,
    required this.range,
    required this.beatsPerBar,
    TextStyle font = const TextStyle(),
  }) : _labels = [
         for (var b = 0; b * beatsPerBar < geo.lengthBeats - 1e-9; b++)
           TextPainter(
             text: TextSpan(
               text: (b + 1).toString(),
               style: font.merge(const TextStyle(fontSize: 10, color: AppColors.textSecondary)),
             ),
             textDirection: TextDirection.ltr,
           )..layout(),
       ],
       super(repaint: Listenable.merge([h, range]));

  final PianoRollGeometry geo;
  final ScrollController h;
  final ValueListenable<(double, double)?> range;
  final int beatsPerBar;
  final List<TextPainter> _labels;
  static final _bg = Paint()..color = AppColors.surface;
  static final _band = Paint()..color = AppColors.queued.withValues(alpha: 0.45);
  static final _tick = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;
  static final _barTick = Paint()
    ..color = AppColors.textSecondary
    ..strokeWidth = 1;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, _bg);
    canvas.save();
    canvas.clipRect(Offset.zero & size);
    canvas.translate(h.hasClients ? -h.offset : 0, 0);
    final r = range.value;
    if (r != null) canvas.drawRect(Rect.fromLTRB(geo.xOf(r.$1), 0, geo.xOf(r.$2), size.height), _band);
    for (var k = 0; k <= geo.lengthBeats.floor(); k++) {
      final x = geo.xOf(k.toDouble());
      final bar = k % beatsPerBar == 0;
      canvas.drawLine(Offset(x, bar ? 0 : size.height * 0.6), Offset(x, size.height), bar ? _barTick : _tick);
      final i = k ~/ beatsPerBar;
      if (bar && i < _labels.length) _labels[i].paint(canvas, Offset(x + 3, (size.height - _labels[i].height) / 2));
    }
    canvas.restore();
    canvas.drawLine(Offset(0, size.height - 0.5), Offset(size.width, size.height - 0.5), _tick);
  }

  @override
  bool shouldRepaint(RulerPainter old) =>
      old.h != h ||
      old.range != range ||
      old.beatsPerBar != beatsPerBar ||
      old.geo.width != geo.width ||
      old.geo.lengthBeats != geo.lengthBeats;
}

/// Thanh velocity dưới piano roll: mỗi thời điểm bắt đầu một cột ở đầu nốt (hợp âm chung cột, nhãn "×N"), cao theo
/// velocity lớn nhất 1..127; cột có nốt đang chọn sáng hơn và vẽ sau cùng. Dịch theo cuộn ngang [h]; khi kéo cột thì
/// [draft] đè giá trị — chỉ lớp này vẽ lại.
class VelocityLanePainter extends CustomPainter {
  VelocityLanePainter(
    this.geo,
    this.notes,
    this.selected,
    Color color, {
    required this.h,
    required this.draft,
    TextStyle font = const TextStyle(),
  }) : columns = NoteEdit.velocityColumns(notes),
       _stem = Paint()
         ..color = color.withValues(alpha: 0.7)
         ..strokeWidth = 2,
       _stemSel = Paint()
         ..color = Color.lerp(color, Colors.white, 0.55)!
         ..strokeWidth = 3,
       _countStyle = font.merge(const TextStyle(fontSize: 9, color: AppColors.textSecondary)),
       super(repaint: Listenable.merge([h, draft])) {
    _counts = [
      for (final col in columns)
        col.length < 2
            ? null
            : (TextPainter(
                text: TextSpan(text: '×${col.length}', style: _countStyle),
                textDirection: TextDirection.ltr,
              )..layout()),
    ];
  }

  final PianoRollGeometry geo;
  final List<Note> notes;
  final Set<int> selected;

  /// Chỉ số nốt theo cột (NoteEdit.velocityColumns).
  final List<List<int>> columns;
  final TextStyle _countStyle;
  late final List<TextPainter?> _counts;
  final ScrollController h;
  final ValueListenable<VelocityDraft?> draft;
  final Paint _stem;
  final Paint _stemSel;
  static final _bg = Paint()..color = const Color(0xFF15171B);
  static final _line = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;

  /// Cột lệch vào trong đầu nốt một chút (khỏi dính vạch lưới).
  static const stemInset = 3.0;
  static const _pad = 4.0;

  /// Chiều cao ứng với velocity 0..127.
  static double usable(double laneH) => math.max(1.0, laneH - 2 * _pad);

  static double _topOf(int v, double laneH) => laneH - _pad - usable(laneH) * v / 127;

  /// Cột velocity dưới [p] (toạ độ nội dung, đã cộng cuộn ngang): trong ±14pt quanh cột, ưu tiên cột có nốt đang chọn
  /// rồi cột gần nhất. Trả chỉ số các nốt của cột; rỗng nếu không trúng.
  static List<int> hit(PianoRollGeometry? geo, List<Note> notes, Set<int> selected, Offset p, double laneH) {
    if (geo == null) return const [];
    var best = const <int>[];
    var bestScore = double.infinity;
    for (final col in NoteEdit.velocityColumns(notes)) {
      final dx = (geo.xOf(notes[col.first].s) + stemInset - p.dx).abs();
      if (dx > 14) continue;
      final score = dx - (col.any(selected.contains) ? 20 : 0);
      if (score < bestScore) {
        bestScore = score;
        best = col;
      }
    }
    return best;
  }

  /// Velocity hiện ở cột: lớn nhất trong cột (nháp đè nếu đang kéo).
  static int shown(List<Note> notes, List<int> column, [VelocityDraft? draft]) {
    var v = 0;
    for (final i in column) {
      v = math.max(v, draft?[i] ?? notes[i].v);
    }
    return v;
  }

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, _bg);
    canvas.drawLine(Offset.zero, Offset(size.width, 0), _line);
    canvas.save();
    canvas.clipRect(Offset.zero & size);
    canvas.translate(h.hasClients ? -h.offset : 0, 0);
    final d = draft.value;
    final base = size.height - _pad;
    for (final pass in const [false, true]) {
      for (var c = 0; c < columns.length; c++) {
        final col = columns[c];
        if (col.any(selected.contains) != pass) continue;
        final x = geo.xOf(notes[col.first].s) + stemInset;
        final top = _topOf(shown(notes, col, d), size.height);
        final paint = pass ? _stemSel : _stem;
        canvas.drawLine(Offset(x, base), Offset(x, top), paint);
        canvas.drawCircle(Offset(x, top), 3, paint);
        final count = _counts[c];
        if (count != null) {
          final y = (top - count.height / 2).clamp(0.0, math.max(0.0, size.height - count.height)).toDouble();
          count.paint(canvas, Offset(x + 5, y));
        }
      }
    }
    canvas.restore();
  }

  @override
  bool shouldRepaint(VelocityLanePainter old) =>
      !listEquals(old.notes, notes) ||
      !setEquals(old.selected, selected) ||
      old.h != h ||
      old.draft != draft ||
      old.geo.width != geo.width ||
      old._stem.color != _stem.color;
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
