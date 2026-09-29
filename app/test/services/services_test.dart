// Tầng service (07 §5): mỗi service gửi đúng op 05 §3, đọc đúng kết quả, lỗi engine → EngineCallException.
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/job_tracker.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/services/audio_device_service.dart';
import 'package:music_looper/services/capture_service.dart';
import 'package:music_looper/services/export_service.dart';
import 'package:music_looper/services/latency_service.dart';
import 'package:music_looper/services/link_service.dart';
import 'package:music_looper/services/midi_service.dart';
import 'package:music_looper/services/peaks_service.dart';

import '../test_utils.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  late FakeEngineClient fake;
  late String tmp;
  setUp(() {
    fake = createFakeEngine();
    final d = Directory.systemTemp.createTempSync('services_');
    addTearDown(() => d.deleteSync(recursive: true));
    tmp = d.path;
  });

  Map<String, dynamic> lastCall() => fake.calls.last.request;

  group('MidiService', () {
    late MidiService midi;
    setUp(() => midi = MidiService(fake, const EnginePlatform()));

    test('listDevices → MidiDevice (có open); enableDevice nhớ cả id chưa cắm (05 §3)', () {
      final d = midi.listDevices();
      expect(d.inputs.map((x) => x.name), ['Launchpad Mini MK3', 'nanoKONTROL2']);
      expect([d.inputs[0].open, d.inputs[1].enabled, d.inputs[1].open], [true, false, false]);
      expect(d.outputs.single.id, 'usb:launchpad-mini');
      midi.enableDevice('ble:nanokontrol2', true);
      expect(lastCall(), {'op': 'midi.enableDevice', 'id': 'ble:nanokontrol2', 'enabled': true});
      midi.enableDevice('usb:chua-cam', true);
      expect(fake.midiEnabledIds['usb:chua-cam'], isTrue);
    });

    test(
      'learnStart → MIDI_LEARNED → learnResult có thiết bị + kênh thật; engine không trả → mọi thiết bị/kênh',
      () async {
        midi.learnStart({'kind': 'scene', 'slot': 1});
        expect(fake.midiLearnTarget, {'kind': 'scene', 'slot': 1});
        final ev = midi.learned.first;
        fake.simulateMidiLearn(isCc: true, number: 21, channel: 3);
        final e = await ev;
        expect(midi.learnResult(e), const MidiSource(device: 'Launchpad Mini MK3', kind: 'cc', channel: 3, number: 21));
        fake.onCall = (r) => r['op'] == 'midi.learnResult'
            ? {
                'ok': false,
                'error': {'code': 'NOT_IMPLEMENTED', 'message': ''},
              }
            : null;
        expect(midi.learnResult(e), const MidiSource(device: '', kind: 'cc', channel: -1, number: 21));
        expect(() => midi.learnStart({'kind': 'tempo'}), throwsA(isA<EngineCallException>()));
      },
    );

    test('setRecordQuantize; pairBluetooth gọi kênh Swift showBluetoothMidi', () async {
      midi.setRecordQuantize(0.25);
      expect(lastCall(), {'op': 'midi.setRecordQuantize', 'grid': 0.25});
      final platform = mockEnginePlatform();
      await midi.pairBluetooth();
      expect(platform, ['showBluetoothMidi']);
    });
  });

  test('CaptureService: start/stop/analyze/createInstrument (rootNote chỉ gửi khi có)', () {
    final c = CaptureService(fake);
    fake.audioStart(); // capture.start cần audio đang chạy (AUDIO_DEVICE)
    c.start('$tmp/source.caf', 4);
    fake.advanceSeconds(1.5);
    final r = c.stop();
    expect(r.file, '$tmp/source.caf');
    expect(r.seconds, closeTo(1.5, 1e-9));
    expect(c.analyze(r.file!), isPositive);
    c.createInstrument(instrumentId: 'i_1', file: r.file!, trimStartSample: 10, trimEndSample: 20, mode: 'classic');
    expect(lastCall().containsKey('rootNote'), isFalse);
    c.createInstrument(
      instrumentId: 'i_2',
      file: r.file!,
      trimStartSample: 10,
      trimEndSample: 20,
      rootNote: 57,
      mode: 'natural',
    );
    expect(lastCall()['rootNote'], 57);
  });

  test('ExportService: jam + export.scene', () {
    final x = ExportService(fake);
    fake.audioStart(); // export.jamStart cần audio đang chạy (AUDIO_DEVICE)
    x.jamStart('$tmp/jam.wav');
    fake.advanceSeconds(2);
    final r = x.jamStop(fallbackPath: '$tmp/jam.wav');
    expect([r.file, r.seconds], ['$tmp/jam.wav', 2.0]);
    expect(() => x.jamStop(fallbackPath: ''), throwsA(isA<EngineCallException>()));
    x.exportScene(scene: 1, bars: 2, path: '$tmp/s.m4a', format: 'm4a', stems: true);
    expect(lastCall(), {
      'op': 'export.scene',
      'scene': 1,
      'bars': 2,
      'path': '$tmp/s.m4a',
      'format': 'm4a',
      'stems': true,
    });
  });

  test('LatencyService: calibrate → parse; setOffset; L thực tế từ LeState', () async {
    final l = LatencyService(fake);
    final tracker = JobTracker(fake);
    addTearDown(tracker.dispose);
    final r = LatencyService.parse(await tracker.awaitJob(l.calibrate()))!;
    expect([r.measuredSamples, r.reportedSamples, r.offsetSamples], [492, 470, 22]);
    expect(l.roundTripSamples, 492);
    expect(l.sampleRate, 48000);
    l.setOffset(-10);
    expect(fake.latencyOffset, -10);
    expect(() => l.setOffset(999999), throwsA(isA<EngineCallException>()));
    expect(LatencyService.parse(const {'measuredSamples': 1}), isNull);
  });

  test('AudioDeviceService / PeaksService / LinkService / JobTracker.cancel', () async {
    final a = AudioDeviceService(fake);
    expect(a.info()['device'], 'FakeEngine');
    a.setBufferSize(256);
    expect(a.info()['bufferSize'], 256);
    expect(() => a.setBufferSize(100), throwsA(isA<EngineCallException>()));

    expect(PeaksService(fake).peaks('c_none', 0, 10), isNull);

    final link = LinkService(fake);
    final next = link.peersChanged.first;
    fake.emit(const LinkPeersChanged(peers: 3));
    expect(await next, 3);

    final tracker = JobTracker(fake);
    addTearDown(tracker.dispose);
    fake.manualJobOps.add('export.scene');
    final id = ExportService(fake).exportScene(scene: 0, bars: 1, path: '$tmp/x.wav', format: 'wav', stems: false);
    final done = tracker.awaitJob(id);
    tracker.cancel(id);
    expect(lastCall(), {'op': 'job.cancel', 'jobId': id});
    await expectLater(done, throwsA(isA<EngineJobException>().having((e) => e.code, 'code', 'JOB_CANCELLED')));
  });
}
