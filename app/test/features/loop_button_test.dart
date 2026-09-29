// P2-28 (07 §3.1b): nút ● LOOP + pedal mode (vòng đầu quyết định BPM) + độ dài thu "Tự do" + hỏi quyền mic
// lần đầu khi cần thu (07 §4.0).
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';
import '../test_utils.dart';

void main() {
  Finder loop() => find.byKey(const Key('session.loop'));
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));
  List<FakeSend> loops(SessionHarness h) =>
      h.fake.sent.where((x) => x.type == LeCommandType.LE_CMD_LOOP_BUTTON).toList();

  // Sự kiện chạm trong test mặc định có timeStamp = 0 → tự đặt đồng hồ để phân biệt chạm đơn / chạm đúp.
  var clock = Duration.zero;

  /// Chạm nhanh (down → up) cách lần trước [gapMs] — pointer-down là đủ để gửi lệnh.
  Future<void> tapLoop(WidgetTester tester, {int gapMs = 500}) async {
    clock += Duration(milliseconds: gapMs);
    final g = await tester.createGesture();
    await g.down(tester.getCenter(loop()), timeStamp: clock);
    await g.up(timeStamp: clock + const Duration(milliseconds: 20));
    await tester.pump();
  }

  testWidgets('chạm LOOP: arm track đang chọn (nếu chưa) rồi gửi LOOP_BUTTON(track, −1); ô đang chọn → slot đó', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('header.name.5')));
    await tester.pump();
    final before = h.fake.sent.length;
    await tapLoop(tester);
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10))); // ensureMic (kênh giả)
    await tester.pump();
    final sent = h.fake.sent.sublist(before);
    final arm = sent.indexWhere((x) => x.type == LeCommandType.LE_CMD_TRACK_ARM);
    final lp = sent.indexWhere((x) => x.type == LeCommandType.LE_CMD_LOOP_BUTTON);
    expect(arm, isNonNegative);
    expect(lp, greaterThan(arm), reason: 'arm trước, LOOP sau');
    expect([sent[lp].track, sent[lp].slot], [5, -1]);
    expect(h.session.armed, contains(5));

    // Edit: chọn ô trống track 5 ô 3 → LOOP nhắm đúng ô đó. Chạm lần hai cách > 300 ms (không phải chạm đúp).
    await tester.pump(const Duration(milliseconds: 400));
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    await tester.tap(cell(5, 3));
    await tester.pump(const Duration(milliseconds: 400));
    await tapLoop(tester);
    expect([loops(h).last.track, loops(h).last.slot], [5, 3]);
    await h.unmount();
  });

  testWidgets('LOOP chỉ có một cử chỉ: chạm lần hai ngay sau vẫn gửi LOOP (không còn chạm đúp = dừng)', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('header.name.7')));
    await tester.pump();
    h.c.read(projectControllerProvider.notifier).setArm(7, true);
    await tapLoop(tester);
    await tapLoop(tester, gapMs: 120);
    expect(loops(h).length, 2);
    expect(h.fake.sent.where((x) => x.type == LeCommandType.LE_CMD_CLIP_STOP), isEmpty);
    await h.unmount();
  });

  testWidgets('■ Dừng track → CLIP_STOP(track đang chọn)', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('header.name.2')));
    await tester.pump();
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('session.trackStop'))));
    await g.up();
    await tester.pump();
    expect([h.fake.sent.last.type, h.fake.sent.last.track], [LeCommandType.LE_CMD_CLIP_STOP, 2]);
    await h.unmount();
  });

  testWidgets('↶ Hoàn tác: clip đang phát không có lớp overdub → hỏi lại → xoá; track trống → nút tắt', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('header.name.0'))); // Drums
    await tester.pump();
    h.fake.send(LeCommandType.LE_CMD_CLIP_LAUNCH, track: 0, slot: 0);
    await tester.pump();
    await tester.pump();
    await tester.tap(find.byKey(const Key('session.loopUndo')));
    await h.settle();
    expect(find.byKey(const Key('loop.confirmDelete')), findsOneWidget);
    await tester.tap(find.byKey(const Key('loop.confirmDelete')));
    await h.settle();
    expect(h.fake.calls.lastWhere((c) => c.op == 'clip.clear').request, {'op': 'clip.clear', 'track': 0, 'slot': 0});
    expect(h.session.project.trackAt(0)!.clipAt(0), isNull);

    await tester.tap(find.byKey(const Key('header.name.7'))); // track trống
    await tester.pump();
    final before = h.fake.calls.length;
    await tester.tap(find.byKey(const Key('session.loopUndo')));
    await h.settle();
    expect(find.byKey(const Key('loop.confirmDelete')), findsNothing);
    expect(h.fake.calls.length, before);
    await h.unmount();
  });

  testWidgets('↶ Hoàn tác: clip audio có lớp overdub → clip.undoOverdub (không hỏi)', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('header.name.5')));
    await tester.pump();
    h.c.read(projectControllerProvider.notifier).setArm(5, true);
    h.fake.send(LeCommandType.LE_CMD_CLIP_RECORD, track: 5, slot: 0, i0: 1);
    h.fake.advanceBeats(4.2); // thu xong 1 bar → Playing
    await tester.pump();
    h.fake.send(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 5);
    h.fake.send(LeCommandType.LE_CMD_OVERDUB_TOGGLE, track: 5); // hết lượt overdub → có lớp undo
    await tester.pump();
    await tester.pump();
    await tester.tap(find.byKey(const Key('session.loopUndo')));
    await h.settle();
    expect(find.byKey(const Key('loop.confirmDelete')), findsNothing);
    expect(h.fake.calls.map((c) => c.op), contains('clip.undoOverdub'));
    await h.unmount();
  });

  testWidgets('pedal mode: project mới chờ vòng đầu ("— BPM · chờ vòng đầu", metronome tắt, không TAP) → '
      'vòng đầu 5 s → 96 BPM, clip 8 beat vào model', (tester) async {
    final h = SessionHarness(tester);
    await h.pump(project: newProject('Pedal'));
    expect(h.session.project.transport.tempoMode, TempoMode.firstLoop);
    expect(h.session.waitingFirstLoop, isTrue);
    expect(find.byKey(const Key('transport.bpm.waiting')), findsOneWidget);
    expect(find.text(S.transportChoVongDau), findsOneWidget);
    expect(find.byKey(const Key('transport.tap')), findsNothing);
    expect(h.fake.calls.where((c) => c.op == 'transport.setTempoMode').single.request['mode'], 'firstLoop');

    await tester.tap(find.byKey(const Key('header.name.0')));
    await tester.pump();
    await tapLoop(tester); // arm + LOOP → thu ngay (transport chưa chạy)
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    await tester.pump();
    expect(h.fake.readState().clipState[0][0], LeClipState.LE_CLIP_RECORDING);
    expect(h.fake.readState().playing, 0);
    h.fake.advanceSeconds(5);
    await tester.pump(const Duration(milliseconds: 400));
    await tapLoop(tester); // chốt vòng
    await tester.pump();
    await tester.pump();
    expect(h.session.waitingFirstLoop, isFalse);
    expect(h.session.project.transport.bpm, closeTo(96, 0.01), reason: 'TEMPO_CHANGED → model');
    expect(h.session.project.trackAt(0)!.clipAt(0)!.lengthBeats, 8);
    expect(h.session.project.transport.firstLoopBeats, 8, reason: '06 §2: lưu độ dài vòng đầu (tempoState)');

    expect(find.byKey(const Key('transport.bpm.waiting')), findsNothing);
    expect(find.text(S.transportBpm('96.0')), findsOneWidget);
    // Dừng + xoá clip → engine về "chưa có tempo", phát TEMPO_CHANGED(0) → model bỏ firstLoopBeats.
    h.fake.send(LeCommandType.LE_CMD_TRANSPORT_STOP);
    await tester.pump();
    h.c.read(projectControllerProvider.notifier).deleteClip(0, 0);
    await tester.pump();
    await tester.pump();
    expect(h.session.waitingFirstLoop, isTrue);
    expect(h.session.project.transport.firstLoopBeats, isNull);
    expect(find.byKey(const Key('transport.bpm.waiting')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('pedal: engine về "chưa có tempo" chậm (~40 ms) → app đọc lại khi nhận TEMPO_CHANGED(0)', (tester) async {
    final base = newProject('P');
    final withClip = base.copyWith(
      tracks: [
        for (final t in base.tracks)
          t.index == 0
              ? t.copyWith(
                  kind: TrackKind.instrument,
                  clips: const [Clip.midi(slot: 0, id: 'c_1', name: 'MIDI 1', lengthBeats: 4)],
                )
              : t,
      ],
    );
    final h = SessionHarness(tester);
    await h.pump(project: withClip);
    expect(h.session.waitingFirstLoop, isFalse, reason: 'đã có clip → có tempo');
    var stale = true; // engine chưa kịp kiểm lại trong pump
    h.fake.onCall = (r) => r['op'] == 'engine.info' && stale
        ? {
            'ok': true,
            'result': {
              'tempoState': {'mode': 'firstLoop', 'hasTempo': true},
            },
          }
        : null;
    h.c.read(projectControllerProvider.notifier).deleteClip(0, 0);
    expect(h.session.waitingFirstLoop, isFalse, reason: 'đọc ngay: engine vẫn báo có tempo');
    stale = false; // engine đổi trạng thái trong pump kế tiếp và phát TEMPO_CHANGED(0) (05 §2)
    h.fake.emit(const TempoChanged(bpm: 0));
    await tester.pump();
    expect(h.session.waitingFirstLoop, isTrue, reason: 'nghe event, không đoán thời gian');
    expect(h.session.project.transport.bpm, 120, reason: 'value = 0 không phải BPM mới');
    h.fake.onCall = null;
    await h.unmount();
  });

  testWidgets('menu metronome: chế độ tempo → transport.setTempoMode + model', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    expect(h.session.project.transport.tempoMode, TempoMode.fixed);
    await tester.tap(find.byKey(const Key('transport.metronome')));
    await h.settle();
    await tester.tap(find.byKey(const Key('tempoMode.firstLoop')));
    await h.settle();
    expect(h.fake.calls.last.request, {'op': 'transport.setTempoMode', 'mode': 'firstLoop'});
    expect(h.session.project.transport.tempoMode, TempoMode.firstLoop);
    expect(h.session.waitingFirstLoop, isFalse, reason: 'demo đã có clip → có tempo');
    await h.unmount();
  });

  testWidgets('độ dài thu Tự do: chạm ô trống (đã arm) → CLIP_RECORD i0 = 0; chạm lần nữa → RECORD_STOP', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    h.c.read(settingsProvider.notifier).setRecordBars(0);
    h.c.read(projectControllerProvider.notifier).setArm(5, true);
    await tester.pump();
    final g = await tester.startGesture(tester.getCenter(cell(5, 2)));
    await g.up();
    await tester.pump();
    final rec = h.fake.sent.lastWhere((x) => x.type == LeCommandType.LE_CMD_CLIP_RECORD);
    expect([rec.track, rec.slot, rec.i0], [5, 2, 0]);
    h.fake.advanceBeats(1.1);
    await tester.pump();
    final g2 = await tester.startGesture(tester.getCenter(cell(5, 2)));
    await g2.up();
    await tester.pump();
    expect(h.fake.sent.last.type, LeCommandType.LE_CMD_RECORD_STOP);
    expect(h.fake.sent.last.track, 5);
    await h.unmount();
  });

  testWidgets('hỏi quyền mic lần đầu khi arm track audio (07 §4.0): chưa hỏi → hộp thoại → được cấp → arm', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    final calls = mockEnginePlatform(permission: 'undetermined', grantOnRequest: true);
    await tester.tap(find.byKey(const Key('header.arm.5')));
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    await tester.pump();
    expect(calls, contains('requestMicPermission'));
    expect(h.fake.inputEnabled, isTrue);
    expect(h.session.armed, contains(5));

    await h.unmount();
  });

  testWidgets('từ chối quyền mic khi arm → không arm, banner "Mở Cài đặt"', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    mockEnginePlatform(permission: 'undetermined', grantOnRequest: false);
    await tester.tap(find.byKey(const Key('header.arm.6')));
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    await tester.pump();
    expect(h.session.armed, isNot(contains(6)));
    await h.unmount();
  });
}
