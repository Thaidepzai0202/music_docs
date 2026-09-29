import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/performance_actions.dart';
import '../session_ui.dart';
import 'track_menu.dart';

/// Nút ▶ của 1 scene (P2-09). Perform: pointer-down → SCENE_LAUNCH ngay (đường nóng, 07 §3.1).
/// Edit: chạm để đổi tên scene (không launch).
class SceneButton extends ConsumerWidget {
  const SceneButton({super.key, required this.scene});

  final int scene;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final edit = ref.watch(sessionUiProvider.select((u) => u.mode == SessionMode.edit));
    if (edit) {
      return GestureDetector(
        behavior: HitTestBehavior.opaque,
        onTap: () => renameSceneDialog(context, ref, scene),
        child: const _ColumnButton(icon: Icons.edit, color: AppColors.queued),
      );
    }
    return Listener(
      behavior: HitTestBehavior.opaque,
      onPointerDown: (_) => ref.read(performanceActionsProvider).launchScene(scene),
      child: const _ColumnButton(icon: Icons.play_arrow, color: AppColors.textSecondary),
    );
  }
}

/// ■ Stop all (P2-09).
class StopAllButton extends ConsumerWidget {
  const StopAllButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Listener(
      behavior: HitTestBehavior.opaque,
      onPointerDown: (_) => ref.read(performanceActionsProvider).stopAll(),
      child: const _ColumnButton(icon: Icons.stop, color: AppColors.textPrimary),
    );
  }
}

class _ColumnButton extends StatelessWidget {
  const _ColumnButton({required this.icon, required this.color});

  final IconData icon;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.all(2),
      decoration: BoxDecoration(color: AppColors.surface, borderRadius: BorderRadius.circular(4)),
      alignment: Alignment.center,
      child: Icon(icon, color: color, size: 22),
    );
  }
}
