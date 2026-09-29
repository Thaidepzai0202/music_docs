import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../project_controller.dart';
import '../session_layout.dart';
import '../session_ui.dart';
import '../../../l10n/l10n.dart';

/// Chip chọn track đang thao tác (tab Instrument, FX): tên tự co lại có "…" để hàng không tràn.
class TrackPicker extends ConsumerWidget {
  const TrackPicker({super.key, required this.track});

  final int track;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tracks = ref.watch(
      projectControllerProvider.select(
        (s) => [for (var i = 0; i < 8; i++) s?.project.trackAt(i)?.name ?? S.sessionTrackName(i + 1)],
      ),
    );
    final hex = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(track)?.color));
    final color = hex == null ? AppColors.tracks[track % 8] : parseHexColor(hex);
    return PopupMenuButton<int>(
      onSelected: (t) => ref.read(sessionUiProvider.notifier).selectTrack(t),
      itemBuilder: (_) => [for (var i = 0; i < 8; i++) PopupMenuItem(value: i, child: Text('${i + 1}. ${tracks[i]}'))],
      child: Container(
        constraints: const BoxConstraints(maxWidth: 220),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
        decoration: BoxDecoration(
          border: Border.all(color: color),
          borderRadius: BorderRadius.circular(6),
        ),
        child: Text(tracks[track], maxLines: 1, overflow: TextOverflow.ellipsis),
      ),
    );
  }
}
