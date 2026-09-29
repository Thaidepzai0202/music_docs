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

  test('thứ tự chính: project.open → transport → từng track → userInstrument → midi → link', () async {
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
    expect(names.sublist(1, 7), [
      'SET_BPM',
      'transport.setTimeSignature',
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
            n.startsWith('midi.') ||
            n == 'link.enable',
      ),
      [
        'track.configure',
        'track.configure',
        'track.configure',
        'instrument.createFromRecording',
        'midi.setMappings',
        'link.enable',
      ],
    );
    // Không có lệnh nào ngoài danh sách op của 05 §3 và job.result.
    const allowed = {
      'project.open',
      'transport.setTimeSignature',
      'track.configure',
      'track.setInstrument',
      'fx.set',
      'clip.setAudio',
      'clip.setMidi',
      'instrument.createFromRecording',
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
    expect(errors, contains(startsWith('job ')));
    expect(errors, contains('link.enable: NOT_IMPLEMENTED P4'));
    expect(errors, contains('le_send SET_BPM bị từ chối'));
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
}
