// ĐỢT 8: monitor từng track, âm lượng metronome + nhịp, đổi tên track/scene + đổi loại track trống,
// hoàn tác overdub, bypass EQ master, ghép Bluetooth MIDI.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/model/project.dart';

import 'package:music_looper/l10n/l10n.dart';

import '../session_harness.dart';
import '../test_utils.dart';

void main() {
  Future<SessionHarness> pump(WidgetTester tester, {Project? project}) async {
    final h = SessionHarness(tester);
    await h.pump(project: project);
    return h;
  }

  testWidgets('(a) monitor từng track trong mixer → TRACK_MONITOR + tracks[].monitor; track instrument không có', (
    tester,
  ) async {
    final h = await pump(tester);
    await tester.tap(find.byKey(const Key('panel.tab.mixer')));
    await h.settle();
    expect(find.byKey(const Key('mixer.monitor.0')), findsNothing, reason: 'Drums là track instrument');
    await tester.tap(find.byKey(const Key('mixer.monitor.5')));
    await h.settle();
    await tester.tap(find.byKey(const Key('monitor.always')));
    await h.settle();
    final s = h.fake.sent.last;
    expect([s.name, s.track, s.i0], ['TRACK_MONITOR', 5, 2]);
    expect(h.session.project.trackAt(5)!.monitor, MonitorMode.always);
    expect(
      find.descendant(of: find.byKey(const Key('mixer.monitor.5')), matching: find.text(S.monitorBadge('always'))),
      findsOneWidget,
    );
    await h.unmount();
  });

  testWidgets('(b)(f) menu metronome: âm lượng (METRONOME f0, lưu khi thả) + nhịp 3/4 (transport.setTimeSignature)', (
    tester,
  ) async {
    final h = await pump(tester);
    final mode = h.session.project.transport.metronome.mode;
    await tester.tap(find.byKey(const Key('transport.metronome')));
    await h.settle();
    final before = h.fake.sent.length;
    await tester.drag(find.byKey(const Key('metronome.volume')), const Offset(-60, 0));
    await tester.pump();
    final sends = h.sentSince(before).where((s) => s.name == 'METRONOME').toList();
    expect(sends, isNotEmpty);
    expect(sends.every((s) => s.i0 == mode.leValue), isTrue, reason: 'giữ chế độ, chỉ đổi âm lượng');
    final v = h.session.project.transport.metronome.volume;
    expect(v, closeTo(sends.last.f0, 1e-6));
    expect(v, lessThan(0.6));
    expect(h.session.project.transport.metronome.mode, mode);

    await tester.tap(find.byKey(const Key('timeSig.3-4')));
    await h.settle();
    expect(h.fake.calls.last.request, {'op': 'transport.setTimeSignature', 'num': 3, 'den': 4});
    expect(h.session.project.transport.timeSignature, [3, 4]);
    expect(h.fake.session.beatsPerBar, 3);
    expect(find.textContaining('3/4'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('(f) nhấn giữ tên track: đổi tên (track.configure name), đổi loại chỉ khi track trống', (tester) async {
    final h = await pump(tester);
    await tester.longPress(find.byKey(const Key('header.name.2')));
    await h.settle();
    await tester.tap(find.byKey(const Key('header.menu.rename')));
    await h.settle();
    await tester.enterText(find.byKey(const Key('nameDialog.field')), 'Piano');
    await tester.tap(find.byKey(const Key('nameDialog.ok')));
    await h.settle();
    expect(h.fake.calls.last.request, {
      'op': 'track.configure',
      'track': 2,
      'kind': 'instrument',
      'name': 'Piano',
      'color': h.session.project.trackAt(2)!.color,
    });
    expect(h.session.project.trackAt(2)!.name, 'Piano');

    await tester.longPress(find.byKey(const Key('header.name.0')));
    await h.settle();
    expect(tester.widget<PopupMenuItem<String>>(find.byKey(const Key('header.menu.kind'))).enabled, isFalse);
    expect(find.textContaining('(track phải trống)'), findsOneWidget);
    await tester.tapAt(const Offset(10, 10));
    await h.settle();

    await tester.longPress(find.byKey(const Key('header.name.5')));
    await h.settle();
    await tester.tap(find.byKey(const Key('header.menu.kind')));
    await h.settle();
    expect(h.fake.calls.last.request, {
      'op': 'track.configure',
      'track': 5,
      'kind': 'instrument',
      'name': 'Track 6',
      'color': h.session.project.trackAt(5)!.color,
    });
    expect(h.session.project.trackAt(5)!.kind, TrackKind.instrument);
    expect(h.fake.session.trackKind[5], 'instrument');
    await h.unmount();
  });

  testWidgets('(f) Edit: chạm nút scene → đổi tên scene (không launch)', (tester) async {
    final h = await pump(tester);
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    final before = h.fake.sent.length;
    await tester.tap(find.byKey(const Key('scene.2')));
    await h.settle();
    await tester.enterText(find.byKey(const Key('nameDialog.field')), 'Điệp khúc');
    await tester.tap(find.byKey(const Key('nameDialog.ok')));
    await h.settle();
    expect(h.sentSince(before).where((s) => s.name == 'SCENE_LAUNCH'), isEmpty);
    expect(h.session.project.scenes.firstWhere((s) => s.index == 2).name, 'Điệp khúc');
    await h.unmount();
  });

  /// Controller mở project trong runAsync → event engine tới qua zone thật.
  Future<void> flush(WidgetTester tester) async {
    for (var i = 0; i < 3; i++) {
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 5)));
      await tester.pump(const Duration(milliseconds: 20));
    }
  }

  /// Thu 1 bar vào ô trống (track 5 là track audio) → clip audio đang phát.
  Future<void> recordAudio(WidgetTester tester, SessionHarness h, {int track = 5, int slot = 0}) async {
    h.fake.send(LeCommandType.LE_CMD_CLIP_RECORD, track: track, slot: slot, i0: 1);
    h.fake.advanceBeats(4.1);
    await flush(tester);
    expect(h.session.project.trackAt(track)!.clipAt(slot), isA<AudioClip>());
  }

  Future<void> overdubPass(WidgetTester tester, SessionHarness h, int track, {void Function()? during}) async {
    h.fake.send(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: track);
    h.fake.advanceBeats(0.5);
    await tester.pump(const Duration(milliseconds: 20));
    during?.call();
    h.fake.advanceBeats(0.5);
    h.fake.send(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: track); // hết lượt → RECORDING_FINISHED
    h.fake.advanceBeats(0.1);
    await flush(tester);
  }

  testWidgets(
    '(e) hoàn tác overdub (clip audio): bật khi clip.info.hasUndo; bấm → clip.undoOverdub; menu Edit cũng có',
    (tester) async {
      final h = await pump(tester);
      await recordAudio(tester, h);
      await tester.tap(find.byKey(const Key('transport.edit')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('cell.5.0')));
      await h.settle();
      IconButton undo() => tester.widget<IconButton>(find.byKey(const Key('clip.undoOverdub')));
      expect(undo().onPressed, isNull, reason: 'chưa overdub');

      await overdubPass(tester, h, 5);
      expect(undo().onPressed, isNotNull);

      await tester.longPress(find.byKey(const Key('cell.5.0')));
      await h.settle();
      expect(tester.widget<PopupMenuItem<String>>(find.byKey(const Key('menu.undoOverdub'))).enabled, isTrue);
      await tester.tapAt(const Offset(10, 10));
      await h.settle();

      await tester.tap(find.byKey(const Key('clip.undoOverdub')));
      await h.settle();
      expect(h.fake.calls.map((c) => c.op), contains('clip.undoOverdub'));
      expect(undo().onPressed, isNull, reason: 'đã hoàn tác');
      await h.unmount();
    },
  );

  testWidgets('04 §5.4: overdub MIDI xong → model có nốt mới, giữ tên/id; clip MIDI không có mục hoàn tác', (
    tester,
  ) async {
    final h = await pump(tester);
    final before = h.session.project.trackAt(1)!.clipAt(0)! as MidiClip;
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 1, slot: 0);
    h.fake.advanceBeats(0.1);
    await overdubPass(
      tester,
      h,
      1,
      during: () {
        h.fake.send(LeCommandType.LE_CMD_NOTE_ON, track: 1, i0: 72, f0: 0.8);
        h.fake.advanceBeats(0.25);
        h.fake.send(LeCommandType.LE_CMD_NOTE_OFF, track: 1, i0: 72);
      },
    );
    final after = h.session.project.trackAt(1)!.clipAt(0)! as MidiClip;
    expect(after.notes.length, before.notes.length + 1);
    expect(after.notes.any((n) => n.p == 72), isTrue);
    expect([after.id, after.name], [before.id, before.name], reason: 'overdub không tạo clip mới');
    await tester.pump(const Duration(seconds: 3));
    expect(h.repo.saves.last.$2.trackAt(1)!.clipAt(0)!, after, reason: 'autosave lưu nốt vừa overdub');

    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.longPress(find.byKey(const Key('cell.1.0')));
    await h.settle();
    expect(find.byKey(const Key('menu.undoOverdub')), findsNothing, reason: 'chỉ clip audio có lớp undo');
    await tester.tapAt(const Offset(10, 10));
    await h.settle();
    await h.unmount();
  });

  testWidgets('04 §5.4: overdub audio xong → revision tăng → waveform lấy lại peaks', (tester) async {
    final h = await pump(tester);
    await recordAudio(tester, h);
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(find.byKey(const Key('cell.5.0')));
    await h.settle();
    final id = h.session.project.trackAt(5)!.clipAt(0)!.id;
    final peaks = h.fake.peakRequests.where((r) => r.$1 == id).length;
    expect(peaks, greaterThan(0));
    await overdubPass(tester, h, 5);
    expect(h.session.revisionOf(id), 1);
    expect(h.fake.peakRequests.where((r) => r.$1 == id).length, greaterThan(peaks));
    await h.unmount();
  });

  testWidgets('(g) bypass EQ master: FX_BYPASS track −1 slot 0 → master.eq3Bypass', (tester) async {
    final h = await pump(tester);
    await tester.tap(find.byKey(const Key('panel.tab.fx')));
    await h.settle();
    await tester.tap(find.byKey(const Key('fx.master.eqBypass')));
    await tester.pump();
    final s = h.fake.sent.last;
    expect([s.name, s.track, s.slot, s.i0], ['FX_BYPASS', -1, 0, 1]);
    expect(h.session.project.master.eq3Bypass, isTrue);
    expect(h.fake.masterEqBypass, isTrue);
    await h.unmount();
  });

  test('(g) mở project có eq3Bypass → replay gửi FX_BYPASS track −1 slot 0', () async {
    final fake = createFakeEngine();
    final c = ProviderContainer(overrides: engineOverrides(fake));
    addTearDown(c.dispose);
    final d = demoProject();
    await c
        .read(projectControllerProvider.notifier)
        .open(d.copyWith(master: d.master.copyWith(eq3Bypass: true)), dir: '/p');
    expect(fake.sent.where((s) => s.name == 'FX_BYPASS' && s.track == -1 && s.slot == 0 && s.i0 == 1), hasLength(1));
    expect(fake.masterEqBypass, isTrue);
  });

  testWidgets('(c) Cài đặt → MIDI → Ghép Bluetooth MIDI: mở màn iOS rồi làm mới danh sách', (tester) async {
    final h = await pump(tester);
    final platform = mockEnginePlatform();
    await tester.tap(find.byKey(const Key('transport.more')));
    await h.settle();
    await tester.tap(find.byKey(const Key('transport.settings')));
    await h.settle();
    await tester.tap(find.byKey(const Key('settings.nav.midi')));
    await h.settle();
    h.fake.midiInputs.add({'id': 'ble:widi', 'name': 'WIDI Master', 'enabled': true}); // vừa ghép xong
    final lists = h.fake.calls.where((c) => c.op == 'midi.listDevices').length;
    await tester.tap(find.byKey(const Key('midi.bluetooth')));
    await h.settle();
    expect(platform, contains('showBluetoothMidi'));
    expect(h.fake.calls.where((c) => c.op == 'midi.listDevices').length, lists + 1);
    expect(find.text('WIDI Master'), findsOneWidget);
    await h.unmount();
  });
}
