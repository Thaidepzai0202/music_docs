// Bộ hợp đồng ClipScheduler chạy với engine THẬT trên Mac (dylib build với LE_ENABLE_SIM).
// Thời gian nhạc trôi bằng `sim.offline` + `sim.advance` (05 §3, op chỉ dành cho test).
// Tự skip khi: chưa có libLoopCore.dylib, hoặc build không bật sim (op trả NOT_IMPLEMENTED).
import 'dart:ffi';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:flutter_test/flutter_test.dart';

import 'contract/clip_scheduler_contract.dart';

String? _findDylib() {
  final env = Platform.environment['LOOPCORE_DYLIB'];
  if (env != null && File(env).existsSync()) return env;
  final p = File('../../../build/mac-debug/libLoopCore.dylib').absolute.path;
  return File(p).existsSync() ? p : null;
}

/// Một engine cho cả file (le_create chỉ gọi được 1 lần mỗi process).
final class _RealHarness implements ClipEngineHarness {
  _RealHarness(this.engine);

  @override
  final EngineClient engine;

  @override
  double beat = 0;

  void _advance(Map<String, dynamic> params) {
    final r = engine.callOk('sim.advance', params);
    beat = (r['beat'] as num).toDouble();
  }

  @override
  Future<void> advanceBeats(double beats) async => _advance({'beats': beats});

  /// le_send được xử lý ở đầu block kế tiếp → render 1 frame để lệnh có hiệu lực.
  @override
  Future<void> settle() async {
    _advance({'frames': 1});
    await Future<void>.delayed(Duration.zero); // cho event (NativeCallable.listener) kịp tới
  }

  @override
  Future<void> advanceSeconds(double seconds) async => _advance({'frames': (seconds * 48000).round()});

  @override
  void makeCaptureSilent() {} // sim.offline không có input → capture luôn im lặng

  @override
  void addSample(String path, {required bool pitched}) => writeTestWav(path, pitched: pitched);

  @override
  void makeCalibrationSilent() {} // sim.offline không có input → không nghe được xung đo

  @override
  Future<void> midiIn(List<int> bytes) async => engine.callOk('sim.midiIn', {'bytes': bytes});

  @override
  bool get deliversEvents => true; // sim.advance pump event sau mỗi block

  @override
  Future<void> dispose() async {
    engine.send(LeCommandType.LE_CMD_TRANSPORT_STOP);
    _advance({'frames': 256});
  }
}

void main() {
  _RealHarness? harness;
  String? skip;
  Directory? dir;

  final path = _findDylib();
  if (path == null) {
    skip = 'Chưa có libLoopCore.dylib (scripts/build_engine_mac.sh mac-debug)';
  } else {
    final client = EngineClient.withBindings(LoopCoreBindings(DynamicLibrary.open(path)));
    dir = Directory.systemTemp.createTempSync('loopcore_sim_');
    final rc = client.create(EngineConfig(dataDir: dir.path, libraryDir: dir.path));
    final sim = client.call({'op': 'sim.offline', 'enabled': true, 'sampleRate': 48000, 'blockSize': 128});
    if (rc != LeError.LE_OK) {
      skip = 'le_create lỗi ${leErrorName(rc)}';
    } else if (sim['ok'] != true) {
      skip = 'Engine chưa bật sim (${(sim['error'] as Map?)?['code']}) — build với LE_ENABLE_SIM';
    } else {
      // sim.offline tự khởi động OfflineDeviceIO, không gọi le_audio_start.
      harness = _RealHarness(client);
    }
    tearDownAll(() {
      client.dispose();
      dir?.deleteSync(recursive: true);
    });
  }

  // Ca engine chưa làm tới: ghi tên ca → lý do; engine xong thì xoá dòng.
  const p4 = 'engine: link.enable chưa có (người dùng chưa quyết, NOT_IMPLEMENTED)';
  defineClipSchedulerContract(
    'engine thật (sim offline)',
    () async => harness!,
    skip: skip,
    knownGaps: const {'link.enable {enabled, startStopSync} → ok, LeState.linkEnabled phản ánh': p4},
  );
}
