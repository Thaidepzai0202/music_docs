import 'package:flutter/foundation.dart';

/// Theo dõi `pointer → nốt` cho pad và bàn phím multi-touch (07 §3.2, P2-16/17).
///
/// - Pointer-down → NOTE_ON; up/cancel → NOTE_OFF; trượt sang phím khác → OFF phím cũ, ON phím mới.
/// - Đếm số ngón trên cùng 1 nốt: ON chỉ gửi khi 0→1, OFF chỉ khi 1→0 → hai ngón cùng phím
///   không làm nốt bị tắt sớm hay treo.
/// - [releaseAll] khi panel đóng / đổi track: không bao giờ để nốt treo.
class NotePlayer {
  NotePlayer({required this.noteOn, required this.noteOff});

  final void Function(int note, double velocity) noteOn;
  final void Function(int note) noteOff;

  final _pointerNote = <int, int>{};
  final _count = <int, int>{};

  /// Nốt đang giữ — painter nghe để tô phím (repaint, không rebuild).
  final held = ValueNotifier<Set<int>>(const {});

  void down(int pointer, int? note, double velocity) {
    if (note == null) return;
    up(pointer); // pointer id tái sử dụng: đóng nốt cũ trước
    _pointerNote[pointer] = note;
    _on(note, velocity);
    _publish();
  }

  /// [note] null = ngón trượt ra ngoài vùng phím → tắt nốt.
  void move(int pointer, int? note, double velocity) {
    if (!_pointerNote.containsKey(pointer) && note == null) return;
    final old = _pointerNote[pointer];
    if (old == note) return;
    if (old != null) _off(old);
    if (note == null) {
      _pointerNote.remove(pointer);
    } else {
      _pointerNote[pointer] = note;
      _on(note, velocity);
    }
    _publish();
  }

  void up(int pointer) {
    final n = _pointerNote.remove(pointer);
    if (n == null) return;
    _off(n);
    _publish();
  }

  void releaseAll() {
    for (final n in _pointerNote.values) {
      _off(n);
    }
    _pointerNote.clear();
    _publish();
  }

  int get activePointers => _pointerNote.length;

  void _on(int note, double v) {
    final c = _count[note] ?? 0;
    _count[note] = c + 1;
    if (c == 0) noteOn(note, v);
  }

  void _off(int note) {
    final c = _count[note] ?? 0;
    if (c <= 1) {
      _count.remove(note);
      noteOff(note);
    } else {
      _count[note] = c - 1;
    }
  }

  void _publish() => held.value = Set.unmodifiable(_count.keys);

  void dispose() {
    releaseAll();
    held.dispose();
  }
}
