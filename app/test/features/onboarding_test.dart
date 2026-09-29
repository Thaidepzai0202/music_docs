// P4-14: onboarding lần đầu — vì sao cần micro → xin quyền → project demo → Session; cờ "đã xem" trong settings.json.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/router.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/data/data_providers.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/session/session_screen.dart';
import 'package:music_looper/features/settings/app_settings.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;
  late MemoryProjectRepository repo;
  late MemorySettingsRepository settings;
  late ProviderContainer c;

  setUp(() {
    fake = createFakeEngine();
    repo = MemoryProjectRepository();
    settings = MemorySettingsRepository();
  });

  Future<void> pumpOnboarding(WidgetTester tester) async {
    useIpad8Screen(tester);
    c = ProviderContainer(
      overrides: [
        ...engineOverrides(fake, settings: settings),
        projectRepositoryProvider.overrideWithValue(repo),
      ],
    );
    addTearDown(c.dispose);
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: MaterialApp(
          theme: buildAppTheme(),
          initialRoute: AppRoutes.onboarding,
          onGenerateRoute: AppRoutes.onGenerateRoute,
        ),
      ),
    );
    await tester.pump();
  }

  Future<void> settle(WidgetTester tester) async {
    for (var i = 0; i < 5; i++) {
      await tester.pump(const Duration(milliseconds: 200));
    }
  }

  Future<void> unmount(WidgetTester tester) async {
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpWidget(const SizedBox());
  }

  testWidgets('cho phép micro → audio chạy → mở project demo → vào Session; quay lại thấy demo trong Projects', (
    tester,
  ) async {
    final platform = mockEnginePlatform(permission: 'undetermined');
    await pumpOnboarding(tester);
    expect(find.byKey(const Key('onboarding.mic')), findsOneWidget);
    expect(find.textContaining('Thu giọng hát'), findsOneWidget);

    await tester.tap(find.byKey(const Key('onboarding.allowMic')));
    await settle(tester);
    expect(platform, contains('requestMicPermission'));
    expect(fake.audioRunning, isTrue, reason: 'có quyền mic → le_audio_start');
    expect(find.byKey(const Key('onboarding.demo')), findsOneWidget);

    await tester.runAsync(() async {
      await tester.tap(find.byKey(const Key('onboarding.openDemo')));
      await Future<void>.delayed(const Duration(milliseconds: 50)); // đọc asset demo
    });
    await settle(tester);
    expect(repo.projects.values.single.name, isNotEmpty);
    expect(settings.stored?.onboardingDone, isTrue);
    expect(find.byType(SessionScreen), findsOneWidget);
    expect(c.read(projectControllerProvider)?.project.tracks, isNotEmpty);

    await tester.tap(find.byKey(const Key('session.back')));
    await settle(tester);
    expect(find.byKey(const Key('projects.demo')), findsOneWidget, reason: 'về màn Projects, không về onboarding');
    expect(find.text(repo.projects.values.single.name), findsOneWidget);
    expect(c.read(projectControllerProvider), isNull);
    await unmount(tester);
  });

  testWidgets('từ chối micro → giải thích + "Mở Cài đặt"; vẫn đi tiếp được, "Tự tạo project" → Projects trống', (
    tester,
  ) async {
    final platform = mockEnginePlatform(permission: 'undetermined', grantOnRequest: false);
    await pumpOnboarding(tester);
    await tester.tap(find.byKey(const Key('onboarding.allowMic')));
    await settle(tester);
    expect(find.textContaining('Micro đang bị tắt'), findsOneWidget);
    expect(fake.audioRunning, isTrue, reason: 'chế độ chỉ phát (07 §4.0): demo vẫn nghe được');
    expect(fake.inputEnabled, isFalse);
    await tester.tap(find.byKey(const Key('onboarding.openSettings')));
    await settle(tester);
    expect(platform, contains('openAppSettings'));

    await tester.tap(find.byKey(const Key('onboarding.micLater')));
    await settle(tester);
    expect(find.textContaining('Chưa có quyền micro'), findsOneWidget);
    await tester.tap(find.byKey(const Key('onboarding.finish')));
    await settle(tester);
    expect(settings.stored?.onboardingDone, isTrue);
    expect(find.textContaining('Chưa có project'), findsOneWidget);
    expect(repo.projects, isEmpty);
    await unmount(tester);
  });

  testWidgets('"Bỏ qua" → ghi cờ, sang Projects; không xin quyền mic', (tester) async {
    final platform = mockEnginePlatform(permission: 'undetermined');
    await pumpOnboarding(tester);
    await tester.tap(find.byKey(const Key('onboarding.skip')));
    await settle(tester);
    expect(settings.saved.last.onboardingDone, isTrue);
    expect(find.byKey(const Key('projects.demo')), findsOneWidget);
    expect(platform, isNot(contains('requestMicPermission')));
    await unmount(tester);
  });

  test('lối vào Projects: chưa xem onboarding → onboarding; main vẫn mở spike (tới M0)', () {
    expect(AppRoutes.projectsEntry(onboardingDone: false), AppRoutes.onboarding);
    expect(AppRoutes.projectsEntry(onboardingDone: true), AppRoutes.projects);
    expect(AppRoutes.initialFor(onboardingDone: false), AppRoutes.spike);
  });

  test('settings.json: onboardingDone đi-về; file cũ không có khoá → false', () {
    const s = AppSettings(onboardingDone: true);
    expect(AppSettings.fromJson(s.toJson()), s);
    expect(AppSettings.fromJson(const {'version': 1, 'recordBars': 4}).onboardingDone, isFalse);
  });
}
