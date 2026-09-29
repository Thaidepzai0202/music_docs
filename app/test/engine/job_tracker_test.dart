// P2-01 DoD: JobTracker.awaitJob(jobId) trả kết quả hoặc lỗi đúng.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/job_tracker.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;
  late JobTracker tracker;

  setUp(() {
    fake = createFakeEngine(autoCompleteJobs: false);
    tracker = JobTracker(fake);
    addTearDown(tracker.dispose);
  });

  test('JOB_DONE → gọi job.result, trả result', () async {
    final id = fake.callJob('clip.setAudio', {'track': 1});
    final f = tracker.awaitJob(id);
    fake.completeJob(id, result: {'frames': 96000});
    expect(await f, {'frames': 96000});
    expect(fake.calls.last.request, {'op': 'job.result', 'jobId': id});
  });

  test('JOB_FAILED → EngineJobException với tên mã lỗi', () async {
    final id = fake.callJob('clip.setAudio');
    final f = tracker.awaitJob(id);
    fake.failJob(id, errorCode: LeError.LE_ERR_FILE_FORMAT);
    await expectLater(
      f,
      throwsA(isA<EngineJobException>().having((e) => e.code, 'code', 'FILE_FORMAT').having((e) => e.jobId, 'id', id)),
    );
  });

  test('JOB_DONE nhưng job.result báo failed → lỗi kèm message', () async {
    final id = fake.callJob('clip.setAudio');
    fake.onCall = (r) => r['op'] == 'job.result'
        ? {
            'ok': true,
            'result': {
              'status': 'failed',
              'error': {'code': 'DISK_FULL', 'message': 'hết chỗ'},
            },
          }
        : null;
    final f = tracker.awaitJob(id);
    fake.emit(JobDone(jobId: id));
    await expectLater(
      f,
      throwsA(
        isA<EngineJobException>().having((e) => e.code, 'code', 'DISK_FULL').having((e) => e.message, 'm', 'hết chỗ'),
      ),
    );
  });

  test('job.result trả JOB_NOT_FOUND (ok:false) → lỗi', () async {
    fake.emit(const JobDone(jobId: 999));
    await expectLater(
      tracker.awaitJob(999),
      throwsA(isA<EngineJobException>().having((e) => e.code, 'code', 'JOB_NOT_FOUND')),
    );
  });

  test('event tới TRƯỚC khi awaitJob vẫn không mất', () async {
    final id = fake.callJob('track.setInstrument');
    fake.completeJob(id, result: {'ok': 1});
    await Future<void>.delayed(Duration.zero); // cho event đi qua stream trước
    expect(await tracker.awaitJob(id), {'ok': 1});
  });

  test('progress stream', () async {
    final got = tracker.progress.first;
    fake.emit(const JobProgress(jobId: 7, progress: 0.5));
    final p = await got;
    expect(p.jobId, 7);
    expect(p.progress, 0.5);
  });

  test('awaitAll báo tiến độ và ném lỗi của job hỏng sau khi mọi job xong', () async {
    final a = fake.callJob('clip.setAudio');
    final b = fake.callJob('clip.setAudio');
    final c = fake.callJob('clip.setAudio');
    final ticks = <String>[];
    final all = tracker.awaitAll([a, b, c], onProgress: (d, t) => ticks.add('$d/$t'));
    fake
      ..completeJob(a)
      ..failJob(b)
      ..completeJob(c);
    await expectLater(all, throwsA(isA<EngineJobException>()));
    expect(ticks, ['0/3', '1/3', '2/3', '3/3']);
  });

  test('dispose → job đang đợi bị huỷ', () async {
    final id = fake.callJob('clip.setAudio');
    final f = tracker.awaitJob(id);
    tracker.dispose();
    await expectLater(f, throwsA(isA<EngineJobException>().having((e) => e.code, 'code', 'JOB_CANCELLED')));
    tracker = JobTracker(fake); // cho addTearDown
  });
}
