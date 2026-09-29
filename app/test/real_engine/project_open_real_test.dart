// P2-24 với ENGINE THẬT (dylib Mac + sim): mở project dùng thư viện thật theo chuỗi 06 §6.
// Tự skip khi chưa có libLoopCore.dylib hoặc build không bật sim.
import 'dart:ffi';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_bootstrap.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

import '../test_utils.dart';
import 'real_engine_utils.dart';

void main() {
  final dylib = File('../build/mac-debug/libLoopCore.dylib').absolute.path;
  String? skip;
  EngineClient? client;
  late Directory tmp;

  if (!File(dylib).existsSync()) {
    skip = 'Chưa có libLoopCore.dylib (scripts/build_engine_mac.sh mac-debug)';
  } else {
    tmp = Directory.systemTemp.createTempSync('real_open_');
    final c = EngineClient.withBindings(LoopCoreBindings(DynamicLibrary.open(dylib)));
    final rc = c.create(EngineConfig(dataDir: tmp.path, libraryDir: Directory('assets/library').absolute.path));
    final sim = c.call({'op': 'sim.offline', 'enabled': true, 'sampleRate': 48000, 'blockSize': 128});
    if (rc != LeError.LE_OK || sim['ok'] != true) {
      skip = 'Engine chưa bật sim hoặc le_create lỗi';
    } else {
      client = c;
    }
    tearDownAll(() {
      c.dispose();
      tmp.deleteSync(recursive: true);
    });
  }

  test('mở project demo + 1 loop audio bằng engine thật: không lỗi, mọi clip nạp xong, scene phát được', () async {
    final e = client!;
    final dir = '${tmp.path}/Projects/Demo.loopproj';
    Directory('$dir/audio').createSync(recursive: true);
    File('assets/library/loops/clip_click_4beats_120.wav').copySync('$dir/audio/c_loop.wav');
    final demo = ProjectCodec().decode(File('assets/demo/demo_project.json').readAsStringSync()).project;
    final project = demo.copyWith(
      tracks: [
        for (final t in demo.tracks)
          t.index == 4
              ? t.copyWith(
                  clips: [
                    const Clip.audio(
                      slot: 0,
                      id: 'c_loop',
                      name: 'Click',
                      file: 'audio/c_loop.wav',
                      lengthBeats: 4,
                      originalBpm: 120,
                    ),
                  ],
                )
              : t,
      ],
    );

    final c = ProviderContainer(
      overrides: [
        engineBootstrapProvider.overrideWithValue(
          EngineBootstrap(engine: e, isFake: false, createResult: LeError.LE_OK, dataDir: tmp.path),
        ),
        settingsRepositoryProvider.overrideWithValue(MemorySettingsRepository()),
      ],
    );
    addTearDown(c.dispose);

    var done = false;
    final open = c.read(projectControllerProvider.notifier).open(project, dir: dir).whenComplete(() => done = true);
    // Job (nạp SFZ, decode WAV) chạy trên worker; event JOB_DONE đi qua pump của sim.advance.
    for (var i = 0; i < 2000 && !done; i++) {
      e.callOk('sim.advance', {'frames': 512});
      await Future<void>.delayed(const Duration(milliseconds: 2));
    }
    expect(done, isTrue, reason: 'job mở project chưa xong sau 2000 lượt sim.advance');
    await open;
    // JOB_DONE được phát ở pump SAU block cuối của sim.advance (onDone đã áp clip vào model/snapshot), còn
    // LeState.clipState do RT ghi ở block kế tiếp → phải chạy thêm ≥ 1 block rồi mới đọc state. Thiếu bước này
    // test đỏ chập chờn khi job cuối cùng xong đúng ở cuối lượt advance (hay gặp khi chạy chung cả bộ, máy bận).
    // Tái hiện tất định: advance đúng 1 block/lượt → 20/20 lần clip audio còn EMPTY ngay sau JOB_DONE.
    e.callOk('sim.advance', {'frames': 128});
    final errors = c.read(projectControllerProvider)!.errors;
    final unexpected = errors.where((m) => !(laterOps.contains(m.op) && m.code == 'NOT_IMPLEMENTED')).toList();
    // ignore: avoid_print
    print('Op phase sau đang NOT_IMPLEMENTED: ${errors.length - unexpected.length} — ${errors.join(' | ')}');
    expect(unexpected, isEmpty);

    final st = e.readState();
    for (final t in project.tracks) {
      for (final clip in t.clips) {
        expect(st.clipState[t.index][clip.slot], LeClipState.LE_CLIP_STOPPED, reason: '${t.name}/${clip.name}');
      }
    }
    expect(
      e.callOk('clip.getMidi', {'track': 0, 'slot': 0})['notes'],
      hasLength(project.trackAt(0)!.clipAt(0)!.noteCount),
    );

    e.send(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 0);
    e.callOk('sim.advance', {'frames': 256});
    final st2 = e.readState();
    expect([
      st2.clipState[0][0],
      st2.clipState[1][0],
      st2.clipState[2][0],
      st2.clipState[4][0],
    ], List.filled(4, LeClipState.LE_CLIP_PLAYING));
    e.send(LeCommandType.LE_CMD_TRANSPORT_STOP);
    e.callOk('sim.advance', {'frames': 256});
  }, skip: skip);
}

extension on Clip {
  int get noteCount => this is MidiClip ? (this as MidiClip).notes.length : 0;
}
