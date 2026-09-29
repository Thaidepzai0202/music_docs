import 'dart:convert';

import 'migrations/project_migrator.dart';
import 'project.dart';

final class DecodedProject {
  const DecodedProject(this.project, {required this.readOnly, required this.fromVersion});
  final Project project;
  final bool readOnly;
  final int fromVersion;
}

/// `project.json` ↔ [Project]: parse → migrate → fromJson. Đọc/ghi file an toàn là việc của
/// ProjectRepository (P2-24, 06 §5).
class ProjectCodec {
  ProjectCodec({ProjectMigrator? migrator}) : _migrator = migrator ?? ProjectMigrator();

  final ProjectMigrator _migrator;

  DecodedProject decode(String text) {
    final raw = jsonDecode(text);
    if (raw is! Map<String, dynamic>) throw const FormatException('project.json phải là object');
    final m = _migrator.migrate(raw);
    return DecodedProject(Project.fromJson(m.json), readOnly: m.readOnly, fromVersion: m.fromVersion);
  }

  String encode(Project project) => const JsonEncoder.withIndent('  ').convert(project.toJson());
}
