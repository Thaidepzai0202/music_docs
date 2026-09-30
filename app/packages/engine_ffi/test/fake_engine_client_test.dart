import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  late FakeEngineClient fake;

  setUp(() => fake = FakeEngineClient());
  tearDown(() => fake.dispose());

  test('ghi lại send và call theo đúng thứ tự', () {
    fake.send(LeCommandType.LE_CMD_SET_BPM, d0: 120);
    fake.call({'op': 'transport.setTimeSignature', 'num': 4, 'den': 4});
    fake.send(LeCommandType.LE_CMD_TRACK_GAIN, track: 1, f0: -3);
    expect(fake.log.map((e) => e.toJson()).toList(), [
      {'send': 'SET_BPM', 'd0': 120.0},
      {
        'call': {'op': 'transport.setTimeSignature', 'num': 4, 'den': 4},
      },
      {'send': 'TRACK_GAIN', 'track': 1, 'f0': -3.0},
    ]);
  });

  test(
    'preview.play: base "project" khi chưa project.open → INVALID_ARG; thiếu file → FILE_NOT_FOUND (như engine)',
    () {
      fake.create(const EngineConfig(dataDir: '/tmp', libraryDir: '/lib'));
      fake.addSample('/lib/loops/a.wav', pitched: false);
      Map<String, dynamic> play(Map<String, Object> source) => fake.call({'op': 'preview.play', 'source': source});
      String? code(Map<String, dynamic> r) => (r['error'] as Map?)?['code'] as String?;
      expect(play({'kind': 'audio', 'file': 'loops/a.wav'})['ok'], isTrue);
      expect(code(play({'kind': 'audio', 'file': 'x.wav', 'base': 'project'})), 'INVALID_ARG');
      fake.call({'op': 'project.open', 'dir': '/p'});
      expect(code(play({'kind': 'audio', 'file': 'x.wav', 'base': 'project'})), 'FILE_NOT_FOUND');
      expect(code(play({'kind': 'audio', 'file': 'loops/b.wav'})), 'FILE_NOT_FOUND');
    },
  );

  test('engine.info báo kích thước struct thật', () {
    final info = fake.callOk('engine.info');
    expect(info['apiVersion'], 1);
    expect(info['stateSize'], 248);
    expect(info['commandSize'], 32);
  });

  test('op job trả jobId, tự phát JobDone, job.result trả kết quả', () async {
    final done = fake.events.firstWhere((e) => e is JobDone);
    final id = fake.callJob('spike.latencyLoopback');
    expect((await done as JobDone).jobId, id);
    final r = fake.callOk('job.result', {'jobId': id});
    expect(r['status'], 'done');
    expect((r['result'] as Map)['measuredSamples'], 492);
  });

  test('failingJobOps → JobFailed', () async {
    fake.failingJobOps.add('clip.setAudio');
    final failed = fake.events.firstWhere((e) => e is JobFailed);
    final id = fake.callJob('clip.setAudio', {'track': 0});
    expect((await failed as JobFailed).jobId, id);
    expect(fake.callOk('job.result', {'jobId': id})['status'], 'failed');
  });

  test('spike.setBufferSize chỉ nhận 64|128|256|512|1024', () {
    fake.create(const EngineConfig(dataDir: '/d', libraryDir: '/l'));
    expect(fake.callOk('spike.setBufferSize', {'frames': 256}), {'bufferSize': 256});
    expect(fake.readState().bufferSize, 256);
    expect(fake.call({'op': 'spike.setBufferSize', 'frames': 100})['ok'], false);
  });

  test('simulate: meter chỉ chạy khi audio đang chạy', () {
    final sim = FakeEngineClient(simulate: true);
    addTearDown(sim.dispose);
    sim.create(const EngineConfig(dataDir: '/d', libraryDir: '/l'));
    sim.send(LeCommandType.LE_CMD_SPIKE_SINE, f0: 440, f1: 0.8);
    expect(sim.readState().masterPeak[0], 0);
    sim.audioStart();
    expect(sim.readState().masterPeak[0], greaterThan(0.5));
    expect(sim.readState().cpuLoad, greaterThan(0));
  });

  test('FX (P3-16): fx.set lưu slot; FX_PARAM/FX_BYPASS cập nhật slot có FX; param id lạ → INVALID_ARG', () {
    fake.create(const EngineConfig(dataDir: '/tmp', libraryDir: '/tmp'));
    expect(
      fake.callOk('fx.set', {
        'track': 2,
        'index': 1,
        'type': 'filter',
        'params': {'1': 800},
        'bypass': false,
      }),
      {},
    );
    expect(fake.send(LeCommandType.LE_CMD_FX_PARAM, track: 2, slot: 1, i0: 1, f0: 1200), isTrue);
    expect(fake.send(LeCommandType.LE_CMD_FX_BYPASS, track: 2, slot: 1, i0: 1), isTrue);
    expect(fake.fx[(2, 1)]!.params['1'], 1200);
    expect(fake.fx[(2, 1)]!.bypass, isTrue);
    expect(fake.send(LeCommandType.LE_CMD_FX_PARAM, track: 2, slot: 3, i0: 1, f0: 1), isFalse);
    final bad = fake.call({
      'op': 'fx.set',
      'track': 0,
      'index': 0,
      'type': 'filter',
      'params': {'7': 1},
    });
    expect((bad['error'] as Map)['code'], 'INVALID_ARG');
  });

  test('manualJobOps: job không tự xong; lastJobId trỏ job vừa cấp', () async {
    fake.create(const EngineConfig(dataDir: '/tmp', libraryDir: '/tmp'));
    fake.manualJobOps.add('export.scene');
    final id = fake.callJob('export.scene', {'scene': 0, 'bars': 1, 'path': '/tmp/x.wav', 'format': 'wav'});
    expect(fake.lastJobId, id);
    await Future<void>.delayed(const Duration(milliseconds: 5));
    expect(fake.callOk('job.result', {'jobId': id})['status'], 'running');
    fake.completeJob(id);
    expect(fake.callOk('job.result', {'jobId': id})['status'], 'done');
  });
}
