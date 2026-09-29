// P2-05 DoD: mở fixture project → FakeEngineClient ghi đúng chuỗi lệnh 06 §6 (so với snapshot).
//
// Snapshot: test/fixtures/snapshots/<fixture>.open_commands.json. Chủ ý đổi chuỗi lệnh thì chạy
//   UPDATE_SNAPSHOTS=1 flutter test test/features/project_controller_test.dart
// rồi đọc diff trước khi giữ.
import 'dart:convert';
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/app_error.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/model/project.dart';
import 'package:music_looper/model/project_codec.dart';

import '../test_utils.dart';

Project loadFixture(String name) =>
    ProjectCodec().decode(File('test/fixtures/projects/$name').readAsStringSync()).project;

void expectMatchesSnapshot(List<Map<String, dynamic>> actual, String snapshotName) {
  final file = File('test/fixtures/snapshots/$snapshotName');
  final pretty = '${const JsonEncoder.withIndent('  ').convert(actual)}\n';
  if (Platform.environment['UPDATE_SNAPSHOTS'] == '1' || !file.existsSync()) {
    file
      ..createSync(recursive: true)
      ..writeAsStringSync(pretty);
    markTestSkipped('Đã ghi snapshot mới ${file.path} — đọc lại rồi chạy test lần nữa');
    return;
  }
  expect(jsonDecode(pretty), equals(jsonDecode(file.readAsStringSync())));
}

void main() {
  late FakeEngineClient fake;
  late ProviderContainer container;

  setUp(() {
    fake = createFakeEngine();
    container = ProviderContainer(overrides: engineOverrides(fake));
    addTearDown(container.dispose);
  });

  ProjectController controller() => container.read(projectControllerProvider.notifier);
  ProjectSession? session() => container.read(projectControllerProvider);

  for (final fixture in ['example_v1.json', 'minimal_v1.json', 'all_enums_v1.json']) {
    test('$fixture: chuỗi lệnh mở project khớp snapshot (06 §6)', () async {
      await controller().open(loadFixture(fixture), dir: '/docs/Projects/X.loopproj');
      expectMatchesSnapshot(
        fake.log.map((e) => e.toJson()).toList(),
        fixture.replaceFirst('.json', '.open_commands.json'),
      );
    });
  }

  test('thứ tự chính: project.open → transport → userInstrument → từng track → midi → link', () async {
    await controller().open(loadFixture('example_v1.json'), dir: '/p');
    final names = fake.log
        .map(
          (e) => switch (e) {
            FakeSend() => e.name,
            FakeCall() => e.op,
          },
        )
        .toList();
    expect(names.first, 'project.open');
    expect(names.sublist(1, 8), [
      'SET_BPM',
      'transport.setTimeSignature',
      'transport.setTempoMode', // 05 §3: ngay sau SET_BPM / timeSignature
      'SET_QUANTIZE',
      'METRONOME',
      'SET_COUNT_IN',
      'MASTER_GAIN',
    ]);
    expect(
      names.where(
        (n) =>
            n == 'track.configure' ||
            n == 'instrument.createFromRecording' ||
            n == 'instrument.setEnvelope' ||
            n.startsWith('midi.') ||
            n == 'link.enable',
      ),
      [
        'instrument.createFromRecording', // trước vòng track (06 §6)
        'instrument.setEnvelope', // envelope đã lưu đi ngay sau (P3-07)
        'track.configure',
        'track.configure',
        'track.configure',
        'midi.setMappings',
        'link.enable',
      ],
    );
    // Không có lệnh nào ngoài danh sách op của 05 §3 và job.result.
    const allowed = {
      'project.open',
      'transport.setTimeSignature',
      'transport.setTempoMode',
      'engine.info', // đọc lại tempoState sau khi mở xong (05 §3)
      'track.configure',
      'track.setInstrument',
      'fx.set',
      'clip.setAudio',
      'clip.setMidi',
      'instrument.createFromRecording',
      'instrument.setEnvelope',
      'midi.setMappings',
      'link.enable',
      'job.result',
    };
    expect(fake.calls.map((c) => c.op).toSet().difference(allowed), isEmpty);
  });

  test('đợi mọi job, progress chạy tới total rồi xoá', () async {
    final progress = <OpenProgress?>[];
    container.listen(projectControllerProvider, (_, s) => progress.add(s?.progress), fireImmediately: false);
    await controller().open(loadFixture('example_v1.json'), dir: '/p');
    // example: 2 track.setInstrument + 1 clip.setAudio + 1 instrument.createFromRecording = 4 job.
    expect(progress.whereType<OpenProgress>().last, const OpenProgress(4, 4));
    expect(progress.last, isNull);
    expect(session()!.isLoading, isFalse);
    expect(session()!.errors, isEmpty);
    expect(fake.calls.where((c) => c.op == 'job.result').length, 4);
  });

  test('job lỗi / lệnh bị từ chối → ghi vào errors, vẫn mở xong', () async {
    fake
      ..failingJobOps.add('clip.setAudio')
      ..sendResult = false;
    fake.onCall = (r) => r['op'] == 'link.enable'
        ? {
            'ok': false,
            'error': {'code': 'NOT_IMPLEMENTED', 'message': 'P4'},
          }
        : null;
    await controller().open(loadFixture('example_v1.json'), dir: '/p');
    final errors = session()!.errors;
    expect(errors.map((e) => e.op), contains(startsWith('job ')));
    expect(errors, contains(const AppError('NOT_IMPLEMENTED', op: 'link.enable')));
    expect(errors.map((e) => '$e'), isNot(contains(contains('P4'))), reason: 'không giữ message của engine (07 §8)');
    expect(errors, contains(const AppError('QUEUE_FULL', op: 'le_send SET_BPM')));
    expect(session()!.isLoading, isFalse);
    expect(session()!.project.name, 'My Jam');
  });

  test('setBpm: gửi SET_BPM TRƯỚC khi model đổi, kẹp 20..300', () async {
    await controller().open(loadFixture('minimal_v1.json'), dir: '/p');
    var sentWhenStateChanged = -1;
    container.listen(projectControllerProvider, (_, _) => sentWhenStateChanged = fake.sent.length);
    final before = fake.sent.length;
    controller().setBpm(400);
    expect(sentWhenStateChanged, before + 1, reason: 'lệnh phải đi trước cập nhật model (07 §6.6)');
    expect(fake.sent.last.name, 'SET_BPM');
    expect(fake.sent.last.d0, 300);
    expect(session()!.project.transport.bpm, 300);
  });

  test('close → project.close, state null', () async {
    await controller().open(loadFixture('minimal_v1.json'), dir: '/p');
    controller().close();
    expect(fake.calls.last.op, 'project.close');
    expect(session(), isNull);
  });

  group('thao tác track (lệnh trước, model sau)', () {
    setUp(() async => controller().open(loadFixture('example_v1.json'), dir: '/p'));

    void expectCommandBeforeModel(void Function() action, String command) {
      var sentAtStateChange = -1;
      final sub = container.listen(projectControllerProvider, (_, _) => sentAtStateChange = fake.sent.length);
      final before = fake.sent.length;
      action();
      sub.close();
      expect(fake.sent.skip(before).map((s) => s.name), contains(command));
      expect(sentAtStateChange, greaterThan(before), reason: '$command phải gửi trước khi model đổi');
    }

    test('mute / solo / gain / pan / arm', () {
      expectCommandBeforeModel(() => controller().setMute(1, true), 'TRACK_MUTE');
      expectCommandBeforeModel(() => controller().setSolo(1, true), 'TRACK_SOLO');
      expectCommandBeforeModel(() => controller().setGain(1, -9), 'TRACK_GAIN');
      expectCommandBeforeModel(() => controller().setPan(1, 2), 'TRACK_PAN');
      expectCommandBeforeModel(() => controller().setArm(1, true), 'TRACK_ARM');
      final m = session()!.project.trackAt(1)!.mixer;
      expect([m.mute, m.solo, m.gainDb, m.pan], [true, true, -9.0, 1.0]);
      expect(fake.sent.last.i0, 1);
      expect(session()!.armed, {1});
      controller().setArm(1, false);
      expect(session()!.armed, isEmpty);
    });

    test('cột chưa có track → tạo track mặc định + track.configure trước lệnh', () {
      expect(session()!.project.trackAt(5), isNull);
      controller().setMute(5, true);
      final t5 = session()!.project.trackAt(5)!;
      expect(t5.name, 'Track 6');
      expect(t5.mixer.mute, isTrue);
      final ops = fake.log.map((e) => e is FakeCall ? e.op : (e as FakeSend).name).toList();
      expect(ops.lastIndexOf('track.configure'), lessThan(ops.lastIndexOf('TRACK_MUTE')));
    });

    test('RECORDING_FINISHED → clip.info → clip audio vào model', () async {
      fake.send(LeCommandType.LE_CMD_CLIP_RECORD, track: 1, slot: 2, i0: 1);
      fake.advanceBeats(8.1); // count-in 1 bar + 1 bar thu
      await Future<void>.delayed(Duration.zero);
      final clip = session()!.project.trackAt(1)!.clipAt(2);
      expect(clip, isA<AudioClip>());
      expect((clip! as AudioClip).file, startsWith('audio/c_'));
      // Sau clip.info, controller đọc lại engine.info.tempoState (pedal mode, 05 §3).
      expect(fake.calls.lastWhere((c) => c.op != 'engine.info').request, {'op': 'clip.info', 'track': 1, 'slot': 2});
    });

    test('RECORDING_FINISHED của spike (track -1) bị bỏ qua', () async {
      fake.emit(const RecordingFinished(track: -1, slot: -1, frames: 100));
      await Future<void>.delayed(Duration.zero);
      expect(fake.calls.where((c) => c.op == 'clip.info'), isEmpty);
    });

    test('project chỉ đọc → không gửi lệnh', () async {
      await controller().open(loadFixture('minimal_v1.json'), dir: '/p', readOnly: true);
      final before = fake.sent.length;
      controller().setGain(0, -3);
      controller().setBpm(90);
      expect(fake.sent.length, before);
    });
  });

  test('clipFromInfo: audio / midi', () {
    final a = ProjectController.clipFromInfo(3, {
      'clipId': 'c_x',
      'kind': 'audio',
      'file': 'audio/c_x.caf',
      'lengthBeats': 16,
      'originalBpm': 96,
      'warp': 'repitch',
      'gainDb': -2,
    });
    expect(
      a,
      const Clip.audio(
        slot: 3,
        id: 'c_x',
        name: 'Bản thu 4',
        file: 'audio/c_x.caf',
        lengthBeats: 16,
        originalBpm: 96,
        warp: WarpMode.repitch,
        gainDb: -2,
      ),
    );
    final m = ProjectController.clipFromInfo(0, {'clipId': '', 'kind': 'midi', 'lengthBeats': 8});
    expect(m, isA<MidiClip>());
    expect(m.id, startsWith('c_'));
  });
}
