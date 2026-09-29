import 'dart:math' as math;

import '../../model/project.dart';

/// Phép sửa nốt của piano roll (07 §4.1b), thuần dữ liệu — widget chỉ gọi và commit kết quả.
/// Mọi nốt nằm trong [0, lengthBeats); độ dài tối thiểu 1 ô lưới; cao độ 0..127.
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

  /// Nốt mới dài 1 ô lưới, velocity 100, tại ô chứa [beat].
  static List<Note> add(
    List<Note> notes, {
    required double beat,
    required int pitch,
    required double grid,
    required double length,
  }) {
    final s = snapDown(beat, grid).clamp(0.0, math.max(0.0, length - grid)).toDouble();
    return [...notes, Note(p: pitch.clamp(0, 127), v: defaultVelocity, s: s, d: grid)];
  }

  static List<Note> removeAt(List<Note> notes, int i) => [...notes]..removeAt(i);

  /// Dời nốt [i] thêm [dBeats] (snap theo lưới) và [dPitch] bán cung; giữ độ dài, giữ trong clip.
  static List<Note> move(
    List<Note> notes,
    int i, {
    required double dBeats,
    required int dPitch,
    required double grid,
    required double length,
  }) {
    final n = notes[i];
    final s = snap(n.s + dBeats, grid).clamp(0.0, math.max(0.0, length - math.min(n.d, length))).toDouble();
    final out = [...notes];
    out[i] = n.copyWith(s: s, p: (n.p + dPitch).clamp(0, 127));
    return out;
  }

  /// Kéo mép phải nốt [i] tới [endBeat] (snap), tối thiểu 1 ô lưới, không vượt cuối clip.
  static List<Note> resize(
    List<Note> notes,
    int i, {
    required double endBeat,
    required double grid,
    required double length,
  }) {
    final n = notes[i];
    final end = snap(endBeat, grid).clamp(n.s + grid, math.max(n.s + grid, length)).toDouble();
    final out = [...notes];
    out[i] = n.copyWith(d: end - n.s);
    return out;
  }

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
