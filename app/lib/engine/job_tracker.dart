import 'dart:async';

import 'package:engine_ffi/engine_ffi.dart';

/// Job thất bại (`JOB_FAILED`, hoặc `job.result` báo failed).
final class EngineJobException implements Exception {
  const EngineJobException(this.jobId, this.code, [this.message = '']);
  final int jobId;

  /// Tên mã lỗi, ví dụ `FILE_FORMAT`, `JOB_CANCELLED`.
  final String code;
  final String message;
  @override
  String toString() => 'EngineJobException(job $jobId: $code${message.isEmpty ? '' : ' — $message'})';
}

/// Biến `jobId` thành `Future` (05 §4 `JobHandle`): hoàn thành khi có `JOB_DONE`
/// (tự gọi `job.result`), báo lỗi khi `JOB_FAILED`.
class JobTracker {
  JobTracker(this._engine) {
    _sub = _engine.events.listen(_onEvent);
  }

  final EngineApi _engine;
  late final StreamSubscription<EngineEvent> _sub;
  final _waiters = <int, Completer<Map<String, dynamic>>>{};

  /// Event xong/lỗi tới TRƯỚC khi có ai gọi [awaitJob] (giữ tối đa [_maxEarly] job).
  final _early = <int, EngineEvent>{};
  static const _maxEarly = 64;
  final _progress = StreamController<JobProgress>.broadcast();

  /// Tiến độ 0..1 của mọi job.
  Stream<JobProgress> get progress => _progress.stream;

  /// Kết quả (`result` của `job.result`) của [jobId], hoặc lỗi [EngineJobException].
  Future<Map<String, dynamic>> awaitJob(int jobId) {
    final early = _early.remove(jobId);
    if (early != null) {
      final c = Completer<Map<String, dynamic>>();
      _complete(c, early);
      return c.future;
    }
    return (_waiters[jobId] ??= Completer<Map<String, dynamic>>()).future;
  }

  /// Đợi nhiều job; [onProgress] được gọi mỗi khi thêm 1 job xong (để hiện "7/12").
  /// Lỗi của job đầu tiên hỏng được ném ra sau khi mọi job đã kết thúc.
  Future<List<Map<String, dynamic>>> awaitAll(
    List<int> jobIds, {
    void Function(int done, int total)? onProgress,
  }) async {
    var done = 0;
    onProgress?.call(0, jobIds.length);
    return Future.wait(
      jobIds.map((id) => awaitJob(id).whenComplete(() => onProgress?.call(++done, jobIds.length))),
      eagerError: false,
    );
  }

  void _onEvent(EngineEvent e) {
    switch (e) {
      case JobProgress():
        _progress.add(e);
      case JobDone(:final jobId) || JobFailed(:final jobId):
        final c = _waiters.remove(jobId);
        if (c != null) {
          _complete(c, e);
        } else {
          _early[jobId] = e;
          if (_early.length > _maxEarly) _early.remove(_early.keys.first);
        }
      default:
        break;
    }
  }

  void _complete(Completer<Map<String, dynamic>> c, EngineEvent e) {
    switch (e) {
      case JobFailed(:final jobId, :final errorCode):
        c.completeError(EngineJobException(jobId, leErrorName(errorCode)));
      case JobDone(:final jobId):
        try {
          final r = _engine.callOk('job.result', {'jobId': jobId});
          switch (r['status']) {
            case 'done':
              c.complete((r['result'] as Map?)?.cast<String, dynamic>() ?? const {});
            case 'failed':
              final err = (r['error'] as Map?) ?? const {};
              c.completeError(EngineJobException(jobId, '${err['code'] ?? 'INTERNAL'}', '${err['message'] ?? ''}'));
            default:
              c.completeError(EngineJobException(jobId, 'INTERNAL', 'JOB_DONE nhưng status = ${r['status']}'));
          }
        } on EngineCallException catch (ex) {
          c.completeError(EngineJobException(jobId, ex.code, ex.message));
        }
      default:
        break;
    }
  }

  void dispose() {
    _sub.cancel();
    for (final entry in _waiters.entries) {
      entry.value.completeError(EngineJobException(entry.key, 'JOB_CANCELLED', 'JobTracker đã dispose'));
    }
    _waiters.clear();
    _progress.close();
  }
}
