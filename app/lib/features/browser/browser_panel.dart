import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../data/library_repository.dart';
import '../../model/ids.dart';
import '../../model/names.dart';
import '../../model/project.dart';
import '../../services/service_providers.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../session/session_ui.dart';
import '../settings/app_settings.dart';
import '../../l10n/l10n.dart';

/// Danh mục ở cột trái của Browser (07 §4.1e).
enum BrowserCategory { drums, instruments, loops, recordings, favorites }

/// Một mục trong Browser: kit / nhạc cụ / loop của thư viện, hoặc nhạc cụ tự thu của project ("Bản thu của tôi").
sealed class BrowserEntry {
  const BrowserEntry();

  /// Khoá ★ (settings.json) và key widget.
  String get key;
  String get id;
  String name(String lang);

  /// Mọi tên (mọi ngôn ngữ) — để tìm kiếm.
  Iterable<String> get names;
  IconData get icon;
  bool get isLoop => false;
}

final class LibraryEntry extends BrowserEntry {
  const LibraryEntry(this.item);
  final LibraryItem item;

  @override
  String get key => item.key;
  @override
  String get id => item.id;
  @override
  String name(String lang) => item.nameFor(lang);
  @override
  Iterable<String> get names => item.names.values;
  @override
  IconData get icon => switch (item.kind) {
    LibraryKind.kit => Icons.grid_view,
    LibraryKind.instrument => Icons.piano,
    LibraryKind.loop => Icons.graphic_eq,
  };
  @override
  bool get isLoop => item.kind == LibraryKind.loop;
}

final class UserEntry extends BrowserEntry {
  const UserEntry(this.instrument);
  final UserInstrument instrument;

  @override
  String get key => 'user:${instrument.id}';
  @override
  String get id => instrument.id;
  @override
  String name(String lang) => instrument.name;
  @override
  Iterable<String> get names => [instrument.name];
  @override
  IconData get icon => Icons.mic;
}

/// Tên thư mục (một đoạn của `category`) → khoá ARB `libraryFolder`: "Winds & Brass" → "windsBrass".
String folderToken(String segment) {
  final words = segment.split(RegExp('[^A-Za-z0-9]+')).where((w) => w.isNotEmpty).toList();
  if (words.isEmpty) return segment;
  return words.first.toLowerCase() + words.skip(1).map((w) => w[0].toUpperCase() + w.substring(1).toLowerCase()).join();
}

String folderLabel(String segment) => S.libraryFolder(folderToken(segment));

/// Tab Browser kiểu thư mục như Ableton (07 §4.1e, P2-34). Cột trái: danh mục (Drums · Instruments · Loops · Bản thu
/// của tôi · ★ Yêu thích). Cột phải: thư mục theo `category` (06 §4) + breadcrumb, ô tìm kiếm trên toàn thư viện, biểu
/// tượng loại. Chạm mục → nghe thử (`preview.play`, bật / tắt bằng 🎧); nhấn giữ → ★; kéo biểu tượng vào header track
/// hoặc ô clip, hoặc nút gán vào track đang chọn. Kit / nhạc cụ → track nhạc cụ; loop → chép vào project rồi đặt vào ô.
class BrowserPanel extends ConsumerStatefulWidget {
  const BrowserPanel({super.key});

  static const categoriesWidth = 180.0;

  @override
  ConsumerState<BrowserPanel> createState() => _BrowserPanelState();
}

class _BrowserPanelState extends ConsumerState<BrowserPanel> {
  BrowserCategory _category = BrowserCategory.drums;

  /// Thư mục con đang mở bên trong danh mục (các đoạn của `category` sau đoạn gốc).
  List<String> _path = const [];
  final _search = TextEditingController();
  String _query = '';
  String? _selectedKey;

  // Lấy 1 lần: dispose() còn phải dừng nghe thử.
  late final _preview = ref.read(previewServiceProvider);

  @override
  void initState() {
    super.initState();
    _preview; // lấy trước: dispose() không đọc ref được nữa
  }

  @override
  void dispose() {
    _preview.stop();
    _search.dispose();
    super.dispose();
  }

  static const _roots = {
    BrowserCategory.drums: 'Drums',
    BrowserCategory.instruments: 'Instruments',
    BrowserCategory.loops: 'Loops',
  };

  String get _lang => Localizations.localeOf(context).languageCode;

  String _categoryLabel(BrowserCategory c) => switch (c) {
    BrowserCategory.drums || BrowserCategory.instruments || BrowserCategory.loops => folderLabel(_roots[c]!),
    BrowserCategory.recordings => S.browserBanThuCuaToi,
    BrowserCategory.favorites => S.browserYeuThich,
  };

  void _open(BrowserCategory c) => setState(() {
    _category = c;
    _path = const [];
    _search.clear();
    _query = '';
  });

  /// Nội dung cột phải: (thư mục con, mục).
  (List<String>, List<BrowserEntry>) _listing(LibraryManifest m, List<UserInstrument> users, List<String> favorites) {
    final library = [
      for (final it in m.all)
        if (!it.hidden) LibraryEntry(it),
    ];
    final user = [for (final u in users) UserEntry(u)];
    if (_query.isNotEmpty) {
      final q = foldSearch(_query);
      bool match(BrowserEntry e) =>
          e.names.any((n) => foldSearch(n).contains(q)) ||
          (e is LibraryEntry &&
              (foldSearch(e.item.category).contains(q) ||
                  e.item.tags.any((t) => foldSearch(S.libraryTag(t)).contains(q) || t.contains(q))));
      return (const [], [...library.where(match), ...user.where(match)]);
    }
    switch (_category) {
      case BrowserCategory.recordings:
        return (const [], user);
      case BrowserCategory.favorites:
        final byKey = {
          for (final e in [...library, ...user]) e.key: e,
        };
        return (
          const [],
          [
            for (final k in favorites)
              if (byKey[k] != null) byKey[k]!,
          ],
        );
      case BrowserCategory.drums || BrowserCategory.instruments || BrowserCategory.loops:
        final prefix = [_roots[_category]!, ..._path].join('/');
        final depth = _path.length + 1;
        final folders = <String>{};
        final direct = <BrowserEntry>[];
        for (final e in library) {
          final cat = e.item.category;
          if (cat == prefix) {
            direct.add(e);
          } else if (cat.startsWith('$prefix/')) {
            folders.add(cat.split('/')[depth]);
          }
        }
        return (folders.toList()..sort(), direct);
    }
  }

  void _tap(BrowserEntry e) {
    setState(() => _selectedKey = e.key);
    if (!ref.read(settingsProvider).autoPreview) return;
    switch (e) {
      case LibraryEntry(:final item):
        _preview.playLibrary(item);
      case UserEntry(:final instrument):
        _preview.playUser(instrument);
    }
  }

  void _favorite(BrowserEntry e) {
    final on = ref.read(settingsProvider.notifier).toggleFavorite(e.key);
    final name = e.name(_lang);
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(on ? S.browserDaThemYeuThich(name) : S.browserDaBoYeuThich(name))));
  }

  /// Gán vào [track] (nút → track đang chọn; kéo thả → track / ô đích, [slot] chỉ dùng cho loop).
  void _assign(BrowserEntry e, int track, {int? slot}) {
    switch (e) {
      case LibraryEntry(:final item) when item.kind == LibraryKind.loop:
        _addLoop(context, ref, track, item, slot: slot);
      case LibraryEntry(:final item):
        _setInstrument(context, ref, track, InstrumentRef.sfz(path: item.path), item.nameFor(_lang));
      case UserEntry(:final instrument):
        _setInstrument(context, ref, track, InstrumentRef.user(id: instrument.id), instrument.name);
    }
  }

  // ---- Kéo thả vào header track / ô clip (dò theo toạ độ như kéo clip ở Edit, P2-21) ----

  CellRef? _targetAt(Offset global) {
    final cell = cellAtGlobal(global);
    if (cell != null) return cell;
    final t = trackHeaderAtGlobal(global);
    return t == null ? null : CellRef(t, -1);
  }

  void _dragUpdate(Offset global) => ref.read(sessionUiProvider.notifier).dragOver(_targetAt(global), global);

  void _dragEnd(BrowserEntry e) {
    final target = ref.read(sessionUiProvider.notifier).endDrag();
    if (target != null) _assign(e, target.track, slot: target.slot >= 0 ? target.slot : null);
  }

  @override
  Widget build(BuildContext context) {
    final manifest = ref.watch(libraryManifestProvider);
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    final trackName = ref.watch(
      projectControllerProvider.select((s) => s?.project.trackAt(track)?.name ?? S.sessionTrackName(track + 1)),
    );
    final users = ref.watch(projectControllerProvider.select((s) => s?.project.userInstruments ?? const []));
    final favorites = ref.watch(settingsProvider.select((s) => s.favorites));
    final autoPreview = ref.watch(settingsProvider.select((s) => s.autoPreview));
    final small = Theme.of(context).textTheme.bodySmall;
    return manifest.when(
      loading: () => const Center(child: CircularProgressIndicator()),
      error: (e, _) => Center(child: Text(S.browserKhongDocDuocThuVien(e))),
      data: (m) {
        final (folders, entries) = _listing(m, users, favorites);
        final empty = folders.isEmpty && entries.isEmpty;
        return Padding(
          padding: const EdgeInsets.fromLTRB(12, 6, 12 + SessionLayout.sceneColumnWidth, 8),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              SizedBox(
                width: BrowserPanel.categoriesWidth,
                child: Material(
                  type: MaterialType.transparency,
                  child: ListView(
                    padding: EdgeInsets.zero,
                    children: [
                      for (final c in BrowserCategory.values)
                        _CategoryTile(
                          key: Key('browser.cat.${c.name}'),
                          icon: switch (c) {
                            BrowserCategory.drums => Icons.grid_view,
                            BrowserCategory.instruments => Icons.piano,
                            BrowserCategory.loops => Icons.graphic_eq,
                            BrowserCategory.recordings => Icons.mic,
                            BrowserCategory.favorites => Icons.star,
                          },
                          label: _categoryLabel(c),
                          selected: _query.isEmpty && c == _category,
                          onTap: () => _open(c),
                        ),
                    ],
                  ),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Container(
                  decoration: BoxDecoration(color: AppColors.background, borderRadius: BorderRadius.circular(6)),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      SizedBox(
                        height: 44,
                        child: Row(
                          children: [
                            Expanded(child: _query.isEmpty ? _breadcrumb() : const SizedBox()),
                            SizedBox(
                              width: 220,
                              child: TextField(
                                key: const Key('browser.search'),
                                controller: _search,
                                onChanged: (v) => setState(() => _query = v.trim()),
                                style: const TextStyle(fontSize: 14),
                                decoration: InputDecoration(
                                  isDense: true,
                                  contentPadding: const EdgeInsets.symmetric(vertical: 10),
                                  hintText: S.browserTimKiem,
                                  prefixIcon: const Icon(Icons.search, size: 18),
                                  suffixIcon: _query.isEmpty
                                      ? null
                                      : IconButton(
                                          key: const Key('browser.search.clear'),
                                          icon: const Icon(Icons.close, size: 18),
                                          onPressed: () => setState(() {
                                            _search.clear();
                                            _query = '';
                                          }),
                                        ),
                                ),
                              ),
                            ),
                            IconButton(
                              key: const Key('browser.autoPreview'),
                              tooltip: S.browserTuNgheThu,
                              isSelected: autoPreview,
                              icon: const Icon(Icons.headphones_outlined),
                              selectedIcon: const Icon(Icons.headphones),
                              onPressed: () {
                                ref.read(settingsProvider.notifier).setAutoPreview(!autoPreview);
                                if (autoPreview) _preview.stop();
                              },
                            ),
                          ],
                        ),
                      ),
                      Expanded(
                        child: empty
                            ? Center(
                                child: Text(
                                  _query.isNotEmpty
                                      ? S.browserKhongCoKetQua
                                      : switch (_category) {
                                          BrowserCategory.recordings => S.browserChuaCoBanThu,
                                          BrowserCategory.favorites => S.browserChuaCoYeuThich,
                                          _ => S.browserThuMucTrong,
                                        },
                                  style: small,
                                  textAlign: TextAlign.center,
                                ),
                              )
                            : Material(
                                type: MaterialType.transparency,
                                child: ListView(
                                  key: const Key('browser.list'),
                                  padding: EdgeInsets.zero,
                                  children: [
                                    for (final f in folders)
                                      ListTile(
                                        key: Key('browser.folder.${folderToken(f)}'),
                                        dense: true,
                                        leading: const Icon(Icons.folder_outlined),
                                        title: Text(folderLabel(f), maxLines: 1, overflow: TextOverflow.ellipsis),
                                        trailing: const Icon(Icons.chevron_right),
                                        onTap: () => setState(() => _path = [..._path, f]),
                                      ),
                                    for (final e in entries)
                                      _EntryRow(
                                        key: Key('browser.${e.id}'),
                                        entry: e,
                                        lang: _lang,
                                        selected: e.key == _selectedKey,
                                        favorite: favorites.contains(e.key),
                                        showFolder: _query.isNotEmpty || _category == BrowserCategory.favorites,
                                        action: e.isLoop ? S.browserThemVao(trackName) : S.browserGan(trackName),
                                        onTap: () => _tap(e),
                                        onLongPress: () => _favorite(e),
                                        onPick: () => _assign(e, track),
                                        onDragUpdate: _dragUpdate,
                                        onDragEnd: () => _dragEnd(e),
                                      ),
                                  ],
                                ),
                              ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _breadcrumb() {
    final root = _roots[_category];
    final crumbs = [_categoryLabel(_category), if (root != null) ..._path.map(folderLabel)];
    return SingleChildScrollView(
      scrollDirection: Axis.horizontal,
      padding: const EdgeInsets.symmetric(horizontal: 8),
      child: Row(
        children: [
          for (var i = 0; i < crumbs.length; i++) ...[
            if (i > 0) const Icon(Icons.chevron_right, size: 16, color: AppColors.textSecondary),
            TextButton(
              key: Key('browser.crumb.$i'),
              onPressed: i == crumbs.length - 1 ? null : () => setState(() => _path = _path.sublist(0, i)),
              child: Text(crumbs[i]),
            ),
          ],
        ],
      ),
    );
  }

  /// Gán kit / nhạc cụ: track thành track nhạc cụ và mang tên nhạc cụ nếu chưa có tên riêng (07 §4.1c). Track audio
  /// đang có clip thì không đổi loại được.
  static void _setInstrument(BuildContext context, WidgetRef ref, int track, InstrumentRef instrument, String name) {
    final ok = ref.read(projectControllerProvider.notifier).setInstrument(track, instrument, name: name);
    if (!ok) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.browserTrackAudioCoClip)));
  }

  /// Loop → chép vào project rồi đặt vào ô [slot] (kéo thả) hoặc ô đang chọn / ô trống đầu tiên của [track].
  static Future<void> _addLoop(BuildContext context, WidgetRef ref, int track, LibraryItem it, {int? slot}) async {
    final session = ref.read(projectControllerProvider);
    if (session == null) return;
    final ui = ref.read(sessionUiProvider);
    final t = session.project.trackAt(track);
    final messenger = ScaffoldMessenger.of(context);
    if (slot != null && t?.clipAt(slot) != null) {
      messenger.showSnackBar(SnackBar(content: Text(S.browserODaCoClip(slot + 1))));
      return;
    }
    // Ô đích: ô thả; không thì ô đang chọn (Edit) nếu thuộc track này và còn trống; không thì ô trống đầu tiên.
    final sel = ui.selected;
    if (slot == null && sel != null && sel.track == track && t?.clipAt(sel.slot) == null) {
      slot = sel.slot;
    }
    if (slot == null) {
      for (var s = 0; s < SessionLayout.scenes; s++) {
        if (t?.clipAt(s) == null) {
          slot = s;
          break;
        }
      }
    }
    // 07 §4.1c: loop làm track thành track audio — chỉ được khi track nhạc cụ còn trống.
    if (t != null && t.kind == TrackKind.instrument && t.clips.isNotEmpty) {
      messenger.showSnackBar(SnackBar(content: Text(S.browserTrackNhacCuCoClip)));
      return;
    }
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

/// Hàng danh mục gọn (36 pt): cả 5 danh mục vừa panel thường (260 pt) mà không phải cuộn.
class _CategoryTile extends StatelessWidget {
  const _CategoryTile({
    super.key,
    required this.icon,
    required this.label,
    required this.selected,
    required this.onTap,
  });

  static const height = 36.0;

  final IconData icon;
  final String label;
  final bool selected;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => Material(
    color: selected ? AppColors.background : Colors.transparent,
    borderRadius: BorderRadius.circular(6),
    child: InkWell(
      borderRadius: BorderRadius.circular(6),
      onTap: onTap,
      child: SizedBox(
        height: height,
        child: Row(
          children: [
            const SizedBox(width: 8),
            Icon(icon, size: 18, color: icon == Icons.star ? AppColors.queued : AppColors.textSecondary),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                label,
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                style: TextStyle(
                  color: selected ? AppColors.textPrimary : AppColors.textSecondary,
                  fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
                ),
              ),
            ),
          ],
        ),
      ),
    ),
  );
}

/// Một mục: [biểu tượng loại = tay kéo] tên + dòng phụ, ★ nếu yêu thích, nút gán vào track đang chọn. Chạm → nghe
/// thử; nhấn giữ → ★. Chỉ biểu tượng kéo được (kéo ngay, không chờ) để cả hàng vẫn cuộn dọc bình thường.
class _EntryRow extends StatelessWidget {
  const _EntryRow({
    super.key,
    required this.entry,
    required this.lang,
    required this.selected,
    required this.favorite,
    required this.showFolder,
    required this.action,
    required this.onTap,
    required this.onLongPress,
    required this.onPick,
    required this.onDragUpdate,
    required this.onDragEnd,
  });

  final BrowserEntry entry;
  final String lang;
  final bool selected;
  final bool favorite;
  final bool showFolder;
  final String action;
  final VoidCallback onTap;
  final VoidCallback onLongPress;
  final VoidCallback onPick;
  final ValueChanged<Offset> onDragUpdate;
  final VoidCallback onDragEnd;

  String _subtitle() {
    final e = entry;
    return switch (e) {
      LibraryEntry(:final item) => [
        if (showFolder) item.category.split('/').map(folderLabel).join(' › '),
        if (item.bpm != null) S.browserLoopInfo(item.bpm!.toStringAsFixed(0), (item.beats ?? 0).round()),
        for (final t in item.tags) S.libraryTag(t),
      ].join(' · '),
      UserEntry() => S.browserBanThuCuaToi,
    };
  }

  @override
  Widget build(BuildContext context) {
    final small = Theme.of(context).textTheme.bodySmall;
    final name = entry.name(lang);
    return Material(
      color: selected ? AppColors.surface : Colors.transparent,
      child: InkWell(
        onTap: onTap,
        onLongPress: onLongPress,
        child: Padding(
          padding: const EdgeInsets.fromLTRB(4, 2, 2, 2),
          child: Row(
            children: [
              Draggable<BrowserEntry>(
                key: Key('browser.drag.${entry.id}'),
                data: entry,
                dragAnchorStrategy: pointerDragAnchorStrategy,
                feedback: Material(
                  color: AppColors.surface,
                  elevation: 6,
                  borderRadius: BorderRadius.circular(8),
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                    child: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [Icon(entry.icon, size: 18), const SizedBox(width: 6), Text(name)],
                    ),
                  ),
                ),
                onDragUpdate: (d) => onDragUpdate(d.globalPosition),
                onDragEnd: (_) => onDragEnd(),
                child: Tooltip(
                  message: S.browserKeoVaoTrack,
                  child: SizedBox(width: 44, height: 44, child: Icon(entry.icon, size: 22)),
                ),
              ),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Text(name, maxLines: 1, overflow: TextOverflow.ellipsis),
                    Text(_subtitle(), maxLines: 1, overflow: TextOverflow.ellipsis, style: small),
                  ],
                ),
              ),
              if (favorite)
                const Padding(
                  padding: EdgeInsets.symmetric(horizontal: 4),
                  child: Icon(Icons.star, size: 16, color: AppColors.queued),
                ),
              IconButton(
                key: Key('browser.pick.${entry.id}'),
                tooltip: action,
                icon: const Icon(Icons.add_circle_outline),
                onPressed: onPick,
              ),
            ],
          ),
        ),
      ),
    );
  }
}
