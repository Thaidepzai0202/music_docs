import 'dart:io';

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

  /// Tạo thư mục project mới (tên trùng thì thêm " 2", " 3"…), lưu lần đầu, trả đường dẫn.
  Future<String> create(Project project) async {
    await projectsDir.create(recursive: true);
    final base = _safeName(project.name);
    var dir = Directory('${projectsDir.path}/$base$extension');
    for (var i = 2; await dir.exists(); i++) {
      dir = Directory('${projectsDir.path}/$base $i$extension');
    }
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

  static String _safeName(String name) {
    final s = name.replaceAll(RegExp(r'[/\\:*?"<>|]'), '_').trim();
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
