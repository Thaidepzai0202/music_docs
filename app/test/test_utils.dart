import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/misc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_bootstrap.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/data/project_repository.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/model/project.dart';

/// Engine giả đã `create`, tự dispose sau test.
FakeEngineClient createFakeEngine({bool autoCompleteJobs = true}) {
  final fake = FakeEngineClient(autoCompleteJobs: autoCompleteJobs);
  fake.create(const EngineConfig(dataDir: '/docs', libraryDir: '/bundle/Library'));
  addTearDown(fake.dispose);
  return fake;
}

/// Settings lưu trong bộ nhớ (test không ghi đĩa).
class MemorySettingsRepository extends SettingsRepository {
  MemorySettingsRepository() : super(File('/mem/settings.json'));
  final saved = <AppSettings>[];
  AppSettings? stored;

  @override
  Future<AppSettings> load() async => stored ?? const AppSettings();

  @override
  Future<void> save(AppSettings s) async {
    saved.add(s);
    stored = s;
  }
}

List<Override> engineOverrides(FakeEngineClient fake, {SettingsRepository? settings}) => [
  settingsRepositoryProvider.overrideWithValue(settings ?? MemorySettingsRepository()),
  engineBootstrapProvider.overrideWithValue(
    EngineBootstrap(engine: fake, isFake: true, createResult: LeError.LE_OK, dataDir: '/docs'),
  ),
];

/// Giả lập kênh Swift của plugin. [permission]: 'granted' | 'denied' | 'undetermined'.
/// Trả danh sách method đã gọi (theo thứ tự).
List<String> mockEnginePlatform({
  String permission = 'granted',
  bool grantOnRequest = true,
  void Function(String method)? onCall,
}) {
  final calls = <String>[];
  TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(EnginePlatform.channel, (
    MethodCall call,
  ) async {
    calls.add(call.method);
    onCall?.call(call.method);
    return switch (call.method) {
      'micPermission' => permission,
      'requestMicPermission' => grantOnRequest,
      'openAppSettings' => true,
      'bundleResourcePath' => '/bundle',
      _ => null,
    };
  });
  addTearDown(
    () => TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(
      EnginePlatform.channel,
      null,
    ),
  );
  return calls;
}

/// Màn hình iPad 8: 1080×810 pt @2x, landscape.
void useIpad8Screen(WidgetTester tester) {
  tester.view.physicalSize = const Size(2160, 1620);
  tester.view.devicePixelRatio = 2;
  addTearDown(tester.view.reset);
}

/// Repository trong bộ nhớ (không IO) cho widget test màn Projects và autosave.
class MemoryProjectRepository extends ProjectRepository {
  MemoryProjectRepository() : super(projectsDir: Directory('/mem/Projects'));

  final projects = <String, Project>{};
  final saves = <(String dir, Project project)>[];
  Object? failSaveWith;

  @override
  Future<List<Directory>> list() async => [for (final d in projects.keys) Directory(d)];

  @override
  Future<List<ProjectSummary>> summaries() async =>
      [for (final e in projects.entries) ProjectSummary(dir: e.key, name: e.value.name, modifiedAt: e.value.modifiedAt)]
        ..sort((a, b) => b.modifiedAt!.compareTo(a.modifiedAt!));

  @override
  Future<String> create(Project project) async {
    var dir = '/mem/Projects/${project.name}.loopproj';
    for (var i = 2; projects.containsKey(dir); i++) {
      dir = '/mem/Projects/${project.name} $i.loopproj';
    }
    projects[dir] = project;
    return dir;
  }

  @override
  Future<void> save(Project project, String dir) async {
    if (failSaveWith != null) throw failSaveWith!;
    saves.add((dir, project));
    projects[dir] = project;
  }

  @override
  Future<LoadedProject> load(String dir) async => LoadedProject(
    project: projects[dir]!,
    dir: dir,
    readOnly: false,
    recoveredFromBackup: false,
    missingClipIds: const {},
  );

  @override
  Future<String> rename(String dir, String newName, {DateTime? now}) async {
    final p = projects.remove(dir)!;
    final nd = '/mem/Projects/$newName.loopproj';
    projects[nd] = p.copyWith(name: newName);
    return nd;
  }

  @override
  Future<String> duplicate(String dir, {required String Function(String name) nameOf, DateTime? now}) async =>
      create(projects[dir]!.copyWith(name: nameOf(projects[dir]!.name), id: 'p_dup'));

  @override
  Future<void> delete(String dir) async => projects.remove(dir);
}
