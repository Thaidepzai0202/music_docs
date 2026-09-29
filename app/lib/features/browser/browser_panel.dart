import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../data/library_repository.dart';
import '../../model/ids.dart';
import '../../model/project.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../session/session_ui.dart';
import '../../l10n/l10n.dart';

/// Tab Browser (P2-22): đọc `Library/manifest.json` (06 §4) — kit, nhạc cụ, loop.
/// Kit/nhạc cụ → gán vào track đang chọn. Loop → chép vào project rồi đặt vào ô trống của track đó.
class BrowserPanel extends ConsumerWidget {
  const BrowserPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final manifest = ref.watch(libraryManifestProvider);
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    final trackName = ref.watch(
      projectControllerProvider.select((s) => s?.project.trackAt(track)?.name ?? S.sessionTrackName(track + 1)),
    );
    return manifest.when(
      loading: () => const Center(child: CircularProgressIndicator()),
      error: (e, _) => Center(child: Text(S.browserKhongDocDuocThuVien(e))),
      data: (m) => Padding(
        padding: const EdgeInsets.fromLTRB(12, 6, 12 + SessionLayout.sceneColumnWidth, 8),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Expanded(
              child: _Section(
                title: S.browserKit,
                items: m.kits,
                action: S.browserGan(trackName),
                onPick: (it) => _assignInstrument(ref, track, it),
              ),
            ),
            Expanded(
              child: _Section(
                title: S.browserNhacCu,
                items: m.instruments,
                action: S.browserGan(trackName),
                onPick: (it) => _assignInstrument(ref, track, it),
              ),
            ),
            Expanded(
              child: _Section(
                title: S.browserLoop,
                items: m.loops,
                action: S.browserThemVao(trackName),
                onPick: (it) => _addLoop(context, ref, track, it),
              ),
            ),
          ],
        ),
      ),
    );
  }

  static void _assignInstrument(WidgetRef ref, int track, LibraryItem it) =>
      ref.read(projectControllerProvider.notifier).setInstrument(track, InstrumentRef.sfz(path: it.path));

  static Future<void> _addLoop(BuildContext context, WidgetRef ref, int track, LibraryItem it) async {
    final session = ref.read(projectControllerProvider);
    if (session == null) return;
    final ui = ref.read(sessionUiProvider);
    final t = session.project.trackAt(track);
    // Ô đích: ô đang chọn (Edit) nếu thuộc track này và còn trống; không thì ô trống đầu tiên.
    int? slot;
    final sel = ui.selected;
    if (sel != null && sel.track == track && t?.clipAt(sel.slot) == null) {
      slot = sel.slot;
    } else {
      for (var s = 0; s < SessionLayout.scenes; s++) {
        if (t?.clipAt(s) == null) {
          slot = s;
          break;
        }
      }
    }
    final messenger = ScaffoldMessenger.of(context);
    if (slot == null) {
      messenger.showSnackBar(SnackBar(content: Text(S.browserTrackNayKhongConO)));
      return;
    }
    final id = newId('c');
    final name = it.nameFor(Localizations.localeOf(context).languageCode); // tên theo ngôn ngữ LÚC GÁN
    final rel = await ref.read(libraryRepositoryProvider).importLoop(it, projectDir: session.dir, clipId: id);
    final ok = ref
        .read(projectControllerProvider.notifier)
        .addAudioClip(
          track,
          slot,
          Clip.audio(
                slot: slot,
                id: id,
                name: name,
                file: rel,
                lengthBeats: it.beats ?? 4,
                originalBpm: it.bpm ?? 120,
                warp: it.defaultWarp ?? WarpMode.stretch, // 06 §4: loop trống mặc định Re-Pitch
                tags: it.tags, // 06 §4: chép tags vào clips[].tags
              )
              as AudioClip,
        );
    if (ok) messenger.showSnackBar(SnackBar(content: Text(S.browserDaThemVaoO(name, slot + 1))));
  }
}

class _Section extends StatelessWidget {
  const _Section({required this.title, required this.items, required this.action, required this.onPick});

  final String title;
  final List<LibraryItem> items;
  final String action;
  final void Function(LibraryItem) onPick;

  @override
  Widget build(BuildContext context) {
    final small = Theme.of(context).textTheme.bodySmall;
    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 4),
      decoration: BoxDecoration(color: AppColors.background, borderRadius: BorderRadius.circular(6)),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(10, 8, 10, 4),
            child: Text.rich(
              TextSpan(
                children: [
                  TextSpan(
                    text: '${S.browserSection(title, items.length)}  ',
                    style: const TextStyle(fontWeight: FontWeight.w600),
                  ),
                  TextSpan(text: S.browserAssignHint(action), style: small),
                ],
              ),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
            ),
          ),
          Expanded(
            child: ListView(
              padding: EdgeInsets.zero,
              children: [
                for (final it in items)
                  // Mục gọn 1 hàng: tên + 1 dòng phụ, nút + luôn nằm trong khung (cột hẹp ~300 pt).
                  Padding(
                    key: Key('browser.${it.id}'),
                    padding: const EdgeInsets.fromLTRB(10, 2, 2, 2),
                    child: Row(
                      children: [
                        Expanded(
                          child: Column(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            children: [
                              Text(
                                it.nameFor(Localizations.localeOf(context).languageCode),
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                              ),
                              Text(
                                [
                                  if (it.bpm != null)
                                    S.browserLoopInfo(it.bpm!.toStringAsFixed(0), (it.beats ?? 0).round()),
                                  for (final t in it.tags) S.libraryTag(t),
                                ].join(' · '),
                                maxLines: 1,
                                overflow: TextOverflow.ellipsis,
                                style: small,
                              ),
                            ],
                          ),
                        ),
                        IconButton(
                          key: Key('browser.pick.${it.id}'),
                          tooltip: action,
                          icon: const Icon(Icons.add_circle_outline),
                          onPressed: () => onPick(it),
                        ),
                      ],
                    ),
                  ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
