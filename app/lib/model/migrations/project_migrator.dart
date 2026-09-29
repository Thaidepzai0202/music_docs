import 'dart:convert';

/// Schema `project.json` mới nhất app hiểu (06 §2).
const int kCurrentSchemaVersion = 1;

/// Biến JSON của version `v` thành JSON của version `v + 1`.
typedef JsonMigration = Map<String, dynamic> Function(Map<String, dynamic> json);

final class MigrationResult {
  const MigrationResult({required this.json, required this.fromVersion, required this.readOnly});

  /// JSON đã nâng lên [kCurrentSchemaVersion] (hoặc giữ nguyên nếu [readOnly]).
  final Map<String, dynamic> json;
  final int fromVersion;

  /// File do app MỚI hơn ghi → mở chỉ đọc và cảnh báo (06 §3).
  final bool readOnly;
}

/// Chuỗi migrator phía Dart: `v1 → v2 → …` (06 §3). Chạy trên JSON thô, TRƯỚC `Project.fromJson`.
class ProjectMigrator {
  ProjectMigrator({Map<int, JsonMigration>? migrations, this.currentVersion = kCurrentSchemaVersion})
    : _migrations = migrations ?? defaultMigrations;

  /// Key = version nguồn. v1 là schema đầu tiên nên chưa có bước nào: `v1 → v1` là no-op.
  static final Map<int, JsonMigration> defaultMigrations = {};

  final Map<int, JsonMigration> _migrations;
  final int currentVersion;

  MigrationResult migrate(Map<String, dynamic> input) {
    final from = input['schemaVersion'];
    if (from is! int || from < 1) {
      throw FormatException('schemaVersion không hợp lệ: $from');
    }
    if (from > currentVersion) {
      return MigrationResult(json: input, fromVersion: from, readOnly: true);
    }
    // Copy sâu: migrator được phép sửa tại chỗ mà không đụng dữ liệu của người gọi.
    var json = (jsonDecode(jsonEncode(input)) as Map).cast<String, dynamic>();
    for (var v = from; v < currentVersion; v++) {
      final step = _migrations[v];
      if (step == null) throw StateError('Thiếu migrator v$v → v${v + 1}');
      json = step(json)..['schemaVersion'] = v + 1;
    }
    return MigrationResult(json: json, fromVersion: from, readOnly: false);
  }
}
