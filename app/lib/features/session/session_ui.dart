import '../clip/note_edit.dart';
import '../clip/piano_roll.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/painting.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../engine/performance_actions.dart';
import '../../model/project.dart';

/// Hai chế độ của grid (07 §3.1). Perform: pointer-down launch. Edit: chạm chọn, nhấn giữ mở menu.
enum SessionMode { perform, edit }

enum PanelTab { clip, instrument, mixer, fx, browser }

@immutable
final class CellRef {
  const CellRef(this.track, this.slot);
  final int track;
  final int slot;

  @override
  bool operator ==(Object other) => other is CellRef && other.track == track && other.slot == slot;
  @override
  int get hashCode => Object.hash(track, slot);
  @override
  String toString() => 'CellRef($track, $slot)';
}

/// State giao diện của màn Session — không lưu vào project.
@immutable
final class SessionUi {
  const SessionUi({
    this.mode = SessionMode.perform,
    this.selected,
    this.selectedTrack = 0,
    this.clipboard,
    this.tab = PanelTab.mixer,
    this.dragTarget,
    this.pianoRollMode = PianoRollMode.select,
    this.panelExpanded = false,
    this.grid = NoteEdit.defaultGrid,
    this.clipKeyboard = false,
    this.stepMode = false,
  });

  final SessionMode mode;

  /// Chế độ piano roll (07 §4.1b): ⬚ Chọn (mặc định) / ✏️ Vẽ. Tạo clip MIDI trống → Vẽ.
  final PianoRollMode pianoRollMode;

  /// ⤢ ở tab Clip: panel cao ~70% màn hình để vẽ nốt / sửa clip (grid co lại, cuộn nếu hàng < 44 pt).
  /// Đổi tab hoặc thu gọn panel → về như cũ.
  final bool panelExpanded;

  /// Lưới piano roll (07 §4.1b), dùng chung cho vẽ nốt và ⇥ Step.
  final double grid;

  /// Bàn phím / pad dưới piano roll ở tab Clip (07 §4.1d). Chỉ hiện khi panel ⤢ — panel thường không đủ chỗ.
  final bool clipKeyboard;

  /// ⇥ Step (07 §4.1d): phím bấm ghi nốt tại con trỏ; chạm lưới đặt con trỏ.
  final bool stepMode;

  bool get clipKeyboardVisible => clipKeyboard && panelExpanded;

  /// Ô đang chọn ở chế độ Edit.
  final CellRef? selected;

  /// Track nhận nốt từ pad/bàn phím (và MIDI controller, qua SELECT_TRACK).
  final int selectedTrack;

  /// Clip đã copy (Edit → Copy / Dán).
  final Clip? clipboard;
  final PanelTab tab;

  /// Ô đang được kéo clip đè lên (Edit, P2-21) → viền vàng.
  final CellRef? dragTarget;

  SessionUi copyWith({
    SessionMode? mode,
    CellRef? selected,
    bool clearSelected = false,
    int? selectedTrack,
    Clip? clipboard,
    PanelTab? tab,
    CellRef? dragTarget,
    bool clearDrag = false,
    PianoRollMode? pianoRollMode,
    bool? panelExpanded,
    double? grid,
    bool? clipKeyboard,
    bool? stepMode,
  }) => SessionUi(
    mode: mode ?? this.mode,
    selected: clearSelected ? null : (selected ?? this.selected),
    selectedTrack: selectedTrack ?? this.selectedTrack,
    clipboard: clipboard ?? this.clipboard,
    tab: tab ?? this.tab,
    dragTarget: clearDrag ? null : (dragTarget ?? this.dragTarget),
    pianoRollMode: pianoRollMode ?? this.pianoRollMode,
    panelExpanded: panelExpanded ?? this.panelExpanded,
    grid: grid ?? this.grid,
    clipKeyboard: clipKeyboard ?? this.clipKeyboard,
    stepMode: stepMode ?? this.stepMode,
  );
}

class SessionUiController extends Notifier<SessionUi> {
  @override
  SessionUi build() => const SessionUi();

  void toggleMode() => state = state.mode == SessionMode.perform
      ? state.copyWith(mode: SessionMode.edit)
      : state.copyWith(mode: SessionMode.perform, clearSelected: true);

  /// Chọn ô (Edit) → mở panel Clip (07 §3.1).
  void select(CellRef cell) => state = state.copyWith(
    selected: cell,
    tab: PanelTab.clip,
    panelExpanded: state.tab == PanelTab.clip && state.panelExpanded,
  );

  void clearSelection() => state = state.copyWith(clearSelected: true);

  /// Track nhận nốt. Báo engine để nốt từ MIDI controller cũng vào track này (05 §2 SELECT_TRACK).
  void selectTrack(int track) {
    if (track == state.selectedTrack) return;
    ref.read(performanceActionsProvider).selectTrack(track);
    state = state.copyWith(selectedTrack: track);
  }

  void copy(Clip clip) => state = state.copyWith(clipboard: clip);

  void showTab(PanelTab tab) =>
      state = state.copyWith(tab: tab, panelExpanded: tab == state.tab && state.panelExpanded);

  void setPanelExpanded(bool expanded) => state = state.copyWith(panelExpanded: expanded);

  void setPianoRollMode(PianoRollMode mode) => state = state.copyWith(pianoRollMode: mode);

  void setGrid(double grid) => state = state.copyWith(grid: grid);

  /// Bật bàn phím thì mở luôn ⤢ (bàn phím chỉ hiện khi panel cao); tắt bàn phím thì tắt luôn Step.
  void setClipKeyboard(bool on) => state = state.copyWith(
    clipKeyboard: on,
    panelExpanded: on || state.panelExpanded,
    stepMode: on && state.stepMode,
  );

  void setStepMode(bool on) => state = state.copyWith(stepMode: on);

  /// Vị trí ngón tay cuối cùng khi kéo (để mở menu thả đúng chỗ) — không phải state UI.
  Offset lastDragPosition = Offset.zero;

  void dragOver(CellRef? target, Offset globalPosition) {
    lastDragPosition = globalPosition;
    if (target != state.dragTarget) {
      state = target == null ? state.copyWith(clearDrag: true) : state.copyWith(dragTarget: target);
    }
  }

  CellRef? endDrag() {
    final t = state.dragTarget;
    if (t != null) state = state.copyWith(clearDrag: true);
    return t;
  }
}

final sessionUiProvider = NotifierProvider<SessionUiController, SessionUi>(SessionUiController.new);
