import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../engine/engine_providers.dart';
import '../../engine/job_tracker.dart';
import '../../engine/project_replay.dart';
import '../../model/project.dart';

/// Tiến độ mở project: "Đang mở project… 7/12" (06 §6).
@immutable
final class OpenProgress {
  const OpenProgress(this.done, this.total);
  final int done;
  final int total;

  @override
  bool operator ==(Object other) => other is OpenProgress && other.done == done && other.total == total;
  @override
  int get hashCode => Object.hash(done, total);
  @override
  String toString() => 'OpenProgress($done/$total)';
}

@immutable
final class ProjectSession {
  const ProjectSession({
    required this.project,
    required this.dir,
    this.readOnly = false,
    this.progress,
    this.errors = const [],
  });

  final Project project;

  /// Thư mục `.loopproj` (engine ghi audio thu âm vào đây).
  final String dir;
  final bool readOnly;

  /// Khác null khi đang đợi job lúc mở project.
  final OpenProgress? progress;

  /// Lỗi khi dựng engine — hiện dạng banner, không chặn app (07 §4.3).
  final List<String> errors;

  bool get isLoading => progress != null;

  ProjectSession copyWith({
    Project? project,
    OpenProgress? progress,
    bool clearProgress = false,
    List<String>? errors,
  }) => ProjectSession(
    project: project ?? this.project,
    dir: dir,
    readOnly: readOnly,
    progress: clearProgress ? null : (progress ?? this.progress),
    errors: errors ?? this.errors,
  );
}

/// Mọi thay đổi project đi qua đây (07 §5): gửi lệnh engine TRƯỚC, rồi cập nhật model bất biến.
/// Autosave (P2-25) sẽ móc vào sau mỗi thay đổi.
class ProjectController extends Notifier<ProjectSession?> {
  int _generation = 0;

  EngineApi get _engine => ref.read(engineProvider);
  JobTracker get _jobs => ref.read(jobTrackerProvider);

  @override
  ProjectSession? build() => null;

  /// Phát lại chuỗi lệnh 06 §6 rồi đợi mọi job. Mở project khác giữa chừng → kết quả cũ bị bỏ.
  Future<void> open(Project project, {required String dir, bool readOnly = false}) async {
    final gen = ++_generation;
    final jobs = _jobs; // tạo tracker trước khi job kịp phát event
    state = ProjectSession(project: project, dir: dir, readOnly: readOnly, progress: const OpenProgress(0, 0));

    final replay = ProjectReplay(_engine).run(project, dir: dir);
    final errors = [...replay.errors];
    final total = replay.jobIds.length;
    var done = 0;
    state = state!.copyWith(progress: OpenProgress(0, total), errors: List.unmodifiable(errors));

    await Future.wait(
      replay.jobIds.map(
        (id) => jobs
            .awaitJob(id)
            .then<void>(
              (_) {},
              onError: (Object e) {
                errors.add(e is EngineJobException ? 'job ${e.jobId}: ${e.code} ${e.message}'.trim() : '$e');
              },
            )
            .whenComplete(() {
              done++;
              if (gen == _generation) state = state!.copyWith(progress: OpenProgress(done, total));
            }),
      ),
    );
    if (gen != _generation) return;
    state = state!.copyWith(clearProgress: true, errors: List.unmodifiable(errors));
  }

  void close() {
    if (state == null) return;
    _generation++;
    _engine.call({'op': 'project.close'});
    state = null;
  }

  /// Ví dụ mẫu cho các thao tác P2 sau: lệnh RT trước, model sau.
  void setBpm(double bpm) {
    final s = state;
    if (s == null || s.readOnly) return;
    final v = bpm.clamp(20.0, 300.0);
    _engine.send(LeCommandType.LE_CMD_SET_BPM, d0: v);
    state = s.copyWith(
      project: s.project.copyWith(transport: s.project.transport.copyWith(bpm: v)),
    );
  }
}

final projectControllerProvider = NotifierProvider<ProjectController, ProjectSession?>(ProjectController.new);
