import 'dart:convert';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_riverpod/misc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/router.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/data/data_providers.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/features/projects/demo_project.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/session/session_screen.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

import 'test_utils.dart';

Project demoProject() => ProjectCodec().decode(File('assets/demo/demo_project.json').readAsStringSync()).project;

/// Manifest thư viện thật (tên kit / nhạc cụ {en, vi}) — cho localizeDemo trong test.
LibraryManifest testLibrary() => LibraryManifest.fromJson(
  jsonDecode(File('assets/library/manifest.json').readAsStringSync()) as Map<String, dynamic>,
);

/// Màn Session với FakeEngine + repository trong bộ nhớ, project demo đã mở.
class SessionHarness {
  SessionHarness(this.tester);

  final WidgetTester tester;
  late final FakeEngineClient fake = createFakeEngine();
  late final MemoryProjectRepository repo = MemoryProjectRepository();
  late ProviderContainer c;

  /// [locale]: `vi` (mặc định) hoặc `en` — layout stress / golden chạy cả hai (07 §8).
  Future<void> pump({
    Project? project,
    List<Override> extraOverrides = const [],
    String dir = '/p',
    Locale locale = const Locale('vi'),
  }) async {
    mockEnginePlatform();
    useIpad8Screen(tester);
    c = ProviderContainer(
      overrides: [...engineOverrides(fake), projectRepositoryProvider.overrideWithValue(repo), ...extraOverrides],
    );
    addTearDown(c.dispose);
    // Như app thật: demo được tạo bằng ngôn ngữ đang chạy (tên gốc tiếng Anh trong asset → ARB).
    final p = project ?? localizeDemo(demoProject(), lookupAppLocalizations(locale), library: testLibrary());
    await tester.runAsync(() => c.read(projectControllerProvider.notifier).open(p, dir: dir));
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: MaterialApp(
          debugShowCheckedModeBanner: false,
          theme: buildAppTheme(),
          locale: locale,
          localizationsDelegates: L10n.localizationsDelegates,
          supportedLocales: L10n.supportedLocales,
          builder: (context, child) => LocaleScope(child: child!),
          home: const SessionScreen(),
          onGenerateRoute: AppRoutes.onGenerateRoute, // ⋮ → Cài đặt
        ),
      ),
    );
    await tester.pump();
  }

  ProjectSession get session => c.read(projectControllerProvider)!;

  List<FakeSend> sentSince(int index) => fake.sent.sublist(index);

  Future<void> settle() async {
    for (var i = 0; i < 3; i++) {
      await tester.pump(const Duration(milliseconds: 200));
    }
  }

  /// Cho autosave (debounce 2 s) chạy xong rồi gỡ cây — không để Timer treo.
  Future<void> unmount() async {
    await tester.pump(const Duration(seconds: 3));
    await tester.pumpWidget(const SizedBox());
    L10n.current = lookupAppLocalizations(const Locale('vi')); // test sau trong cùng file không bị ngôn ngữ cũ
  }
}
