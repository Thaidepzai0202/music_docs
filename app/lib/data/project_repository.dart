import 'dart:io';

import '../model/ids.dart';
import '../model/names.dart';
import '../model/project.dart';
import '../model/project_codec.dart';

final class LoadedProject {
  const LoadedProject({
    required this.project,
    required this.dir,
    required this.readOnly,
    required this.recoveredFromBackup,
    required this.missingClipIds,
  });

  final Project project;
  final String dir;

  /// File do app mới hơn ghi (06 §3).
  final bool readOnly;

  /// `project.json` hỏng/thiếu → đã mở bằng `project.json.bak` (06 §5.4).
  final bool recoveredFromBackup;

  /// Clip audio trỏ tới file không còn → UI vẽ xám, không crash (06 §5.4).
  final Set<String> missingClipIds;
}

/// Đọc/ghi `.loopproj` an toàn (06 §5). Phần lõi của P2-24; dọn rác audio (06 §5.5) làm sau.
class ProjectRepository {
  ProjectRepository({required this.projectsDir, ProjectCodec? codec}) : _codec = codec ?? ProjectCodec();

  /// `<Documents>/Projects`
  final Directory projectsDir;
  final ProjectCodec _codec;

  static const extension = '.loopproj';
  static const fileName = 'project.json';

  Future<List<Directory>> list() async {
    if (!await projectsDir.exists()) return const [];
    final dirs = await projectsDir.list().where((e) => e is Directory && e.path.endsWith(extension)).toList();
    return dirs.cast<Directory>()..sort((a, b) => a.path.compareTo(b.path));
  }

  /// Danh sách cho màn Projects, mới sửa nhất lên đầu. File hỏng vẫn hiện (kèm [ProjectSummary.error]).
  Future<List<ProjectSummary>> summaries() async {
    final out = <ProjectSummary>[];
    for (final d in await list()) {
      try {
        final l = await load(d.path);
        out.add(ProjectSummary(dir: d.path, name: l.project.name, modifiedAt: l.project.modifiedAt));
      } catch (e) {
        out.add(ProjectSummary(dir: d.path, name: _nameOf(d.path), modifiedAt: null, error: '$e'));
      }
    }
    out.sort((a, b) => (b.modifiedAt ?? DateTime(0)).compareTo(a.modifiedAt ?? DateTime(0)));
    return out;
  }

  /// Tạo thư mục project mới (tên trùng thì thêm " 2", " 3"…), lưu lần đầu, trả đường dẫn.
  Future<String> create(Project project) async {
    await projectsDir.create(recursive: true);
    project = cleanNames(project);
    final dir = await _freeDir(project.name);
    for (final sub in const ['audio', 'instruments', 'cache']) {
      await Directory('${dir.path}/$sub').create(recursive: true);
    }
    await save(project, dir.path);
    return dir.path;
  }

  /// Ghi `.tmp` → fsync → `project.json` cũ thành `.bak` → `.tmp` thành `project.json` (06 §5.1).
  /// Chết giữa chừng ở bất kỳ bước nào vẫn còn ít nhất một bản đầy đủ để [load] mở lại.
  Future<void> save(Project project, String dir) async {
    final tmp = File('$dir/$fileName.tmp');
    final main = File('$dir/$fileName');
    final bak = File('$dir/$fileName.bak');
    await tmp.writeAsString(_codec.encode(project), flush: true);
    if (await main.exists()) await main.rename(bak.path);
    await tmp.rename(main.path);
  }

  Future<LoadedProject> load(String dir) async {
    DecodedProject? decoded;
    var fromBackup = false;
    Object? firstError;
    try {
      decoded = _codec.decode(await File('$dir/$fileName').readAsString());
    } catch (e) {
      firstError = e;
    }
    if (decoded == null) {
      try {
        decoded = _codec.decode(await File('$dir/$fileName.bak').readAsString());
        fromBackup = true;
      } catch (_) {
        throw ProjectLoadException(dir, firstError);
      }
    }
    final p = decoded.project;
    final missing = <String>{
      for (final t in p.tracks)
        for (final c in t.clips)
          if (c is AudioClip && !File('$dir/${c.file}').existsSync()) c.id,
    };
    return LoadedProject(
      project: p,
      dir: dir,
      readOnly: decoded.readOnly,
      recoveredFromBackup: fromBackup,
      missingClipIds: missing,
    );
  }

  /// Đổi tên: đổi cả thư mục `.loopproj` lẫn `name` trong file. Trả đường dẫn mới.
  Future<String> rename(String dir, String newName, {DateTime? now}) async {
    newName = cleanName(newName);
    if (newName.isEmpty) return dir; // toàn ký tự điều khiển → giữ tên cũ
    final loaded = await load(dir);
    final target = await _freeDir(newName, except: dir);
    final moved = target.path == dir ? Directory(dir) : await Directory(dir).rename(target.path);
    await save(loaded.project.copyWith(name: newName, modifiedAt: (now ?? DateTime.now()).toUtc()), moved.path);
    return moved.path;
  }

  /// Nhân bản cả thư mục (audio, instruments…), project mới có id mới và tên [nameOf] (tên gốc) — UI truyền
  /// `S.projectsBanSao` để tên theo ngôn ngữ máy ("… (copy)" / "… (bản sao)").
  Future<String> duplicate(String dir, {required String Function(String name) nameOf, DateTime? now}) async {
    final loaded = await load(dir);
    final name = nameOf(loaded.project.name);
    final target = await _freeDir(name);
    await _copyDir(Directory(dir), target);
    // Bỏ file project của bản gốc: nếu để lại, save() bên dưới sẽ biến nó thành .bak (tên/id cũ).
    for (final stale in [fileName, '$fileName.bak', '$fileName.tmp']) {
      final f = File('${target.path}/$stale');
      if (await f.exists()) await f.delete();
    }
    final t = (now ?? DateTime.now()).toUtc();
    await save(loaded.project.copyWith(id: newId('p'), name: name, createdAt: t, modifiedAt: t), target.path);
    return target.path;
  }

  Future<void> delete(String dir) async {
    final d = Directory(dir);
    if (!d.path.endsWith(extension) || !d.path.startsWith(projectsDir.path)) {
      throw ArgumentError('Không xoá thư mục ngoài Projects/: $dir');
    }
    if (await d.exists()) await d.delete(recursive: true);
  }

  Future<Directory> _freeDir(String name, {String? except}) async {
    final base = _safeName(name);
    var dir = Directory('${projectsDir.path}/$base$extension');
    for (var i = 2; dir.path != except && await dir.exists(); i++) {
      dir = Directory('${projectsDir.path}/$base $i$extension');
    }
    return dir;
  }

  static Future<void> _copyDir(Directory from, Directory to) async {
    await to.create(recursive: true);
    await for (final e in from.list(followLinks: false)) {
      final name = e.uri.pathSegments.lastWhere((s) => s.isNotEmpty);
      if (e is Directory) {
        await _copyDir(e, Directory('${to.path}/$name'));
      } else if (e is File) {
        await e.copy('${to.path}/$name');
      }
    }
  }

  static String _nameOf(String dir) {
    final last = dir.split('/').lastWhere((s) => s.isNotEmpty);
    return last.endsWith(extension) ? last.substring(0, last.length - extension.length) : last;
  }

  static String _safeName(String name) {
    final s = cleanName(name).replaceAll(RegExp(r'[/\\:*?"<>|]'), '_').trim();
    return s.isEmpty ? 'Project' : s;
  }
}

final class ProjectLoadException implements Exception {
  const ProjectLoadException(this.dir, this.cause);
  final String dir;
  final Object? cause;
  @override
  String toString() => 'ProjectLoadException($dir: $cause)';
}

final class ProjectSummary {
  const ProjectSummary({required this.dir, required this.name, required this.modifiedAt, this.error});
  final String dir;
  final String name;
  final DateTime? modifiedAt;

  /// Khác null nếu cả project.json lẫn .bak đều không đọc được.
  final String? error;
}
