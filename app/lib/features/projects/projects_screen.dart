import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/router.dart';
import '../../app/theme.dart';
import '../../data/data_providers.dart';
import '../../data/project_repository.dart';
import '../../engine/engine_providers.dart';
import '../../model/ids.dart';
import '../../ui_kit/name_dialog.dart';
import '../session/project_controller.dart';
import '../settings/app_settings.dart';
import 'demo_project.dart';
import '../../l10n/l10n.dart';

/// Màn Projects (P2-23): danh sách, tạo mới, project demo, đổi tên, nhân bản, xoá, mở.
class ProjectsScreen extends ConsumerStatefulWidget {
  const ProjectsScreen({super.key});

  @override
  ConsumerState<ProjectsScreen> createState() => _ProjectsScreenState();
}

class _ProjectsScreenState extends ConsumerState<ProjectsScreen> {
  late Future<List<ProjectSummary>> _items = _load();
  bool _busy = false;
  bool _argsHandled = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    // Onboarding vừa tạo project demo → mở luôn (07 §1: Onboarding → Projects → Session).
    if (_argsHandled) return;
    _argsHandled = true;
    final openDir = ModalRoute.of(context)?.settings.arguments;
    if (openDir is String) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) _open(openDir);
      });
    }
  }

  ProjectRepository get _repo => ref.read(projectRepositoryProvider);

  Future<List<ProjectSummary>> _load() => ref.read(projectRepositoryProvider).summaries();

  void _reload() => setState(() {
    _items = _load(); // thân khối: setState không được nhận callback trả về Future
  });

  Future<void> _run(Future<void> Function() action) async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      await action();
    } catch (e) {
      if (mounted) ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.projectsLoi(e))));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _open(String dir) => _run(() => _openNow(dir));

  /// Mở project rồi vào Session; quay lại thì đóng project. Gọi bên trong [_run].
  Future<void> _openNow(String dir) async {
    final loaded = await _repo.load(dir);
    final ctl = ref.read(projectControllerProvider.notifier);
    // Không await: màn Session hiện tiến độ "Đang mở project… x/y" trong lúc đợi job.
    ctl.open(loaded.project, dir: dir, readOnly: loaded.readOnly, missingClipIds: loaded.missingClipIds);
    if (!mounted) return;
    if (loaded.recoveredFromBackup) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(S.projectsProjectJsonBiHongDa)));
    }
    // Vào Session thẳng từ Projects (không qua màn spike) thì audio chưa chạy → bật (idempotent; cần quyền mic).
    unawaited(ref.read(engineAudioProvider).start().then<void>((_) {}, onError: (Object _) {}));
    await Navigator.pushNamed(context, AppRoutes.session);
    ctl.close(); // autosave lưu nốt thay đổi còn chờ khi đóng
    if (mounted) _reload();
  }

  Future<void> _create() async {
    final name = await _askName(title: S.projectsProjectMoi, initial: S.projectsProjectMoi);
    if (name == null) return;
    await _run(() async {
      final dir = await _repo.create(
        newProject(
          name,
          monitor: ref.read(settingsProvider).defaultMonitor,
          trackName: S.sessionTrackName,
          sceneName: S.sessionSceneName,
        ),
      );
      _reload();
      await _openNow(dir);
    });
  }

  Future<void> _createDemo() => _run(() async {
    await createDemoProject(_repo);
    _reload();
  });

  Future<void> _rename(ProjectSummary p) async {
    final name = await _askName(title: S.projectsDoiTen, initial: p.name);
    if (name == null || name == p.name) return;
    await _run(() async {
      await _repo.rename(p.dir, name);
      _reload();
    });
  }

  Future<void> _duplicate(ProjectSummary p) => _run(() async {
    await _repo.duplicate(p.dir, nameOf: S.projectsBanSao);
    _reload();
  });

  Future<void> _delete(ProjectSummary p) async {
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(S.projectsXoa(p.name)),
        content: Text(S.projectsXoaCaBanThuAm),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(S.exportHuy)),
          FilledButton(
            key: const Key('projects.confirmDelete'),
            style: FilledButton.styleFrom(backgroundColor: AppColors.record),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(S.projectsXoa2),
          ),
        ],
      ),
    );
    if (ok != true) return;
    await _run(() async {
      await _repo.delete(p.dir);
      _reload();
    });
  }

  Future<String?> _askName({required String title, required String initial}) =>
      askName(context, title: title, initial: initial);

  static String _date(DateTime? d) {
    if (d == null) return '—';
    final l = d.toLocal();
    String two(int v) => v.toString().padLeft(2, '0');
    return '${two(l.day)}/${two(l.month)}/${l.year} ${two(l.hour)}:${two(l.minute)}';
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(S.projectsTitle),
        actions: [
          TextButton.icon(
            key: const Key('projects.demo'),
            onPressed: _busy ? null : _createDemo,
            icon: const Icon(Icons.auto_awesome),
            label: Text(S.projectsTaoProjectDemo),
          ),
          const SizedBox(width: 8),
          FilledButton.icon(
            key: const Key('projects.new'),
            onPressed: _busy ? null : _create,
            icon: const Icon(Icons.add),
            label: Text(S.projectsProjectMoi),
          ),
          const SizedBox(width: 8),
          IconButton(
            key: const Key('projects.settings'),
            tooltip: S.projectsCaiDat,
            icon: const Icon(Icons.settings),
            onPressed: () => Navigator.pushNamed(context, AppRoutes.settings),
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: FutureBuilder<List<ProjectSummary>>(
        future: _items,
        builder: (context, snap) {
          if (!snap.hasData) return const SizedBox.shrink();
          final items = snap.data!;
          if (items.isEmpty) {
            return Center(child: Text(S.projectsChuaCoProjectBamProject));
          }
          return ListView.separated(
            padding: const EdgeInsets.all(16),
            itemCount: items.length,
            separatorBuilder: (_, _) => const SizedBox(height: 8),
            itemBuilder: (context, i) {
              final p = items[i];
              return ListTile(
                key: Key('projects.tile.${p.name}'),
                tileColor: AppColors.surface,
                shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
                title: Text(p.name),
                subtitle: Text(
                  p.error == null ? S.projectsSuaLanCuoi(_date(p.modifiedAt)) : S.projectsKhongDocDuocProject,
                  style: TextStyle(color: p.error == null ? AppColors.textSecondary : AppColors.record),
                ),
                onTap: p.error == null && !_busy ? () => _open(p.dir) : null,
                trailing: PopupMenuButton<String>(
                  key: Key('projects.menu.${p.name}'),
                  onSelected: (v) => switch (v) {
                    'rename' => _rename(p),
                    'duplicate' => _duplicate(p),
                    _ => _delete(p),
                  },
                  itemBuilder: (_) => [
                    PopupMenuItem(value: 'rename', child: Text(S.projectsDoiTen)),
                    PopupMenuItem(value: 'duplicate', child: Text(S.projectsNhanBan)),
                    PopupMenuItem(value: 'delete', child: Text(S.projectsXoa2)),
                  ],
                ),
              );
            },
          );
        },
      ),
    );
  }
}
