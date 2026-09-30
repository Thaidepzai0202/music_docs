import 'dart:math' as math;

import '../../model/project.dart';

/// Phép sửa nốt của piano roll (07 §4.1b), thuần dữ liệu — widget chỉ gọi và commit kết quả.
/// Mọi nốt nằm trong [0, lengthBeats); độ dài tối thiểu 1 ô lưới; cao độ 0..127; velocity 1..127.
/// Thao tác nhóm nhận [group] là chỉ số trong danh sách nốt; nốt đơn = nhóm một phần tử.
abstract final class NoteEdit {
  /// Lưới chọn được (beat): 1/4 · 1/8 · 1/16 (mặc định) · 1/32.
  static const grids = {'1/4': 1.0, '1/8': 0.5, '1/16': 0.25, '1/32': 0.125};
  static const defaultGrid = 0.25;
  static const defaultVelocity = 100;
  static const _eps = 1e-9;

  /// Ô lưới chứa [beat] (làm tròn XUỐNG — chạm vào ô nào thì nốt bắt đầu ở ô đó).
  static double snapDown(double beat, double grid) => ((beat + _eps) / grid).floor() * grid;

  /// Gần nhất (khi kéo).
  static double snap(double beat, double grid) => (beat / grid).roundToDouble() * grid;

  /// Đầu nốt mới khi chạm ở [beat]: ô chứa điểm chạm, không để nốt bắt đầu sát cuối clip hơn 1 ô.
  static double drawStart(double beat, {required double grid, required double length}) =>
      snapDown(beat, grid).clamp(0.0, math.max(0.0, length - grid)).toDouble();

  /// Độ dài nốt đang vẽ bằng cách kéo sang phải: cuối nốt phủ hết ô dưới ngón ([endBeat]), ít nhất 1 ô, không vượt
  /// cuối clip.
  static double drawnLength(double start, double endBeat, {required double grid, required double length}) {
    final end = (snapDown(endBeat, grid) + grid).clamp(start + grid, math.max(start + grid, length));
    return end - start;
  }

  /// Nốt mới tại ô chứa [beat], dài [d] (mặc định 1 ô — widget truyền độ dài nốt vừa vẽ gần nhất), velocity 100.
  static List<Note> add(
    List<Note> notes, {
    required double beat,
    required int pitch,
    required double grid,
    required double length,
    double? d,
  }) {
    final s = drawStart(beat, grid: grid, length: length);
    final dd = (d ?? grid).clamp(math.min(grid, length - s), math.max(grid, length - s)).toDouble();
    return [...notes, Note(p: pitch.clamp(0, 127), v: defaultVelocity, s: s, d: dd)];
  }

  static List<Note> removeAt(List<Note> notes, int i) => [...notes]..removeAt(i);

  /// Dời nhóm theo ngón kéo nốt [anchor]: đầu nốt [anchor] snap theo lưới, cả nhóm lệch cùng một lượng; cao độ đi
  /// [dRows] hàng trên trục [rows] (cao độ theo hàng, trên → dưới: piano là bán cung, kit là pad). Cả nhóm dừng ở biên
  /// clip / biên trục mà không méo đội hình. Nốt có cao độ không có hàng thì giữ nguyên cao độ.
  static List<Note> moveGroup(
    List<Note> notes,
    Set<int> group, {
    required int anchor,
    required double dBeats,
    required int dRows,
    required List<int> rows,
    required double grid,
    required double length,
  }) {
    final a = notes[anchor];
    var minS = double.infinity;
    var maxEnd = double.negativeInfinity;
    for (final i in group) {
      minS = math.min(minS, notes[i].s);
      maxEnd = math.max(maxEnd, notes[i].s + notes[i].d);
    }
    final off = (snap(a.s + dBeats, grid) - a.s).clamp(-minS, math.max(-minS, length - maxEnd)).toDouble();
    final index = {for (var k = 0; k < rows.length; k++) rows[k]: k};
    var lo = -rows.length;
    var hi = rows.length;
    for (final i in group) {
      final k = index[notes[i].p];
      if (k == null) continue;
      lo = math.max(lo, -k);
      hi = math.min(hi, rows.length - 1 - k);
    }
    final dr = dRows.clamp(lo, math.max(lo, hi)).toInt();
    final out = [...notes];
    for (final i in group) {
      final n = notes[i];
      final k = index[n.p];
      out[i] = n.copyWith(s: n.s + off, p: k == null ? n.p : rows[k + dr]);
    }
    return out;
  }

  /// Kéo tay nắm (cuối nốt) của [anchor] tới [endBeat] (snap gần nhất): mọi nốt trong [group] đổi độ dài cùng một
  /// lượng. Mỗi nốt tối thiểu 1 ô (nốt vốn ngắn hơn 1 ô thì không bị kéo dài ra khi thu), không vượt cuối clip.
  static List<Note> resizeGroup(
    List<Note> notes,
    Set<int> group, {
    required int anchor,
    required double endBeat,
    required double grid,
    required double length,
  }) {
    final a = notes[anchor];
    final end = snap(endBeat, grid).clamp(a.s + grid, math.max(a.s + grid, length));
    final dd = end - (a.s + a.d);
    final out = [...notes];
    for (final i in group) {
      final n = notes[i];
      final lo = math.min(grid, n.d);
      out[i] = n.copyWith(d: (n.d + dd).clamp(lo, math.max(lo, length - n.s)).toDouble());
    }
    return out;
  }

  /// Chép nhóm, đặt ngay sau chính nó: độ lệch = độ dài đoạn nhóm chiếm (đầu nốt sớm nhất → cuối nốt muộn nhất),
  /// làm tròn LÊN theo lưới. Bản chép nối vào cuối danh sách; [copies] là chỉ số của chúng (để chọn tiếp, bấm nhân bản
  /// lần nữa thì chép tiếp về sau). [end] là cuối nốt muộn nhất sau khi chép: lớn hơn độ dài clip thì phải hỏi tăng.
  static ({List<Note> notes, Set<int> copies, double end}) duplicate(
    List<Note> notes,
    Set<int> group, {
    required double grid,
  }) {
    final order = group.toList()..sort();
    var minS = double.infinity;
    var maxEnd = double.negativeInfinity;
    for (final i in order) {
      minS = math.min(minS, notes[i].s);
      maxEnd = math.max(maxEnd, notes[i].s + notes[i].d);
    }
    final off = math.max(grid, ((maxEnd - minS - _eps) / grid).ceil() * grid);
    final out = [...notes, for (final i in order) notes[i].copyWith(s: notes[i].s + off)];
    return (notes: out, copies: {for (var k = notes.length; k < out.length; k++) k}, end: maxEnd + off);
  }

  /// Chỉ số các nốt BẮT ĐẦU trong đoạn [from, to), ở mọi cao độ (chọn theo đoạn thời gian trên thước bar).
  static Set<int> inRange(List<Note> notes, double from, double to) => {
    for (var i = 0; i < notes.length; i++)
      if (notes[i].s >= from - _eps && notes[i].s < to - _eps) i,
  };

  /// Cột của thanh velocity (07 §4.1b): các nốt bắt đầu cùng lúc (hợp âm) chung một cột; cột theo thời gian tăng dần.
  static List<List<int>> velocityColumns(List<Note> notes) {
    final byStart = <int, List<int>>{};
    for (var i = 0; i < notes.length; i++) {
      byStart.putIfAbsent((notes[i].s * 1e6).round(), () => []).add(i);
    }
    final keys = byStart.keys.toList()..sort();
    return [for (final k in keys) byStart[k]!];
  }

  /// Nốt đổi khi kéo cột [column]: cột có nốt đang chọn → mọi nốt đã chọn; không thì mọi nốt của cột (cả hợp âm).
  static Set<int> velocityTargets(List<int> column, Set<int> selected) =>
      column.any(selected.contains) ? selected : column.toSet();

  /// Kéo cột velocity của [anchor] tới [v]; các nốt khác trong [group] đổi theo cùng tỉ lệ. Kẹp 1..127.
  static List<Note> velocity(List<Note> notes, Set<int> group, {required int anchor, required int v}) {
    final a = notes[anchor];
    final target = v.clamp(1, 127);
    final ratio = target / a.v;
    final out = [...notes];
    for (final i in group) {
      if (i != anchor) out[i] = notes[i].copyWith(v: (notes[i].v * ratio).round().clamp(1, 127));
    }
    out[anchor] = a.copyWith(v: target);
    return out;
  }

  /// ⇥ Step (07 §4.1d): ghi hợp âm [chord] (cao độ → velocity) dài 1 ô tại con trỏ [at]. Nốt cùng cao độ bắt đầu đúng
  /// [at] được thay (bấm lại phím không nhân đôi nốt).
  static List<Note> stepChord(
    List<Note> notes,
    Map<int, int> chord, {
    required double at,
    required double grid,
    required double length,
  }) {
    final d = math.max(math.min(grid, length - at), _eps);
    return [
      for (final n in notes)
        if (!((n.s - at).abs() < _eps && chord.containsKey(n.p))) n,
      for (final e in chord.entries) Note(p: e.key.clamp(0, 127), v: e.value.clamp(1, 127), s: at, d: d),
    ];
  }

  /// Con trỏ Step tiến 1 ô; hết clip thì vòng về đầu (clip chạy lặp).
  static double stepNext(double at, {required double grid, required double length}) =>
      at + grid >= length - _eps ? 0 : at + grid;

  /// Con trỏ Step lùi 1 ô; đang ở đầu thì về ô cuối clip.
  static double stepPrev(double at, {required double grid, required double length}) {
    if (at - grid >= -_eps) return math.max(0.0, at - grid);
    return math.max(0.0, ((length - _eps) / grid).floor() * grid);
  }

  /// ⌫ của Step: xoá mọi nốt bắt đầu trong ô [at, at + grid), mọi cao độ.
  static List<Note> clearCell(List<Note> notes, {required double at, required double grid}) => [
    for (final n in notes)
      if (n.s < at - _eps || n.s >= at + grid - _eps) n,
  ];

  /// Đổi độ dài clip: bỏ nốt bắt đầu từ cuối clip mới trở đi, cắt nốt tràn qua cuối.
  static List<Note> fitLength(List<Note> notes, double length) => [
    for (final n in notes)
      if (n.s < length - _eps) n.s + n.d > length ? n.copyWith(d: length - n.s) : n,
  ];
}

/// Undo/redo sửa nốt theo từng clip, chỉ trong phiên (07 §4.1b): giữ 50 bước.
final class NoteHistory {
  static const maxSteps = 50;
  final _undo = <({List<Note> notes, double length})>[];
  final _redo = <({List<Note> notes, double length})>[];

  bool get canUndo => _undo.isNotEmpty;
  bool get canRedo => _redo.isNotEmpty;

  /// Ghi trạng thái TRƯỚC một thao tác. Thao tác mới xoá nhánh redo.
  void push(List<Note> notes, double length) {
    _undo.add((notes: List.unmodifiable(notes), length: length));
    if (_undo.length > maxSteps) _undo.removeAt(0);
    _redo.clear();
  }

  /// Trả trạng thái cần áp (null nếu hết); [current] vào nhánh redo.
  ({List<Note> notes, double length})? undo(List<Note> current, double length) {
    if (_undo.isEmpty) return null;
    _redo.add((notes: List.unmodifiable(current), length: length));
    return _undo.removeLast();
  }

  ({List<Note> notes, double length})? redo(List<Note> current, double length) {
    if (_redo.isEmpty) return null;
    _undo.add((notes: List.unmodifiable(current), length: length));
    return _redo.removeLast();
  }
}

/// Lịch sử theo clipId (sống cùng màn Session; không lưu vào file).
final class NoteHistories {
  final _byClip = <String, NoteHistory>{};
  NoteHistory of(String clipId) => _byClip.putIfAbsent(clipId, NoteHistory.new);
}
