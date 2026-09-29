import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../model/project.dart';
import '../../../ui_kit/name_dialog.dart';
import '../project_controller.dart';
import '../../../l10n/l10n.dart';

/// Menu nhấn giữ tên track (header): đổi tên (`track.configure name`), đổi loại audio ↔ nhạc cụ (chỉ khi track trống).
Future<void> showTrackMenu(BuildContext context, WidgetRef ref, int track, Offset globalPosition) async {
  final t = ref.read(projectControllerProvider)?.project.trackAt(track);
  final name = t?.name ?? S.sessionTrackName(track + 1);
  final isInstrument = t?.kind == TrackKind.instrument;
  final empty = t == null || t.clips.isEmpty;
  final overlay = Overlay.of(context).context.findRenderObject()! as RenderBox;
  final action = await showMenu<String>(
    context: context,
    position: RelativeRect.fromRect(globalPosition & const Size(1, 1), Offset.zero & overlay.size),
    items: [
      PopupMenuItem(key: Key('header.menu.rename'), value: 'rename', child: Text(S.trackMenuDoiTenTrack)),
      PopupMenuItem(
        key: const Key('header.menu.kind'),
        value: 'kind',
        enabled: empty,
        child: Text(
          '${isInstrument ? S.trackMenuToAudio : S.trackMenuToInstrument}${empty ? '' : S.trackMenuMustBeEmpty}',
        ),
      ),
    ],
  );
  if (action == null || !context.mounted) return;
  final ctl = ref.read(projectControllerProvider.notifier);
  switch (action) {
    case 'rename':
      final v = await askName(context, title: S.trackMenuDoiTenTrack, initial: name);
      if (v != null) ctl.renameTrack(track, v);
    case 'kind':
      ctl.setTrackKind(track, isInstrument ? TrackKind.audio : TrackKind.instrument);
  }
}

/// Đổi tên scene (chỉ model).
Future<void> renameSceneDialog(BuildContext context, WidgetRef ref, int scene) async {
  final p = ref.read(projectControllerProvider)?.project;
  final current =
      p?.scenes.where((s) => s.index == scene).map((s) => s.name).firstOrNull ?? S.sessionSceneName(scene + 1);
  final v = await askName(context, title: S.trackMenuDoiTenScene, initial: current);
  if (v != null) ref.read(projectControllerProvider.notifier).renameScene(scene, v);
}
