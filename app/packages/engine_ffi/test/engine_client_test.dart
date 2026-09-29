// Test EngineClient đi qua đường FFI thật (toNativeUtf8, le_call, le_free_string, struct…),
// nhưng các hàm le_* là hàm Dart giả, gắn vào bằng NativeCallable.isolateLocal.
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';

/// Bảng hàm `le_*` giả, ghi lại mọi thứ EngineClient truyền xuống C.
class _NativeFakes {
  _NativeFakes() {
    _add('le_api_version', NativeCallable<Int32 Function()>.isolateLocal(() => 1, exceptionalReturn: -99));
    _add('le_audio_start', NativeCallable<Int32 Function()>.isolateLocal(() => 0, exceptionalReturn: -99));
    _add('le_audio_stop', NativeCallable<Void Function()>.isolateLocal(() {}));
    _add('le_destroy', NativeCallable<Void Function()>.isolateLocal(() => destroyCount++));
    _add(
      'le_create',
      NativeCallable<Int32 Function(Pointer<LeConfig>)>.isolateLocal((Pointer<LeConfig> c) {
        created = {
          'apiVersion': c.ref.apiVersion,
          'preferredBufferSize': c.ref.preferredBufferSize,
          'preferredSampleRate': c.ref.preferredSampleRate,
          'numInputChannels': c.ref.numInputChannels,
          'dataDir': c.ref.dataDir.cast<Utf8>().toDartString(),
          'libraryDir': c.ref.libraryDir.cast<Utf8>().toDartString(),
        };
        return 0;
      }, exceptionalReturn: -99),
    );
    _add(
      'le_call',
      NativeCallable<Pointer<Char> Function(Pointer<Char>)>.isolateLocal((Pointer<Char> req) {
        requests.add(req.cast<Utf8>().toDartString());
        final res = response.toNativeUtf8(allocator: malloc).cast<Char>();
        returned.add(res.address);
        return res;
      }),
    );
    _add(
      'le_free_string',
      NativeCallable<Void Function(Pointer<Char>)>.isolateLocal((Pointer<Char> p) {
        freed.add(p.address);
        malloc.free(p);
      }),
    );
    _add(
      'le_send',
      NativeCallable<Bool Function(Pointer<LeCommand>)>.isolateLocal((Pointer<LeCommand> p) {
        final c = p.ref;
        sent.add({
          'type': c.type,
          'track': c.track,
          'slot': c.slot,
          'i0': c.i0,
          'f0': c.f0,
          'f1': c.f1,
          'd0': c.d0,
          'hostTimeNs': c.hostTimeNs,
        });
        return sendReturn;
      }, exceptionalReturn: false),
    );
    _add(
      'le_read_state',
      NativeCallable<Void Function(Pointer<LeState>)>.isolateLocal((Pointer<LeState> out) {
        readCount++;
        out.ref
          ..publishCounter = readCount
          ..bpm = 133
          ..cpuLoad = 0.25;
        out.ref.clipState[3][4] = LeClipState.LE_CLIP_PLAYING;
      }),
    );
    _add(
      'le_set_event_callback',
      NativeCallable<Void Function(LeEventCallback)>.isolateLocal((LeEventCallback cb) => callback = cb),
    );
    _add(
      'le_get_peaks',
      NativeCallable<Int32 Function(Pointer<Char>, Int32, Pointer<Float>, Int32)>.isolateLocal((
        Pointer<Char> id,
        int level,
        Pointer<Float> out,
        int maxPairs,
      ) {
        peaksClipId = id.cast<Utf8>().toDartString();
        if (peaksClipId == 'missing') return LeError.LE_ERR_FILE_NOT_FOUND;
        final n = maxPairs < 2 ? maxPairs : 2;
        for (var i = 0; i < n; i++) {
          out[i * 2] = -0.5 * (i + 1);
          out[i * 2 + 1] = 0.5 * (i + 1);
        }
        return n;
      }, exceptionalReturn: -99),
    );
  }

  final _table = <String, Pointer>{};
  final _callables = <NativeCallable>[];

  String response = '{"ok":true,"result":{"x":1}}';
  final requests = <String>[];
  final returned = <int>[];
  final freed = <int>[];
  final sent = <Map<String, num>>[];
  bool sendReturn = true;
  int readCount = 0;
  int destroyCount = 0;
  Map<String, Object>? created;
  LeEventCallback? callback;
  String? peaksClipId;

  void _add(String name, NativeCallable c) {
    _callables.add(c);
    _table[name] = c.nativeFunction;
  }

  LoopCoreBindings get bindings =>
      LoopCoreBindings.fromLookup(<T extends NativeType>(String name) => _table[name]!.cast<T>());

  void close() {
    for (final c in _callables) {
      c.close();
    }
  }
}

void main() {
  late _NativeFakes fakes;
  late EngineClient client;

  setUp(() {
    fakes = _NativeFakes();
    client = EngineClient.withBindings(fakes.bindings);
  });

  tearDown(() {
    client.dispose();
    fakes.close();
  });

  group('call (le_call)', () {
    test('encode request thành JSON UTF-8, decode response', () {
      final res = client.call({'op': 'engine.info', 'tên': 'Nhạc'});
      expect(fakes.requests.single, '{"op":"engine.info","tên":"Nhạc"}');
      expect(res, {
        'ok': true,
        'result': {'x': 1},
      });
    });

    test('luôn giải phóng chuỗi engine trả về bằng le_free_string, đúng 1 lần', () {
      client.call({'op': 'a'});
      client.call({'op': 'b'});
      expect(fakes.freed, fakes.returned);
    });

    test('response không phải JSON → envelope lỗi, vẫn free', () {
      fakes.response = 'không phải json';
      final res = client.call({'op': 'x'});
      expect(res['ok'], false);
      expect((res['error'] as Map)['code'], 'INTERNAL');
      expect(fakes.freed, fakes.returned);
    });

    test('callOk trả result, ném EngineCallException khi ok:false', () {
      expect(client.callOk('engine.info'), {'x': 1});
      fakes.response = '{"ok":false,"error":{"code":"NOT_IMPLEMENTED","message":"chưa có"}}';
      expect(
        () => client.callOk('project.open', {'dir': '/tmp'}),
        throwsA(isA<EngineCallException>().having((e) => e.code, 'code', 'NOT_IMPLEMENTED')),
      );
      expect(fakes.requests.last, '{"op":"project.open","dir":"/tmp"}');
    });

    test('callJob đọc jobId trong result', () {
      fakes.response = '{"ok":true,"result":{"jobId":77}}';
      expect(client.callJob('spike.latencyLoopback'), 77);
    });
  });

  group('send (le_send)', () {
    test('ghi đủ trường LeCommand, hostTimeNs = 0, mặc định track/slot = -1', () {
      expect(client.send(LeCommandType.LE_CMD_SPIKE_SINE, f0: 440, f1: 0.5), isTrue);
      expect(client.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 2, slot: 5), isTrue);
      expect(client.send(LeCommandType.LE_CMD_SET_BPM, d0: 97.5), isTrue);
      expect(fakes.sent[0], {
        'type': LeCommandType.LE_CMD_SPIKE_SINE,
        'track': -1,
        'slot': -1,
        'i0': 0,
        'f0': 440.0,
        'f1': 0.5,
        'd0': 0.0,
        'hostTimeNs': 0,
      });
      expect(fakes.sent[1]['track'], 2);
      expect(fakes.sent[1]['slot'], 5);
      // Struct _cmd dùng lại: trường của lệnh trước không được lọt sang lệnh sau.
      expect(fakes.sent[1]['f0'], 0.0);
      expect(fakes.sent[2]['d0'], 97.5);
    });

    test('trả false khi engine báo queue đầy', () {
      fakes.sendReturn = false;
      expect(client.send(LeCommandType.LE_CMD_STOP_ALL), isFalse);
    });
  });

  test('readState trả view struct native dùng lại mỗi frame', () {
    final s1 = client.readState();
    expect(s1.bpm, 133);
    expect(s1.clipState[3][4], LeClipState.LE_CLIP_PLAYING);
    final s2 = client.readState();
    expect(s2.publishCounter, 2);
    expect(fakes.readCount, 2);
  });

  test('create truyền LeConfig đúng (apiVersion, chuỗi UTF-8)', () {
    final rc = client.create(const EngineConfig(dataDir: '/Documents/Thư mục', libraryDir: '/Bundle/Library'));
    expect(rc, LeError.LE_OK);
    expect(fakes.created, {
      'apiVersion': LE_API_VERSION,
      'preferredBufferSize': 128,
      'preferredSampleRate': 48000.0,
      'numInputChannels': 1,
      'dataDir': '/Documents/Thư mục',
      'libraryDir': '/Bundle/Library',
    });
    expect(fakes.callback, isNotNull, reason: 'phải đăng ký callback trước le_create');
  });

  test('event C → Stream<EngineEvent> qua NativeCallable.listener', () async {
    client.create(const EngineConfig(dataDir: '/d', libraryDir: '/l'));
    final fire = fakes.callback!.asFunction<void Function(int, int, int, int, double)>();

    final events = client.events.take(3).toList();
    fire(LeEventType.LE_EVT_JOB_PROGRESS, 0, 0, 42, 0.5);
    fire(LeEventType.LE_EVT_JOB_DONE, 0, 0, 42, 0);
    fire(LeEventType.LE_EVT_RECORDING_FINISHED, 3, 1, 0, 0);
    final got = await events;

    expect(got[0], isA<JobProgress>().having((e) => e.progress, 'progress', 0.5));
    expect(got[1], isA<JobDone>().having((e) => e.jobId, 'jobId', 42));
    expect(got[2], isA<RecordingFinished>().having((e) => e.track, 'track', 3).having((e) => e.slot, 'slot', 1));
  });

  test('getPeaks copy cặp min/max, null khi lỗi', () {
    final peaks = client.getPeaks('c_1', 0, 16)!;
    expect(peaks, [-0.5, 0.5, -1.0, 1.0]);
    expect(fakes.peaksClipId, 'c_1');
    expect(client.getPeaks('missing', 0, 16), isNull);
  });

  test('dispose gỡ callback rồi le_destroy', () {
    client.create(const EngineConfig(dataDir: '/d', libraryDir: '/l'));
    client.dispose();
    expect(fakes.callback, nullptr);
    expect(fakes.destroyCount, 1);
    client.dispose(); // gọi lần 2 không làm gì
    expect(fakes.destroyCount, 1);
  });
}
