// P2-23: màn Projects — tạo, demo, mở vào Session, đổi tên, nhân bản, xoá.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/router.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/data/data_providers.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/model/ids.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;
  late MemoryProjectRepository repo;
  late ProviderContainer c;

  setUp(() {
    fake = createFakeEngine();
    mockEnginePlatform();
    repo = MemoryProjectRepository();
  });

  Future<void> pumpProjects(WidgetTester tester) async {
    useIpad8Screen(tester);
    c = ProviderContainer(overrides: [...engineOverrides(fake), projectRepositoryProvider.overrideWithValue(repo)]);
    addTearDown(c.dispose);
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: MaterialApp(
          theme: buildAppTheme(),
          initialRoute: AppRoutes.projects,
          onGenerateRoute: AppRoutes.onGenerateRoute,
        ),
      ),
    );
    await tester.pump();
  }

  /// Chờ route chuyển + job giả hoàn thành (Ticker ở Session chạy liên tục nên không dùng pumpAndSettle).
  Future<void> settle(WidgetTester tester) async {
    for (var i = 0; i < 4; i++) {
      await tester.pump(const Duration(milliseconds: 200));
    }
  }

  Future<void> unmount(WidgetTester tester) async {
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpWidget(const SizedBox());
  }

  Future<void> chooseMenu(WidgetTester tester, String projectName, String item) async {
    await tester.tap(find.byKey(Key('projects.menu.$projectName')));
    await settle(tester);
    await tester.tap(find.text(item).last);
    await settle(tester);
  }

  testWidgets('chưa có project → hướng dẫn', (tester) async {
    await pumpProjects(tester);
    expect(find.textContaining('Chưa có project'), findsOneWidget);
    await unmount(tester);
  });

  testWidgets('Project mới → đặt tên → mở Session; quay lại → đóng project, danh sách có project', (tester) async {
    await pumpProjects(tester);
    await tester.tap(find.byKey(const Key('projects.new')));
    await settle(tester);
    await tester.enterText(find.byKey(const Key('nameDialog.field')), 'Tập tối');
    await tester.tap(find.byKey(const Key('nameDialog.ok')));
    await settle(tester);

    expect(find.byKey(const Key('session.grid')), findsOneWidget);
    expect(find.text('Tập tối'), findsOneWidget);
    expect(c.read(projectControllerProvider)!.project.tracks.length, 8);
    expect(fake.calls.first.op, 'project.open');

    await tester.tap(find.byKey(const Key('session.back')));
    await settle(tester);
    expect(c.read(projectControllerProvider), isNull);
    expect(fake.calls.last.op, 'project.close');
    expect(find.byKey(const Key('projects.tile.Tập tối')), findsOneWidget);
    await unmount(tester);
  });

  testWidgets('Tạo project demo → có trong danh sách, mở được, có clip', (tester) async {
    await pumpProjects(tester);
    await tester.tap(find.byKey(const Key('projects.demo')));
    // rootBundle đọc asset thật (IO) → cho vòng lặp thật chạy tới khi project demo được tạo.
    await tester.runAsync(() async {
      for (var i = 0; i < 60 && repo.projects.isEmpty; i++) {
        await Future<void>.delayed(const Duration(milliseconds: 50));
      }
    });
    expect(repo.projects.values.single.name, 'Demo');
    await settle(tester);
    expect(find.textContaining('Lỗi'), findsNothing);
    expect(find.byKey(const Key('projects.tile.Demo')), findsOneWidget);

    await tester.tap(find.byKey(const Key('projects.tile.Demo')));
    await settle(tester);
    expect(find.text('Beat A'), findsOneWidget);
    expect(fake.calls.where((x) => x.op == 'clip.setMidi').length, 11);
    await unmount(tester);
  });

  testWidgets('đổi tên / nhân bản / xoá qua menu', (tester) async {
    await repo.create(newProject('Cũ'));
    await pumpProjects(tester);
    expect(find.byKey(const Key('projects.tile.Cũ')), findsOneWidget);

    await chooseMenu(tester, 'Cũ', 'Đổi tên');
    await tester.enterText(find.byKey(const Key('nameDialog.field')), 'Mới');
    await tester.tap(find.byKey(const Key('nameDialog.ok')));
    await settle(tester);
    expect(find.byKey(const Key('projects.tile.Mới')), findsOneWidget);
    expect(find.byKey(const Key('projects.tile.Cũ')), findsNothing);

    await chooseMenu(tester, 'Mới', 'Nhân bản');
    expect(find.byKey(const Key('projects.tile.Mới (bản sao)')), findsOneWidget);

    await chooseMenu(tester, 'Mới', 'Xoá');
    expect(find.textContaining('Không hoàn tác'), findsOneWidget);
    await tester.tap(find.byKey(const Key('projects.confirmDelete')));
    await settle(tester);
    expect(find.byKey(const Key('projects.tile.Mới')), findsNothing);
    expect(repo.projects.length, 1);
    await unmount(tester);
  });
}
