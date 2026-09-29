// P2-25: autosave debounce 2 s, lưu ngay khi thu xong, khi xuống nền, khi đóng project.
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/engine_lifecycle_scope.dart';
import 'package:music_looper/data/data_providers.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

import '../test_utils.dart';

Project fixture() => ProjectCodec().decode(File('test/fixtures/projects/example_v1.json').readAsStringSync()).project;

void main() {
  late FakeEngineClient fake;
  late MemoryProjectRepository repo;
  late ProviderContainer c;

  setUp(() {
    fake = createFakeEngine();
    mockEnginePlatform();
    repo = MemoryProjectRepository();
  });

  /// App tối giản có EngineLifecycleScope (nơi tạo autosave) + project đã mở.
  Future<ProjectController> pumpApp(WidgetTester tester, {bool readOnly = false}) async {
    c = ProviderContainer(overrides: [...engineOverrides(fake), projectRepositoryProvider.overrideWithValue(repo)]);
    addTearDown(c.dispose);
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: const EngineLifecycleScope(child: SizedBox()),
      ),
    );
    final ctl = c.read(projectControllerProvider.notifier);
    await tester.runAsync(() => ctl.open(fixture(), dir: '/mem/p', readOnly: readOnly));
    return ctl;
  }

  testWidgets('mở project không tự lưu', (tester) async {
    await pumpApp(tester);
    await tester.pump(const Duration(seconds: 5));
    expect(repo.saves, isEmpty);
  });

  testWidgets('debounce 2 s: nhiều thay đổi liên tiếp → 1 lần lưu, modifiedAt mới', (tester) async {
    final ctl = await pumpApp(tester);
    ctl.setBpm(100);
    await tester.pump(const Duration(milliseconds: 1500));
    ctl.setBpm(101);
    await tester.pump(const Duration(milliseconds: 1900));
    expect(repo.saves, isEmpty, reason: 'chưa đủ 2 s kể từ thay đổi cuối');
    await tester.pump(const Duration(milliseconds: 200));
    expect(repo.saves.length, 1);
    expect(repo.saves.single.$1, '/mem/p');
    expect(repo.saves.single.$2.transport.bpm, 101);
    expect(repo.saves.single.$2.modifiedAt, isNot(fixture().modifiedAt), reason: 'autosave ghi thời điểm lưu');
  });

  testWidgets('chỉ đổi arm (không lưu vào file) → không lưu', (tester) async {
    final ctl = await pumpApp(tester);
    ctl.setArm(0, true);
    await tester.pump(const Duration(seconds: 3));
    expect(repo.saves, isEmpty);
  });

  testWidgets('project chỉ đọc → không lưu', (tester) async {
    final ctl = await pumpApp(tester, readOnly: true);
    ctl.setBpm(90);
    await tester.pump(const Duration(seconds: 3));
    expect(repo.saves, isEmpty);
  });

  testWidgets('thu xong → lưu ngay, không đợi 2 s', (tester) async {
    final ctl = await pumpApp(tester);
    ctl.setArm(1, true);
    fake.send(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 1, i0: 1);
    fake.advanceBeats(8.1); // fixture có count-in 1 bar: thu từ beat 4 tới beat 8
    await tester.pump(); // event → controller thêm clip → autosave lưu
    expect(repo.saves.length, 1);
    expect(repo.saves.single.$2.trackAt(1)!.clipAt(1), isA<AudioClip>());
  });

  testWidgets('xuống nền → lưu ngay thay đổi đang chờ', (tester) async {
    final ctl = await pumpApp(tester);
    ctl.setBpm(95);
    for (final s in const [AppLifecycleState.inactive, AppLifecycleState.hidden, AppLifecycleState.paused]) {
      tester.binding.handleAppLifecycleStateChanged(s);
    }
    await tester.pump();
    expect(repo.saves.single.$2.transport.bpm, 95);
    await tester.pump(const Duration(seconds: 3));
    expect(repo.saves.length, 1, reason: 'không lưu lần 2 khi hết debounce');
  });

  testWidgets('đóng project còn thay đổi chờ → lưu nốt', (tester) async {
    final ctl = await pumpApp(tester);
    ctl.setBpm(88);
    ctl.close();
    await tester.pump();
    expect(repo.saves.single.$2.transport.bpm, 88);
  });

  testWidgets('lưu lỗi → lastError giữ mã lỗi (không giữ message hệ thống), lần sau thành công thì xoá', (
    tester,
  ) async {
    final ctl = await pumpApp(tester);
    repo.failSaveWith = const FileSystemException('write failed', '/p/project.json.tmp', OSError('No space', 28));
    ctl.setBpm(99);
    await tester.pump(const Duration(seconds: 3));
    final autosave = c.read(autosaveProvider);
    expect(autosave.lastError.value, 'DISK_FULL');
    repo.failSaveWith = const FileSystemException('read-only');
    ctl.setBpm(97);
    await tester.pump(const Duration(seconds: 3));
    expect(autosave.lastError.value, 'FILE_WRITE');
    repo.failSaveWith = null;
    ctl.setBpm(98);
    await tester.pump(const Duration(seconds: 3));
    expect(autosave.lastError.value, isNull);
  });
}
