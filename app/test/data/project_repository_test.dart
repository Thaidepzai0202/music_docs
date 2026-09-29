// Lõi P2-24: lưu an toàn (06 §5.1), khôi phục từ .bak và đánh dấu audio thiếu (06 §5.4).
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/project_repository.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

void main() {
  late Directory root;
  late ProjectRepository repo;
  late Project example;

  setUp(() {
    root = Directory.systemTemp.createTempSync('loopcore_repo_');
    addTearDown(() => root.deleteSync(recursive: true));
    repo = ProjectRepository(projectsDir: Directory('${root.path}/Projects'));
    example = ProjectCodec().decode(File('test/fixtures/projects/example_v1.json').readAsStringSync()).project;
  });

  test('create → tạo .loopproj + thư mục con, load lại bằng nhau', () async {
    final dir = await repo.create(example);
    expect(dir, endsWith('/Projects/My Jam.loopproj'));
    for (final sub in ['audio', 'instruments', 'cache']) {
      expect(Directory('$dir/$sub').existsSync(), isTrue);
    }
    final loaded = await repo.load(dir);
    expect(loaded.project, example);
    expect(loaded.recoveredFromBackup, isFalse);
    expect((await repo.list()).map((d) => d.path), [dir]);
  });

  test('tên trùng → thêm số', () async {
    final a = await repo.create(example);
    final b = await repo.create(example);
    expect(a, isNot(b));
    expect(b, endsWith('My Jam 2.loopproj'));
  });

  test('save lần 2 giữ bản trước ở .bak, không còn .tmp', () async {
    final dir = await repo.create(example);
    await repo.save(example.copyWith(name: 'Bản 2'), dir);
    expect((await repo.load(dir)).project.name, 'Bản 2');
    final bak = ProjectCodec().decode(File('$dir/project.json.bak').readAsStringSync()).project;
    expect(bak.name, 'My Jam');
    expect(File('$dir/project.json.tmp').existsSync(), isFalse);
  });

  test('project.json hỏng (chết giữa lúc ghi) → mở bằng .bak', () async {
    final dir = await repo.create(example);
    await repo.save(example.copyWith(name: 'Mới'), dir);
    File('$dir/project.json').writeAsStringSync('{"schemaVersion": 1, "name": "cụt');
    final loaded = await repo.load(dir);
    expect(loaded.recoveredFromBackup, isTrue);
    expect(loaded.project.name, 'My Jam');
  });

  test('chết giữa 2 lần rename (không còn project.json) → mở bằng .bak', () async {
    final dir = await repo.create(example);
    await repo.save(example, dir);
    File('$dir/project.json').renameSync('$dir/project.json.bak');
    final loaded = await repo.load(dir);
    expect(loaded.recoveredFromBackup, isTrue);
    expect(loaded.project, example);
  });

  test('hỏng cả hai → ProjectLoadException', () async {
    final dir = await repo.create(example);
    File('$dir/project.json').writeAsStringSync('rác');
    expect(() => repo.load(dir), throwsA(isA<ProjectLoadException>()));
  });

  test('file audio thiếu → missingClipIds, không crash', () async {
    final dir = await repo.create(example);
    expect((await repo.load(dir)).missingClipIds, {'c_3f…'});
    File('$dir/audio/c_3f….caf').writeAsBytesSync([0]);
    expect((await repo.load(dir)).missingClipIds, isEmpty);
  });
}
