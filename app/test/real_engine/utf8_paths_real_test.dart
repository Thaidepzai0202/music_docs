// 05 §1 mục 3: tên project, đường dẫn, tên clip có dấu tiếng Việt + emoji chạy đúng từ đầu đến cuối — ENGINE THẬT
// (dylib Mac + sim). Luồng: tạo project "Bài hát mới 🥁" → thư mục .loopproj → nạp loop có tên file UTF-8 → thu một
// take (sim) → clip.info trả file đúng → đổi tên clip + project → lưu → mở lại → mọi clip nạp được, mọi đường dẫn
// đúng từng byte. Tự skip khi chưa có libLoopCore.dylib hoặc build không bật sim.
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:engine_ffi/src/loopcore_bindings.g.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/project_repository.dart';
import 'package:music_looper/engine/engine_bootstrap.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/engine/performance_actions.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/project.dart';

import '../test_utils.dart';
import 'real_engine_utils.dart';

const projectName = 'Bài hát mới 🥁';
const renamedProject = 'Đổi tên — Ước mơ 🎸✨';
const loopFile = 'audio/Trống ồ ạt 🥁.wav';
const loopClipName = 'Nhịp trống 🥁 ạ';
const takeName = 'Bản thu ✨ số một';

/// Mọi thực thể trong [dir] (đệ quy) có đúng chuỗi byte UTF-8 của [path] — không chỉ "tìm thấy" nhờ hệ thống file
/// so khớp không phân biệt dạng chuẩn hoá Unicode.
bool existsByteExact(String dir, String path) {
  final want = utf8.encode(path);
  return Directory(dir).listSync(recursive: true).any((e) => _sameBytes(utf8.encode(e.path), want));
}

bool _sameBytes(List<int> a, List<int> b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void main() {
  final dylib = File('../build/mac-debug/libLoopCore.dylib').absolute.path;
  String? skip;
  EngineClient? client;
  late Directory tmp;

  if (!File(dylib).existsSync()) {
    skip = 'Chưa có libLoopCore.dylib (scripts/build_engine_mac.sh mac-debug)';
  } else {
    tmp = Directory.systemTemp.createTempSync('real_utf8_');
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

  test('tên có dấu + emoji: tạo → nạp loop → thu take → đổi tên → lưu → mở lại, đường dẫn đúng từng byte', () async {
    final e = client!;
    final projects = Directory('${tmp.path}/Projects');
    final repo = ProjectRepository(projectsDir: projects);
    final c = ProviderContainer(
      overrides: [
        engineBootstrapProvider.overrideWithValue(
          EngineBootstrap(engine: e, isFake: false, createResult: LeError.LE_OK, dataDir: tmp.path),
        ),
        settingsRepositoryProvider.overrideWithValue(MemorySettingsRepository()),
      ],
    );
    addTearDown(c.dispose);
    final ctl = c.read(projectControllerProvider.notifier);
    final finished = <RecordingFinished>[];
    final sub = e.events.listen((ev) {
      if (ev is RecordingFinished) finished.add(ev);
    });
    addTearDown(sub.cancel);

    Future<void> advanceUntil(bool Function() cond, String what, {int frames = 512}) async {
      for (var i = 0; i < 2000 && !cond(); i++) {
        e.callOk('sim.advance', {'frames': frames});
        await Future<void>.delayed(const Duration(milliseconds: 2));
      }
      expect(cond(), isTrue, reason: what);
      e.callOk('sim.advance', {'frames': 128}); // LeState trễ ≤ 1 block sau JOB_DONE (05 §3)
    }

    Future<void> openAndWait(Project p, String dir) async {
      var done = false;
      final f = ctl.open(p, dir: dir).whenComplete(() => done = true);
      await advanceUntil(() => done, 'job mở project $dir chưa xong');
      await f;
      final errors = c.read(projectControllerProvider)!.errors;
      expect(errors.where((m) => !(laterOps.contains(m.op) && m.code == 'NOT_IMPLEMENTED')), isEmpty);
    }

    // 1. Tạo project: thư mục .loopproj mang đúng tên UTF-8.
    final dir = await repo.create(newProject(projectName));
    expect(dir, '${projects.path}/$projectName.loopproj');
    expect(existsByteExact(projects.path, dir), isTrue, reason: 'tên thư mục project');

    // 2. Loop có tên file UTF-8 trên track 0 (track + clip cũng tên UTF-8).
    File('assets/library/loops/clip_click_4beats_120.wav').copySync('$dir/$loopFile');
    var project = (await repo.load(dir)).project;
    project = project.copyWith(
      tracks: [
        for (final t in project.tracks)
          t.index == 0
              ? t.copyWith(
                  name: 'Trống 🥁 đệm',
                  clips: const [
                    Clip.audio(
                      slot: 0,
                      id: 'c_loop',
                      name: loopClipName,
                      file: loopFile,
                      lengthBeats: 4,
                      originalBpm: 120,
                    ),
                  ],
                )
              : t,
      ],
    );
    await repo.save(project, dir);
    await openAndWait(project, dir);
    expect(e.readState().clipState[0][0], LeClipState.LE_CLIP_STOPPED, reason: 'engine decode được file UTF-8');
    expect(e.callOk('clip.info', {'track': 0, 'slot': 0})['file'], loopFile);
    expect(e.getPeaks('c_loop', 0, 64), isNotEmpty);

    // 3. Thu 1 bar vào track 1 ô 0 (sim không có input → take im lặng, nhưng file vẫn được ghi).
    c.read(settingsProvider.notifier).setRecordBars(1);
    ctl.setArm(1, true);
    e.callOk('sim.advance', {'frames': 128});
    expect(c.read(performanceActionsProvider).recordClip(1, 0), isTrue);
    await advanceUntil(
      () => c.read(projectControllerProvider)!.project.trackAt(1)?.clipAt(0) != null,
      'RECORDING_FINISHED + clip.info chưa đưa take vào model (event: $finished)',
    );
    e.send(LeCommandType.LE_CMD_TRANSPORT_STOP);
    e.callOk('sim.advance', {'frames': 256});
    final take = c.read(projectControllerProvider)!.project.trackAt(1)!.clipAt(0)! as AudioClip;
    final takeInfo = e.callOk('clip.info', {'track': 1, 'slot': 0});
    // ignore: avoid_print
    print(
      'take: ${take.file} trong $dir; RECORDING_FINISHED: ${finished.map((f) => '(${f.track},${f.slot},${f.frames})')}',
    );
    expect(takeInfo['file'], take.file);
    expect(existsByteExact(dir, '$dir/${take.file}'), isTrue, reason: 'take nằm trong thư mục project UTF-8');

    // 4. Đổi tên take, lưu, đóng; đổi tên project (đổi cả thư mục).
    ctl.renameClip(1, 0, takeName);
    await repo.save(c.read(projectControllerProvider)!.project, dir);
    ctl.close();
    final newDir = await repo.rename(dir, renamedProject);
    expect(newDir, '${projects.path}/$renamedProject.loopproj');
    expect(existsByteExact(projects.path, newDir), isTrue, reason: 'thư mục sau đổi tên');
    expect(Directory(dir).existsSync(), isFalse);

    // 5. Mở lại từ đĩa: tên giữ nguyên từng byte, mọi clip có file, engine nạp được hết.
    final loaded = await repo.load(newDir);
    expect(loaded.missingClipIds, isEmpty);
    expect(utf8.encode(loaded.project.name), utf8.encode(renamedProject));
    expect(utf8.encode(loaded.project.trackAt(0)!.name), utf8.encode('Trống 🥁 đệm'));
    expect(utf8.encode(loaded.project.trackAt(0)!.clipAt(0)!.name), utf8.encode(loopClipName));
    expect(utf8.encode(loaded.project.trackAt(1)!.clipAt(0)!.name), utf8.encode(takeName));
    await openAndWait(loaded.project, newDir);
    final st = e.readState();
    for (final (t, s) in [(0, 0), (1, 0)]) {
      expect(st.clipState[t][s], LeClipState.LE_CLIP_STOPPED, reason: 'clip $t/$s nạp lại được');
      final file = (loaded.project.trackAt(t)!.clipAt(s)! as AudioClip).file;
      expect(e.callOk('clip.info', {'track': t, 'slot': s})['file'], file);
      expect(existsByteExact(newDir, '$newDir/$file'), isTrue, reason: file);
    }

    // Phát thử cả hai clip.
    e.send(LeCommandType.LE_CMD_SCENE_LAUNCH, slot: 0);
    e.callOk('sim.advance', {'frames': 256});
    expect(
      [e.readState().clipState[0][0], e.readState().clipState[1][0]],
      [LeClipState.LE_CLIP_PLAYING, LeClipState.LE_CLIP_PLAYING],
    );
    e.send(LeCommandType.LE_CMD_TRANSPORT_STOP);
    e.callOk('sim.advance', {'frames': 256});
    ctl.close();
  }, skip: skip);
}
