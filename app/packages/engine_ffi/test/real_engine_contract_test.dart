// Test hợp đồng Dart ↔ engine THẬT trên Mac (05 §5, P2-01 "mọi op trả envelope đúng").
//
// Cần dylib: `scripts/build_engine_mac.sh mac-debug` → build/mac-debug/libLoopCore.dylib
// (hoặc đặt LOOPCORE_DYLIB=<đường dẫn>). Không có dylib → cả file tự skip.
//
// Không bật audio device (le_audio_start) và không chờ event: event cần run loop của JUCE,
// nên job được theo dõi bằng cách hỏi `job.result`.
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';

String? _findDylib() {
  final env = Platform.environment['LOOPCORE_DYLIB'];
  if (env != null && File(env).existsSync()) return env;
  // cwd của `flutter test` = app/packages/engine_ffi
  final p = File('../../../build/mac-debug/libLoopCore.dylib').absolute.path;
  return File(p).existsSync() ? p : null;
}

/// Mọi op trong bảng 05 §3.
const allOps = [
  'engine.info',
  'spike.setBufferSize',
  'spike.latencyLoopback',
  'spike.stretchBench',
  'spike.setSessionMode',
  'spike.sessionInfo',
  'project.open',
  'project.close',
  'transport.setTimeSignature',
  'track.configure',
  'track.setInstrument',
  'clip.setAudio',
  'clip.setMidi',
  'clip.getMidi',
  'clip.info',
  'clip.clear',
  'clip.undoOverdub',
  'clip.setLoopRegion',
  'midiClip.quantize',
  'capture.start',
  'capture.stop',
  'instrument.createFromRecording',
  'instrument.setMode',
  'fx.set',
  'fx.remove',
  'export.scene',
  'export.jamStart',
  'export.jamStop',
  'latency.calibrate',
  'latency.setOffset',
  'midi.listDevices',
  'midi.enableDevice',
  'midi.learnStart',
  'midi.learnCancel',
  'midi.setMappings',
  'link.enable',
  'job.result',
  'job.cancel',
];

void expectEnvelope(Map<String, dynamic> res, {required String op}) {
  expect(res['ok'], isA<bool>(), reason: '$op: thiếu "ok"');
  if (res['ok'] == true) {
    expect(res['result'], isA<Map>(), reason: '$op: ok:true phải có "result" là object');
  } else {
    final err = res['error'];
    expect(err, isA<Map>(), reason: '$op: ok:false phải có "error"');
    expect((err as Map)['code'], isA<String>(), reason: '$op: error.code phải là chuỗi');
    expect(err['message'], isA<String>(), reason: '$op: error.message phải là chuỗi');
  }
}

void main() {
  final path = _findDylib();
  final skip = path == null ? 'Chưa có libLoopCore.dylib (scripts/build_engine_mac.sh mac-debug)' : null;

  late LoopCoreBindings b;
  late EngineClient client;
  late Directory dataDir;

  setUpAll(() {
    if (path == null) return;
    b = LoopCoreBindings(DynamicLibrary.open(path));
    client = EngineClient.withBindings(b);
    dataDir = Directory.systemTemp.createTempSync('loopcore_contract_');
  });

  tearDownAll(() {
    if (path == null) return;
    client.dispose(); // gỡ callback + le_destroy
    dataDir.deleteSync(recursive: true);
  });

  test('apiVersion lệch → LE_ERR_API_VERSION (trước khi create thật)', () {
    final rc = using((a) {
      final c = const EngineConfig(dataDir: '/tmp', libraryDir: '/tmp').toNative(a);
      c.ref.apiVersion = LE_API_VERSION + 99;
      return b.le_create(c);
    });
    expect(rc, LeError.LE_ERR_API_VERSION);
  }, skip: skip);

  test('le_create OK, gọi lần 2 → ALREADY_CREATED', () {
    expect(client.apiVersion, LE_API_VERSION);
    final cfg = EngineConfig(dataDir: dataDir.path, libraryDir: dataDir.path);
    expect(client.create(cfg), LeError.LE_OK);
    expect(client.create(cfg), LeError.LE_ERR_ALREADY_CREATED);
  }, skip: skip);

  test('engine.info khớp kích thước struct phía Dart', () {
    final info = client.callOk('engine.info');
    expect(info['apiVersion'], LE_API_VERSION);
    expect(info['stateSize'], sizeOf<LeState>());
    expect(info['commandSize'], sizeOf<LeCommand>());
    if (info.containsKey('configSize')) expect(info['configSize'], sizeOf<LeConfig>());
  }, skip: skip);

  test('mọi op trong 05 §3 trả envelope đúng dạng (tham số rỗng)', () {
    for (final op in allOps) {
      expectEnvelope(client.call({'op': op}), op: op);
    }
  }, skip: skip);

  test('op lạ và JSON hỏng → ok:false', () {
    final res = client.call({'op': 'khong.ton.tai'});
    expectEnvelope(res, op: 'khong.ton.tai');
    expect(res['ok'], isFalse);

    // Gọi thẳng C với chuỗi không phải JSON.
    final raw = using((a) {
      final p = b.le_call('{hỏng'.toNativeUtf8(allocator: a).cast());
      final s = p.cast<Utf8>().toDartString();
      b.le_free_string(p);
      return s;
    });
    final parsed = jsonDecode(raw) as Map<String, dynamic>;
    expectEnvelope(parsed, op: '(JSON hỏng)');
    expect(parsed['ok'], isFalse);
  }, skip: skip);

  test('job.result / job.cancel với jobId lạ → JOB_NOT_FOUND', () {
    for (final op in ['job.result', 'job.cancel']) {
      final res = client.call({'op': op, 'jobId': 987654});
      expect(res['ok'], isFalse, reason: op);
      expect((res['error'] as Map)['code'], 'JOB_NOT_FOUND', reason: op);
    }
  }, skip: skip);

  test('spike.latencyLoopback khi chưa bật audio → AUDIO_DEVICE (lỗi đồng bộ)', () {
    final res = client.call({'op': 'spike.latencyLoopback'});
    expect(res['ok'], isFalse);
    expect((res['error'] as Map)['code'], 'AUDIO_DEVICE');
  }, skip: skip);

  test('spike.stretchBench: thiếu semitones / saveDir tương đối → INVALID_ARG', () {
    final a = client.call({'op': 'spike.stretchBench'});
    expect((a['error'] as Map?)?['code'], 'INVALID_ARG');
    final b2 = client.call({
      'op': 'spike.stretchBench',
      'semitones': [0],
      'saveDir': 'tuong/doi',
    });
    expect((b2['error'] as Map?)?['code'], 'INVALID_ARG');
  }, skip: skip);

  test('spike.stretchBench khi chưa thu → job failed INVALID_ARG (hỏi job.result)', () async {
    final id = client.callJob('spike.stretchBench', {
      'semitones': [-12, 0, 12],
      'formant': true,
      'saveDir': '${dataDir.path}/spike',
    });
    Map<String, dynamic> r = const {};
    for (var i = 0; i < 250; i++) {
      r = client.callOk('job.result', {'jobId': id});
      if (r['status'] != 'running') break;
      await Future<void>.delayed(const Duration(milliseconds: 20));
    }
    expect(r['status'], 'failed');
    expect((r['error'] as Map)['code'], 'INVALID_ARG');
  }, skip: skip);

  test('le_send: lệnh spike vào queue, lệnh P1 bị từ chối ở P0', () {
    expect(client.send(LeCommandType.LE_CMD_SPIKE_SINE, f0: 440, f1: 0), isTrue);
    expect(client.send(LeCommandType.LE_CMD_SPIKE_LOAD_VOICES, i0: 0), isTrue);
    expect(client.send(9999), isFalse, reason: 'type lạ');
  }, skip: skip);

  test('le_read_state đọc được, getPeaks clip lạ → null', () {
    final s = client.readState();
    expect(s.bufferSize, greaterThanOrEqualTo(0));
    expect(client.getPeaks('c_khong_co', 0, 16), isNull);
  }, skip: skip);
}
