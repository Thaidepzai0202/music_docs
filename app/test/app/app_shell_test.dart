// P2-03: router, vòng đời engine theo AppLifecycle. P2-02: Ticker chỉ chạy khi Session hiển thị.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/engine_lifecycle_scope.dart';
import 'package:music_looper/app/router.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/projects/projects_screen.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;

  setUp(() {
    fake = createFakeEngine();
    mockEnginePlatform();
  });

  Future<ProviderContainer> pumpShell(WidgetTester tester, {String initialRoute = AppRoutes.projects}) async {
    useIpad8Screen(tester);
    await tester.pumpWidget(
      ProviderScope(
        overrides: engineOverrides(fake),
        child: EngineLifecycleScope(
          child: MaterialApp(
            theme: buildAppTheme(),
            initialRoute: initialRoute,
            onGenerateRoute: AppRoutes.onGenerateRoute,
          ),
        ),
      ),
    );
    return ProviderScope.containerOf(tester.element(find.byType(MaterialApp)));
  }

  Future<void> goThrough(WidgetTester tester, List<AppLifecycleState> states) async {
    for (final s in states) {
      tester.binding.handleAppLifecycleStateChanged(s);
    }
    await tester.pump();
  }

  const toBackground = [AppLifecycleState.inactive, AppLifecycleState.hidden, AppLifecycleState.paused];
  const toForeground = [AppLifecycleState.hidden, AppLifecycleState.inactive, AppLifecycleState.resumed];

  testWidgets('router: Projects → Session, Ticker chỉ chạy ở Session', (tester) async {
    final c = await pumpShell(tester);
    final ticker = c.read(engineStateTickerProvider);
    expect(find.byType(ProjectsScreen), findsOneWidget);
    expect(ticker.isActive, isFalse);

    await tester.tap(find.byKey(const Key('projects.openSession')));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 600));
    expect(find.text('Chưa mở project'), findsOneWidget);
    expect(ticker.isActive, isTrue);

    tester.state<NavigatorState>(find.byType(Navigator)).pop();
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 600));
    expect(ticker.isActive, isFalse);
  });

  testWidgets('route không tồn tại → null (Navigator báo lỗi thay vì màn trống)', (tester) async {
    expect(AppRoutes.onGenerateRoute(const RouteSettings(name: '/khong-co')), isNull);
  });

  group('vòng đời engine', () {
    testWidgets('xuống nền khi không phát → le_audio_stop; lên lại → start', (tester) async {
      final c = await pumpShell(tester);
      await c.read(engineAudioProvider).start();
      expect(fake.audioStartCount, 1);

      await goThrough(tester, toBackground);
      expect(fake.audioStopCount, 1);
      expect(fake.audioRunning, isFalse);

      await goThrough(tester, toForeground);
      expect(fake.audioStartCount, 2);
      expect(fake.audioRunning, isTrue);
    });

    testWidgets('đang phát → giữ audio khi xuống nền', (tester) async {
      final c = await pumpShell(tester);
      await c.read(engineAudioProvider).start();
      fake.state.playing = 1;
      await goThrough(tester, toBackground);
      expect(fake.audioStopCount, 0);
      expect(fake.audioRunning, isTrue);
    });

    testWidgets('đang thu → giữ audio khi xuống nền', (tester) async {
      final c = await pumpShell(tester);
      await c.read(engineAudioProvider).start();
      fake.state.anyRecording = 1;
      await goThrough(tester, toBackground);
      expect(fake.audioRunning, isTrue);
    });

    testWidgets('chỉ inactive (kéo Control Center) → không tắt', (tester) async {
      final c = await pumpShell(tester);
      await c.read(engineAudioProvider).start();
      await goThrough(tester, const [AppLifecycleState.inactive, AppLifecycleState.resumed]);
      expect(fake.audioStopCount, 0);
    });
  });
}
