// P4-13 Settings đầy đủ + P4-12 hiệu chỉnh latency: Audio (buffer, micro, mặc định khi thu), Độ trễ (đo + bù tay),
// MIDI (thiết bị + learn), Link, Giấy phép, Giới thiệu.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/router.dart';
import 'package:music_looper/app/theme.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/fx/fx_specs.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/services/engine_settings.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';
import '../test_utils.dart';

void main() {
  Future<SessionHarness> openSettings(WidgetTester tester, {String? section, Project? project}) async {
    final h = SessionHarness(tester);
    await h.pump(project: project);
    await tester.tap(find.byKey(const Key('transport.more')));
    await h.settle();
    await tester.tap(find.byKey(const Key('transport.settings')));
    await h.settle();
    if (section != null) {
      await tester.tap(find.byKey(Key('settings.nav.$section')));
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
      await h.settle();
    }
    return h;
  }

  /// JobTracker tạo trong runAsync lúc mở project → event job đi qua zone thật.
  Future<void> realTick(WidgetTester tester) =>
      tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));

  group('Audio', () {
    testWidgets('buffer 256 → spike.setBufferSize, lưu; monitor mặc định + số bar + rung lưu vào settings', (
      tester,
    ) async {
      final h = await openSettings(tester);
      expect(find.text('Cài đặt'), findsOneWidget);
      await tester.tap(find.text('256'));
      await h.settle();
      expect(h.fake.calls.lastWhere((c) => c.op == 'spike.setBufferSize').request, {
        'op': 'spike.setBufferSize',
        'frames': 256,
      });
      expect(h.c.read(settingsProvider).bufferSize, 256);
      expect(find.textContaining('Đang chạy: 256 frame (5.3 ms)'), findsOneWidget);

      await tester.tap(find.text('Tự động'));
      await tester.tap(find.text('2 bar'));
      await tester.tap(find.byKey(const Key('settings.haptics')));
      await h.settle();
      final s = h.c.read(settingsProvider);
      expect([s.defaultMonitor, s.recordBars, s.haptics], [MonitorMode.auto, 2, false]);
      await tester.tap(find.byKey(const Key('settings.back')));
      await h.settle();
      expect(find.byKey(const Key('session.topBar')), findsOneWidget);
      await h.unmount();
    });

    testWidgets('chỉ phát (thiếu quyền mic) → dòng Micro báo + nút Mở Cài đặt', (tester) async {
      final h = await openSettings(tester);
      mockEnginePlatform(permission: 'denied');
      await h.c.read(engineAudioProvider).start();
      await tester.pump();
      expect(find.text('Chưa có quyền micro: chỉ phát được, chưa thu được'), findsWidgets);
      expect(find.byKey(const Key('settings.mic.openSettings')), findsOneWidget);
      await h.unmount();
    });

    test('mở app: gửi lại cài đặt nằm ở engine (chỉ phần khác mặc định)', () {
      final fake = createFakeEngine();
      expect(applyEngineSettings(fake, const AppSettings()), isEmpty);
      expect(fake.calls, isEmpty);
      final errors = applyEngineSettings(
        fake,
        const AppSettings(midiRecordQuantize: RecordQuantize.eighth, bufferSize: 256, latencyOffsetSamples: -48),
      );
      expect(errors, isEmpty);
      expect(fake.calls.map((c) => c.request), [
        {'op': 'midi.setRecordQuantize', 'grid': 0.5},
        {'op': 'spike.setBufferSize', 'frames': 256},
        {'op': 'latency.setOffset', 'samples': -48},
      ]);
    });

    test('settings.json đi-về đủ trường mới; file cũ thiếu trường → mặc định', () {
      const s = AppSettings(
        bufferSize: 256,
        defaultMonitor: MonitorMode.always,
        latencyOffsetSamples: 96,
        latencyMeasuredSamples: 480,
      );
      expect(s.toJson()['defaultMonitor'], 'on', reason: 'cùng giá trị JSON với track.monitor (06 §2)');
      expect(AppSettings.fromJson(s.toJson()), s);
      final old = AppSettings.fromJson(const {'version': 1, 'recordBars': 2, 'bufferSize': 999});
      expect(
        [old.bufferSize, old.defaultMonitor, old.latencyOffsetSamples, old.latencyMeasuredSamples],
        [128, MonitorMode.off, 0, null],
      );
    });

    test('monitor mặc định áp cho project mới và cột chưa có track (gửi TRACK_MONITOR)', () async {
      expect(newProject('x', monitor: MonitorMode.auto).tracks.every((t) => t.monitor == MonitorMode.auto), isTrue);
      final fake = createFakeEngine();
      final settings = MemorySettingsRepository();
      final c = ProviderContainer(
        overrides: [
          ...engineOverrides(fake, settings: settings),
          initialSettingsProvider.overrideWithValue(const AppSettings(defaultMonitor: MonitorMode.auto)),
        ],
      );
      addTearDown(c.dispose);
      final p = newProject('y');
      await c
          .read(projectControllerProvider.notifier)
          .open(p.copyWith(tracks: p.tracks.where((t) => t.index != 7).toList()), dir: '/p');
      c.read(projectControllerProvider.notifier).setMute(7, true);
      final ops = fake.log.reversed.take(3).toList().reversed.map((e) => e.toJson()).toList();
      expect(ops[0]['call']['op'], 'track.configure');
      expect(ops[1], {'send': 'TRACK_MONITOR', 'track': 7, 'i0': 1});
      expect(c.read(projectControllerProvider)!.project.trackAt(7)!.monitor, MonitorMode.auto);
    });
  });

  group('Độ trễ (P4-12)', () {
    testWidgets('đo: latency.calibrate job → tiến độ → kết quả lưu + engine dùng; bù tay → latency.setOffset', (
      tester,
    ) async {
      final h = await openSettings(tester, section: 'latency');
      expect(find.text('chưa có'), findsOneWidget);
      await tester.tap(find.byKey(const Key('latency.calibrate')));
      // quyền mic (kênh giả) → start audio → callJob → job giả xong: đi qua cả zone giả lẫn zone thật.
      for (var i = 0; i < 20 && h.c.read(settingsProvider).latencyMeasuredSamples == null; i++) {
        await tester.pump(const Duration(milliseconds: 1)); // pump() không đối số không chạy Timer(0)
        await realTick(tester);
      }
      await tester.pump();
      expect(h.fake.calls.map((c) => c.op), contains('latency.calibrate'));
      expect(h.c.read(settingsProvider).latencyMeasuredSamples, 492);
      expect(find.text('492 sample · 10.3 ms'), findsNWidgets(2), reason: 'engine đang dùng + lần đo gần nhất');
      // Engine đã áp dụng offset = measured − reported; app lưu để gửi lại lúc mở app (05 §3), không gửi lần nữa.
      expect(h.c.read(settingsProvider).latencyOffsetSamples, 22);
      expect(h.fake.latencyOffset, 22);
      expect(h.fake.calls.where((c) => c.op == 'latency.setOffset'), isEmpty);
      expect(find.textContaining('(22 sample)'), findsOneWidget);
      expect(find.textContaining('Device báo 470 · bù 22 sample'), findsOneWidget);

      await tester.drag(find.byKey(const Key('latency.offset')), const Offset(120, 0));
      await tester.pump();
      final off = h.fake.calls.lastWhere((c) => c.op == 'latency.setOffset').request['samples'] as int;
      expect(off, greaterThan(0));
      expect(h.fake.latencyOffset, off);
      expect(h.c.read(settingsProvider).latencyOffsetSamples, off);
      await tester.tap(find.byKey(const Key('latency.offset.reset')));
      await tester.pump();
      expect(h.fake.latencyOffset, 0);
      await h.unmount();
    });

    testWidgets('đo thất bại (job AUDIO_DEVICE, message NO_SIGNAL) → câu lý do đã dịch, không lưu kết quả', (
      tester,
    ) async {
      final h = await openSettings(tester, section: 'latency');
      h.fake.calibrateFailure = 'NO_SIGNAL';
      await tester.tap(find.byKey(const Key('latency.calibrate')));
      for (var i = 0; i < 20 && find.textContaining('Không nghe thấy tiếng click').evaluate().isEmpty; i++) {
        await tester.pump(const Duration(milliseconds: 1));
        await realTick(tester);
      }
      await tester.pump();
      expect(find.text('Đo lỗi: Không nghe thấy tiếng click đo — tăng âm lượng hoặc cắm cáp loopback'), findsOneWidget);
      expect(find.textContaining('NO_SIGNAL'), findsNothing);
      expect(h.c.read(settingsProvider).latencyMeasuredSamples, isNull);
      await h.unmount();
    });

    testWidgets('chỉ phát → nút đo bị khoá (cần micro)', (tester) async {
      final h = await openSettings(tester);
      mockEnginePlatform(permission: 'denied');
      await h.c.read(engineAudioProvider).start();
      await tester.tap(find.byKey(const Key('settings.nav.latency')));
      await h.settle();
      expect(tester.widget<FilledButton>(find.byKey(const Key('latency.calibrate'))).onPressed, isNull);
      expect(find.text('Cần quyền micro để đo'), findsOneWidget);
      await h.unmount();
    });
  });

  group('MIDI', () {
    testWidgets('thiết bị: danh sách + bật/tắt (midi.enableDevice); MIDI_DEVICES → tự làm mới', (tester) async {
      final h = await openSettings(tester, section: 'midi');
      expect(find.text('Launchpad Mini MK3'), findsWidgets);
      expect(find.text('nanoKONTROL2'), findsOneWidget);
      await tester.tap(find.byKey(const Key('midi.device.ble:nanokontrol2')));
      await tester.pump();
      expect(h.fake.calls.lastWhere((c) => c.op == 'midi.enableDevice').request, {
        'op': 'midi.enableDevice',
        'id': 'ble:nanokontrol2',
        'enabled': true,
      });
      expect(tester.widget<Switch>(find.byKey(const Key('midi.device.ble:nanokontrol2'))).value, isTrue);

      h.fake.midiInputs.add({'id': 'usb:keystep', 'name': 'KeyStep 37', 'enabled': true});
      h.fake.emit(const MidiDevicesChanged());
      await tester.pump();
      await tester.pump();
      expect(find.text('KeyStep 37'), findsOneWidget);
      await h.unmount();
    });

    testWidgets('learn clip: learnStart {target} → MIDI_LEARNED → mapping vào project + midi.setMappings; xoá', (
      tester,
    ) async {
      final h = await openSettings(tester, section: 'midi');
      await tester.tap(find.byKey(const Key('midi.learn.add')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.track')));
      await h.settle();
      await tester.tap(find.textContaining('3. ').last);
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.slot')));
      await h.settle();
      await tester.tap(find.text('Ô 2').last);
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.start')));
      await tester.pump();
      expect(h.fake.midiLearnTarget, {'kind': 'clip', 'track': 2, 'slot': 1});
      expect(find.byKey(const Key('midi.learn.waiting')), findsOneWidget);

      h.fake.simulateMidiLearn(isCc: false, number: 36);
      await h.settle();
      final m = h.session.project.midiMappings.single;
      expect(m.src, const MidiSource(device: 'Launchpad Mini MK3', kind: 'note', channel: 0, number: 36));
      expect(m.target, {'kind': 'clip', 'track': 2, 'slot': 1});
      expect(h.fake.midiMappings, [m.toJson()]);
      expect(find.text('Nốt 36 · Launchpad Mini MK3 · kênh 1'), findsOneWidget, reason: 'channel 0 = kênh 1');

      await tester.tap(find.byKey(const Key('midi.mapping.delete.0')));
      await h.settle();
      expect(h.session.project.midiMappings, isEmpty);
      expect(h.fake.midiMappings, isEmpty);
      await h.unmount();
    });

    testWidgets('learn tham số FX; gán lại cùng nút → thay gán cũ; huỷ → midi.learnCancel', (tester) async {
      final d = demoProject();
      final project = d.copyWith(
        midiMappings: const [
          MidiMapping(
            src: MidiSource(device: 'Launchpad Mini MK3', kind: 'cc', channel: 0, number: 14),
            target: {'kind': 'clip', 'track': 0, 'slot': 0},
          ),
        ],
        tracks: [
          for (final t in d.tracks)
            t.index == 1
                ? t.copyWith(
                    fx: [FxSlot(type: FxType.delay, params: fxDefaultParams(FxType.delay))],
                  )
                : t,
        ],
      );
      final h = await openSettings(tester, section: 'midi', project: project);
      await tester.tap(find.byKey(const Key('midi.learn.add')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.kind')));
      await h.settle();
      await tester.tap(find.text('Tham số FX').last);
      await h.settle();
      expect(find.text('Track này chưa có FX.'), findsOneWidget);
      await tester.tap(find.byKey(const Key('midi.learn.track')));
      await h.settle();
      await tester.tap(find.textContaining('2. ').last);
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.param')));
      await h.settle();
      await tester.tap(find.text('Feedback').last);
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.start')));
      await tester.pump();
      expect(h.fake.midiLearnTarget, {'kind': 'fx', 'track': 1, 'slot': 0, 'param': 1});
      h.fake.simulateMidiLearn(isCc: true, number: 14);
      await h.settle();
      final m = h.session.project.midiMappings.single;
      expect(m.target, {'kind': 'fx', 'track': 1, 'slot': 0, 'param': 1}, reason: 'CC 14 cũ (clip) bị thay');
      expect(find.textContaining('Delay · Feedback'), findsOneWidget);

      await tester.tap(find.byKey(const Key('midi.learn.add')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.start')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('midi.learn.cancel')));
      await h.settle();
      expect(h.fake.calls.last.op, 'midi.learnCancel');
      expect(h.fake.midiLearnTarget, isNull);
      await h.unmount();
    });
  });

  testWidgets('learn transport / scene / master FX; learnResult cho kênh thật; mọi kênh khi engine không trả nguồn', (
    tester,
  ) async {
    final h = await openSettings(tester, section: 'midi');
    Future<void> pick(String key, String text) async {
      await tester.tap(find.byKey(Key(key)));
      await h.settle();
      await tester.tap(find.text(text).last);
      await h.settle();
    }

    Future<Map<String, dynamic>?> learn(Future<void> Function() choose, {int channel = 0, int number = 1}) async {
      await tester.tap(find.byKey(const Key('midi.learn.add')));
      await h.settle();
      await choose();
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.learn.start')));
      await tester.pump();
      final target = h.fake.midiLearnTarget;
      h.fake.simulateMidiLearn(isCc: true, number: number, channel: channel);
      await h.settle();
      return target;
    }

    expect(await learn(() => pick('midi.learn.kind', 'Transport'), channel: 9, number: 20), {
      'kind': 'transport',
      'action': 'toggle',
    });
    expect(find.text('Transport · Play/Stop'), findsOneWidget);
    expect(find.text('CC 20 · Launchpad Mini MK3 · kênh 10'), findsOneWidget);

    await tester.tap(find.byKey(const Key('midi.learn.add')));
    await h.settle();
    await pick('midi.learn.kind', 'Scene');
    await pick('midi.learn.scene', '3. Scene 3');
    await tester.tap(find.byKey(const Key('midi.learn.start')));
    await tester.pump();
    expect(h.fake.midiLearnTarget, {'kind': 'scene', 'slot': 2});
    h.fake.simulateMidiLearn(isCc: false, number: 60);
    await h.settle();

    await tester.tap(find.byKey(const Key('midi.learn.add')));
    await h.settle();
    await pick('midi.learn.kind', 'Tham số FX');
    await pick('midi.learn.track', 'Master');
    await pick('midi.learn.fxSlot', 'Limiter');
    await tester.tap(find.byKey(const Key('midi.learn.start')));
    await tester.pump();
    expect(h.fake.midiLearnTarget, {'kind': 'fx', 'track': -1, 'slot': 1, 'param': 0});
    // Engine không trả nguồn (op chưa có) → gán cho mọi thiết bị / mọi kênh.
    h.fake.onCall = (r) => r['op'] == 'midi.learnResult'
        ? {
            'ok': false,
            'error': {'code': 'NOT_IMPLEMENTED', 'message': ''},
          }
        : null;
    h.fake.simulateMidiLearn(isCc: true, number: 7);
    await h.settle();
    h.fake.onCall = null;
    final maps = h.session.project.midiMappings;
    expect(maps, hasLength(3));
    expect(maps.last.src, const MidiSource(device: '', kind: 'cc', channel: -1, number: 7));
    expect(find.text('FX · Master · Limiter · Ceiling'), findsOneWidget);
    expect(find.text('CC 7 · mọi thiết bị · mọi kênh'), findsOneWidget);
    expect(h.fake.midiMappings, [for (final m in maps) m.toJson()]);
    await h.unmount();
  });

  testWidgets('Link: bật → link.enable + project.link; số peer theo LINK_PEERS; đồng bộ start/stop', (tester) async {
    final h = await openSettings(tester, section: 'link');
    expect(tester.widget<Switch>(find.byKey(const Key('link.startStop'))).onChanged, isNull);
    await tester.tap(find.byKey(const Key('link.enable')));
    await tester.pump();
    expect(h.fake.calls.last.request, {'op': 'link.enable', 'enabled': true, 'startStopSync': true});
    expect(h.session.project.link.enabled, isTrue);
    h.fake.emit(const LinkPeersChanged(peers: 2));
    await tester.pump();
    await tester.pump();
    expect(find.text('Đang nối: 2 thiết bị'), findsOneWidget);
    await tester.tap(find.byKey(const Key('link.startStop')));
    await tester.pump();
    expect(h.session.project.link, const LinkSettings(enabled: true, startStopSync: false));
    expect(h.fake.linkStartStopSync, isFalse);
    await h.unmount();
  });

  testWidgets('Giấy phép: nội dung + thư viện (mở toàn văn), LinkKit chưa có → mờ, gói Flutter', (tester) async {
    final h = await openSettings(tester, section: 'licenses');
    expect(find.byKey(const Key('licenses.content.SYNTHESIZED.md')), findsOneWidget);
    expect(tester.widget<ListTile>(find.byKey(const Key('licenses.lib.Ableton Link (LinkKit)'))).enabled, isFalse);
    await tester.tap(find.byKey(const Key('licenses.lib.Signalsmith Stretch')));
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 20)));
    await h.settle();
    expect(find.textContaining('MIT License'), findsOneWidget);
    Navigator.of(tester.element(find.byKey(const Key('licenses.text')))).pop();
    await h.settle();
    await tester.ensureVisible(find.byKey(const Key('licenses.flutter')));
    await h.settle();
    await tester.tap(find.byKey(const Key('licenses.flutter')));
    await h.settle();
    expect(find.byType(LicensePage), findsOneWidget);
    await h.unmount();
  });

  testWidgets('đứng yên ở Settings: không còn frame nào được xếp (Ticker 60 Hz của Session bên dưới đã tắt)', (
    tester,
  ) async {
    final h = await openSettings(tester);
    for (final sec in ['audio', 'latency', 'midi', 'link', 'about']) {
      await tester.tap(find.byKey(Key('settings.nav.$sec')));
      await tester.pump(const Duration(milliseconds: 500)); // hết ripple
      await tester.pump(const Duration(milliseconds: 500));
      expect(tester.binding.hasScheduledFrame, isFalse, reason: 'mục $sec đứng yên mà vẫn vẽ lại');
    }
    await tester.tap(find.byKey(const Key('settings.back')));
    await h.settle();
    expect(tester.binding.hasScheduledFrame, isTrue, reason: 'về Session: Ticker chạy lại');
    await h.unmount();
  });

  testWidgets('Giới thiệu: phiên bản + engine', (tester) async {
    final h = await openSettings(tester, section: 'about');
    expect(find.text('Phiên bản 0.1.0 (1)'), findsOneWidget);
    expect(find.text('Engine giả (LOOPCORE_FAKE)'), findsOneWidget);
    expect(find.text('FakeEngine'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('mở từ màn Projects (chưa có project): MIDI learn + Link báo cần mở project', (tester) async {
    final fake = createFakeEngine();
    mockEnginePlatform();
    useIpad8Screen(tester);
    final c = ProviderContainer(overrides: engineOverrides(fake));
    addTearDown(c.dispose);
    await tester.pumpWidget(
      UncontrolledProviderScope(
        container: c,
        child: MaterialApp(
          theme: buildAppTheme(),
          initialRoute: AppRoutes.settings,
          onGenerateRoute: AppRoutes.onGenerateRoute,
        ),
      ),
    );
    await tester.tap(find.byKey(const Key('settings.nav.midi')));
    await tester.pump();
    expect(find.text('Mở một project để gán điều khiển MIDI.'), findsOneWidget);
    await tester.tap(find.byKey(const Key('settings.nav.link')));
    await tester.pump();
    expect(find.text('Mở một project để bật Link.'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
