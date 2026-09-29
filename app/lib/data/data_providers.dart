import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../engine/engine_providers.dart';
import '../features/session/project_controller.dart';
import 'autosave.dart';
import 'project_repository.dart';

/// `<Documents>/Projects` (06 §1).
final projectRepositoryProvider = Provider<ProjectRepository>(
  (ref) => ProjectRepository(projectsDir: Directory('${ref.watch(engineBootstrapProvider).dataDir}/Projects')),
);

/// Sống suốt vòng đời app (EngineLifecycleScope đọc nó lúc khởi động).
final autosaveProvider = Provider<Autosave>((ref) {
  final a = Autosave(save: ref.watch(projectRepositoryProvider).save);
  ref.listen<ProjectSession?>(projectControllerProvider, (prev, next) => a.onSessionChanged(prev, next));
  final sub = ref.watch(engineProvider).events.listen((e) {
    if (e is RecordingFinished && e.track >= 0) a.onRecordingFinished();
  });
  ref.onDispose(() {
    sub.cancel();
    a.dispose();
  });
  return a;
});
