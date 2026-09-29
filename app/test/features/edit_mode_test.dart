// P2-12: chế độ Edit/Perform + menu ngữ cảnh. DoD: Perform nhấn giữ không làm gì; Edit chạm chọn,
// nhấn giữ mở menu; mọi thao tác cập nhật cả model lẫn engine.
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

void main() {
  Finder cell(int t, int s) => find.byKey(Key('cell.$t.$s'));

  Future<void> enterEdit(WidgetTester tester) async {
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
  }

  Future<void> openMenu(SessionHarness h, int t, int s) async {
    await h.tester.longPress(cell(t, s));
    await h.settle();
  }

  testWidgets('Perform: nhấn giữ chỉ launch 1 lần, không mở menu', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    final n = h.fake.sent.length;
    await tester.longPress(cell(0, 0));
    await h.settle();
    expect(h.sentSince(n).map((s) => s.name), ['CLIP_LAUNCH']);
    expect(find.byKey(const Key('menu.duplicate')), findsNothing);
    await h.unmount();
  });

  testWidgets('Edit: chạm để chọn (không launch), mở tab Clip với thông tin clip', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    final n = h.fake.sent.length;
    await tester.tap(cell(1, 0));
    await h.settle();
    expect(h.sentSince(n).where((s) => s.name == 'CLIP_LAUNCH'), isEmpty);
    expect(h.c.read(sessionUiProvider).selected, const CellRef(1, 0));
    expect(find.byKey(const Key('clipPanel')), findsOneWidget);
    expect(find.text('Bass A'), findsWidgets);
    expect(find.textContaining('· 8 beat'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('Edit → menu Nhân bản: clip sang ô trống kế dưới, engine nhận clip.setMidi', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 1, 1); // Bass B (slot 1); slot 2 trống
    await tester.tap(find.byKey(const Key('menu.duplicate')));
    await h.settle();
    final copy = h.session.project.trackAt(1)!.clipAt(2)!;
    expect(copy.name, 'Bass B');
    expect(copy.id, isNot('c_demo_b1'));
    final req = h.fake.calls.lastWhere((c) => c.op == 'clip.setMidi').request;
    expect([req['track'], req['slot'], req['clipId']], [1, 2, copy.id]);
    expect(h.fake.readState().clipState[1][2], LeClipState.LE_CLIP_STOPPED);
    await h.unmount();
  });

  testWidgets('Edit → Đổi tên (chỉ model)', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 0, 0);
    await tester.tap(find.byKey(const Key('menu.rename')));
    await h.settle();
    await tester.enterText(find.byKey(const Key('nameDialog.field')), 'Nhịp chính');
    await tester.tap(find.byKey(const Key('nameDialog.ok')));
    await h.settle();
    expect(h.session.project.trackAt(0)!.clipAt(0)!.name, 'Nhịp chính');
    expect(find.descendant(of: cell(0, 0), matching: find.text('Nhịp chính')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('Edit → Copy rồi Dán vào ô trống ở track khác', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 2, 0); // Keys "Hợp âm"
    await tester.tap(find.byKey(const Key('menu.copy')));
    await h.settle();
    await openMenu(h, 5, 3); // ô trống
    await tester.tap(find.byKey(const Key('menu.paste')));
    await h.settle();
    final pasted = h.session.project.trackAt(5)!.clipAt(3)!;
    expect(pasted.name, 'Hợp âm');
    expect(pasted.lengthBeats, 16);
    final writes = h.fake.calls.where((c) => c.op != 'clip.info').toList(); // bỏ lệnh đọc (nút hoàn tác overdub)
    expect(writes.last.request['op'], 'clip.setMidi');
    expect([writes.last.request['track'], writes.last.request['slot']], [5, 3]);
    await h.unmount();
  });

  testWidgets('ô trống chưa copy gì → mục Dán bị tắt', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 6, 6);
    final item = tester.widget<PopupMenuItem<String>>(find.byKey(const Key('menu.paste')));
    expect(item.enabled, isFalse);
    await tester.tapAt(const Offset(5, 400)); // đóng menu
    await h.settle();
    await h.unmount();
  });

  testWidgets('Edit → Xoá: clip.clear trước, model sau', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 3, 1);
    await tester.tap(find.byKey(const Key('menu.delete')));
    await h.settle();
    // Bỏ lệnh đọc: clip.info (nút hoàn tác overdub) và engine.info (tempoState sau khi xoá clip, 05 §3).
    expect(h.fake.calls.lastWhere((c) => c.op != 'clip.info' && c.op != 'engine.info').request, {
      'op': 'clip.clear',
      'track': 3,
      'slot': 1,
    });
    expect(h.session.project.trackAt(3)!.clipAt(1), isNull);
    expect(h.fake.readState().clipState[3][1], LeClipState.LE_CLIP_EMPTY);
    await h.unmount();
  });

  testWidgets('Edit → Màu track', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await openMenu(h, 0, 0);
    await tester.tap(find.byKey(const Key('menu.color')));
    await h.settle();
    await tester.tap(find.byKey(const Key('color.4')));
    await h.settle();
    expect(h.session.project.trackAt(0)!.color, '#59C3FF');
    // Màu mới gửi kèm track.configure → engine đổi màu LED Launchpad (P4-05).
    expect(h.fake.calls.lastWhere((c) => c.op == 'track.configure').request['color'], '#59C3FF');
    expect(h.fake.trackColors[0], '#59C3FF');
    await h.unmount();
  });

  testWidgets('thoát Edit → bỏ chọn, chạm lại launch như Perform', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await enterEdit(tester);
    await tester.tap(cell(0, 0));
    await tester.pump();
    await enterEdit(tester); // bấm lần 2 = về Perform
    expect(h.c.read(sessionUiProvider).selected, isNull);
    final n = h.fake.sent.length;
    await tester.tap(cell(0, 0));
    expect(h.sentSince(n).first.name, 'CLIP_LAUNCH');
    await h.unmount();
  });

  testWidgets('P2-14: mixer — fader master gửi MASTER_GAIN, M/S trong mixer', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    final n = h.fake.sent.length;
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('mixer.fader.master'))));
    await g.moveBy(const Offset(0, 30));
    await tester.pump();
    await g.up();
    await tester.pump();
    final master = h.sentSince(n).where((s) => s.name == 'MASTER_GAIN').toList();
    expect(master, isNotEmpty);
    expect(h.session.project.master.gainDb, closeTo(master.last.f0, 1e-6));
    expect(h.session.project.master.gainDb, lessThan(0));

    await tester.tap(find.byKey(const Key('mixer.mute.2')));
    await tester.pump();
    expect([h.fake.sent.last.name, h.fake.sent.last.track, h.fake.sent.last.i0], ['TRACK_MUTE', 2, 1]);
    await tester.tap(find.byKey(const Key('mixer.solo.2')));
    await tester.pump();
    expect([h.fake.sent.last.name, h.fake.sent.last.i0], ['TRACK_SOLO', 1]);
    expect(find.byKey(const Key('mixer.meter.master')), findsOneWidget);
    expect(h.session.project.trackAt(2)!.mixer, const Mixer(gainDb: -6, pan: -0.2, mute: true, solo: true));
    await h.unmount();
  });
}
