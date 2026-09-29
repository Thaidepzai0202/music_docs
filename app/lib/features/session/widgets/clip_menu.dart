import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../model/ids.dart';
import '../../../model/project.dart';
import '../../../ui_kit/name_dialog.dart';
import '../../clip/piano_roll.dart';
import '../project_controller.dart';
import '../session_ui.dart';
import '../../../l10n/l10n.dart';

/// Menu ngữ cảnh của ô ở chế độ Edit (P2-12, 07 §3.1): nhân bản, đổi tên, copy, màu track, xoá;
/// ô trống: dán. Mọi thao tác đi qua ProjectController → cập nhật cả model lẫn engine.
Future<void> showClipMenu(BuildContext context, WidgetRef ref, CellRef cell, Offset globalPosition) async {
  final clip = ref.read(projectControllerProvider)?.project.trackAt(cell.track)?.clipAt(cell.slot);
  final instrumentTrack =
      ref.read(projectControllerProvider)?.project.trackAt(cell.track)?.kind == TrackKind.instrument;
  final clipboard = ref.read(sessionUiProvider).clipboard;
  final hasUndo = clip != null && ref.read(projectControllerProvider.notifier).hasUndo(cell.track, cell.slot);
  final overlay = Overlay.of(context).context.findRenderObject()! as RenderBox;
  final action = await showMenu<String>(
    context: context,
    position: RelativeRect.fromRect(globalPosition & const Size(1, 1), Offset.zero & overlay.size),
    items: clip != null
        ? [
            PopupMenuItem(key: Key('menu.duplicate'), value: 'duplicate', child: Text(S.projectsNhanBan)),
            PopupMenuItem(key: Key('menu.rename'), value: 'rename', child: Text(S.projectsDoiTen)),
            PopupMenuItem(key: const Key('menu.copy'), value: 'copy', child: Text(S.menuCopy)),
            PopupMenuItem(key: Key('menu.color'), value: 'color', child: Text(S.menuMauTrack)),
            // Chỉ clip audio có lớp undo overdub (engine).
            if (clip is AudioClip)
              PopupMenuItem(
                key: const Key('menu.undoOverdub'),
                value: 'undoOverdub',
                enabled: hasUndo,
                child: Text(S.clipHoanTacOverdub),
              ),
            PopupMenuItem(key: Key('menu.delete'), value: 'delete', child: Text(S.projectsXoa2)),
          ]
        : [
            // 07 §4.1b: clip MIDI trống → mở piano roll ở chế độ Vẽ.
            if (instrumentTrack)
              for (final b in const [1, 2, 4])
                PopupMenuItem(
                  key: Key('menu.emptyMidi.$b'),
                  value: 'emptyMidi.$b',
                  child: Text(S.menuClipMidiTrong(b)),
                ),
            PopupMenuItem(
              key: const Key('menu.paste'),
              value: 'paste',
              enabled: clipboard != null,
              child: Text(clipboard == null ? S.menuDanChuaCopyClipNao : S.menuDan(clipboard.name)),
            ),
          ],
  );
  if (action == null || !context.mounted) return;
  final ctl = ref.read(projectControllerProvider.notifier);
  switch (action) {
    case 'duplicate':
      if (ctl.duplicateClip(cell.track, cell.slot) < 0 && context.mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.menuKhongConOTrongBen)));
      }
    case 'rename':
      final name = await askName(context, title: S.menuDoiTenClip, initial: clip!.name);
      if (name != null) ctl.renameClip(cell.track, cell.slot, name);
    case 'copy':
      ref.read(sessionUiProvider.notifier).copy(clip!);
    case 'color':
      final i = await showDialog<int>(context: context, builder: (_) => const _ColorPicker());
      if (i != null) ctl.setTrackColor(cell.track, colorHex(i));
    case 'undoOverdub':
      ctl.undoOverdub(cell.track, cell.slot);
    case 'delete':
      ctl.deleteClip(cell.track, cell.slot);
    case 'paste':
      ctl.placeClip(clipboard!, cell.track, cell.slot);
    case final a when a.startsWith('emptyMidi.'):
      if (ctl.createMidiClip(cell.track, cell.slot, int.parse(a.split('.').last))) {
        final ui = ref.read(sessionUiProvider.notifier);
        ui.setPianoRollMode(PianoRollMode.draw);
        ui.select(cell); // mở tab Clip với piano roll
      }
  }
}

/// Thả clip từ [from] xuống [to] (P2-21): Di chuyển hoặc Copy. Ô đích đã có clip → báo, không làm gì.
Future<void> showDropMenu(BuildContext context, WidgetRef ref, CellRef from, CellRef to, Offset globalPosition) async {
  final project = ref.read(projectControllerProvider)?.project;
  final clip = project?.trackAt(from.track)?.clipAt(from.slot);
  if (clip == null) return;
  final messenger = ScaffoldMessenger.of(context);
  if (project?.trackAt(to.track)?.clipAt(to.slot) != null) {
    messenger.showSnackBar(SnackBar(content: Text(S.menuODichDaCoClip)));
    return;
  }
  final overlay = Overlay.of(context).context.findRenderObject()! as RenderBox;
  final action = await showMenu<String>(
    context: context,
    position: RelativeRect.fromRect(globalPosition & const Size(1, 1), Offset.zero & overlay.size),
    items: [
      PopupMenuItem(key: Key('drop.move'), value: 'move', child: Text(S.menuDiChuyenToiDay)),
      PopupMenuItem(key: Key('drop.copy'), value: 'copy', child: Text(S.menuCopyToiDay)),
    ],
  );
  final ctl = ref.read(projectControllerProvider.notifier);
  switch (action) {
    case 'move':
      if (ctl.moveClip((track: from.track, slot: from.slot), (track: to.track, slot: to.slot))) {
        ref.read(sessionUiProvider.notifier).select(to);
      }
    case 'copy':
      ctl.placeClip(clip, to.track, to.slot);
  }
}

class _ColorPicker extends StatelessWidget {
  const _ColorPicker();

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(S.menuMauTrack),
      content: Wrap(
        spacing: 12,
        runSpacing: 12,
        children: [
          for (var i = 0; i < AppColors.tracks.length; i++)
            GestureDetector(
              key: Key('color.$i'),
              onTap: () => Navigator.pop(context, i),
              child: CircleAvatar(backgroundColor: AppColors.tracks[i], radius: 20),
            ),
        ],
      ),
    );
  }
}
