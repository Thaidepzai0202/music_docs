import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../model/project.dart';

/// Nốt của clip MIDI đang được ghi chồng (07 §4.1d, `LE_EVT_CLIP_CHANGED`): controller đọc `clip.getMidi` rồi đặt vào
/// notifier của ô; lớp nốt của piano roll nghe notifier → chỉ vẽ lại, không rebuild. Model chỉ cập nhật một lần khi
/// RECORDING_FINISHED (cả lượt ghi là một bước ↶), lúc đó notifier về null.
class LiveClipNotes {
  final _byCell = <int, ValueNotifier<List<Note>?>>{};

  ValueNotifier<List<Note>?> of(int track, int slot) =>
      _byCell.putIfAbsent(track * 1000 + slot, () => ValueNotifier<List<Note>?>(null));

  void clear(int track, int slot) => _byCell[track * 1000 + slot]?.value = null;

  void dispose() {
    for (final n in _byCell.values) {
      n.dispose();
    }
    _byCell.clear();
  }
}

final liveClipNotesProvider = Provider<LiveClipNotes>((ref) {
  final l = LiveClipNotes();
  ref.onDispose(l.dispose);
  return l;
});
