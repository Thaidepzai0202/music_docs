import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  EngineEvent ev(int type, {int a = 0, int b = 0, int jobId = 0, double value = 0}) =>
      EngineEvent.fromNative(type, a, b, jobId, value);

  test('map đủ mọi LeEventType sang lớp sealed', () {
    expect(
      ev(LeEventType.LE_EVT_RECORDING_FINISHED, a: 2, b: 6),
      isA<RecordingFinished>().having((e) => e.track, 'track', 2).having((e) => e.slot, 'slot', 6),
    );
    expect(
      ev(LeEventType.LE_EVT_JOB_PROGRESS, jobId: 5, value: 0.3),
      isA<JobProgress>().having((e) => e.progress, 'progress', 0.3),
    );
    expect(ev(LeEventType.LE_EVT_JOB_DONE, jobId: 5), isA<JobDone>().having((e) => e.jobId, 'jobId', 5));
    expect(
      ev(LeEventType.LE_EVT_JOB_FAILED, jobId: 5, a: LeError.LE_ERR_FILE_FORMAT),
      isA<JobFailed>().having((e) => e.errorCode, 'errorCode', LeError.LE_ERR_FILE_FORMAT),
    );
    expect(ev(LeEventType.LE_EVT_XRUN, a: 3), isA<XrunOccurred>().having((e) => e.totalXruns, 'n', 3));
    expect(
      ev(LeEventType.LE_EVT_AUDIO_INTERRUPTED, a: 1),
      isA<AudioInterrupted>().having((e) => e.began, 'began', true),
    );
    expect(
      ev(LeEventType.LE_EVT_AUDIO_INTERRUPTED, a: 0),
      isA<AudioInterrupted>().having((e) => e.began, 'began', false),
    );
    expect(
      ev(LeEventType.LE_EVT_ROUTE_CHANGED, a: 0, b: 1),
      isA<RouteChanged>().having((e) => e.bluetooth, 'bt', true).having((e) => e.wiredOrInterface, 'wired', false),
    );
    expect(ev(LeEventType.LE_EVT_TEMPO_CHANGED, value: 101.5), isA<TempoChanged>().having((e) => e.bpm, 'bpm', 101.5));
    expect(ev(LeEventType.LE_EVT_LINK_PEERS, a: 2), isA<LinkPeersChanged>().having((e) => e.peers, 'peers', 2));
    expect(ev(LeEventType.LE_EVT_MIDI_DEVICES), isA<MidiDevicesChanged>());
    expect(
      ev(LeEventType.LE_EVT_MIDI_LEARNED, a: 1, b: 74),
      isA<MidiLearned>().having((e) => e.isCc, 'cc', true).having((e) => e.number, 'n', 74),
    );
    expect(
      ev(LeEventType.LE_EVT_ERROR, a: LeError.LE_ERR_DISK_FULL),
      isA<EngineErrorEvent>().having((e) => e.errorCode, 'code', LeError.LE_ERR_DISK_FULL),
    );
    expect(
      ev(LeEventType.LE_EVT_MEMORY_WARNING, value: 512),
      isA<MemoryWarning>().having((e) => e.megabytes, 'mb', 512),
    );
    expect(ev(999, a: 1), isA<UnknownEngineEvent>().having((e) => e.type, 'type', 999));
  });

  test('leErrorName', () {
    expect(leErrorName(LeError.LE_ERR_MIC_PERMISSION), 'MIC_PERMISSION');
    expect(leErrorName(-12345), 'ERROR(-12345)');
  });
}
