// P2-16/17: pad 4×4 + bàn phím 2 quãng tám multi-touch.
// DoD: 10 ngón chạm cùng lúc → 10 nốt; không nốt treo khi nhấc tay hoặc khi gesture bị huỷ.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/instrument/note_player.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/ui_kit/keyboard_view.dart';

import '../../session_harness.dart';

void main() {
  group('NotePlayer', () {
    late List<String> log;
    late NotePlayer p;
    setUp(() {
      log = [];
      p = NotePlayer(noteOn: (n, v) => log.add('on $n'), noteOff: (n) => log.add('off $n'));
      addTearDown(p.dispose);
    });

    test('10 ngón → 10 ON; nhấc hết → 10 OFF, không còn nốt giữ', () {
      for (var i = 0; i < 10; i++) {
        p.down(i, 60 + i, 0.8);
      }
      expect(log.where((l) => l.startsWith('on')).length, 10);
      for (var i = 0; i < 10; i++) {
        p.up(i);
      }
      expect(log.where((l) => l.startsWith('off')).length, 10);
      expect(p.held.value, isEmpty);
    });

    test('2 ngón cùng 1 phím: ON 1 lần, OFF khi ngón cuối nhấc', () {
      p.down(1, 60, 1);
      p.down(2, 60, 1);
      p.up(1);
      expect(log, ['on 60']);
      p.up(2);
      expect(log, ['on 60', 'off 60']);
    });

    test('trượt sang phím khác: OFF phím cũ rồi ON phím mới; trượt ra ngoài → OFF', () {
      p.down(1, 60, 1);
      p.move(1, 60, 1); // cùng phím: không gửi gì
      p.move(1, 62, 1);
      p.move(1, null, 1);
      expect(log, ['on 60', 'off 60', 'on 62', 'off 62']);
      p.up(1);
      expect(log.length, 4);
    });

    test('releaseAll (panel đóng / đổi track) → tắt mọi nốt', () {
      p.down(1, 60, 1);
      p.down(2, 64, 1);
      p.releaseAll();
      expect(log, ['on 60', 'on 64', 'off 60', 'off 64']);
    });
  });

  group('hình học phím', () {
    const size = Size(700, 200); // 14 phím trắng × 50
    const kb = KeyboardLayout(baseNote: 48);

    test('phím trắng + phím đen (phần trên) + velocity theo chiều dọc', () {
      expect(kb.noteAt(const Offset(25, 180), size), 48); // C3
      expect(kb.noteAt(const Offset(75, 180), size), 50); // D3
      expect(kb.noteAt(const Offset(50, 40), size), 49); // C#3 (giữa C và D, phần trên)
      expect(kb.noteAt(const Offset(140, 40), size), 52, reason: 'sát ranh giới E/F: không có phím đen'); // E3
      expect(kb.noteAt(const Offset(375, 180), size), 60); // C4
      expect(kb.noteAt(const Offset(699, 180), size), 71); // B4
      expect(kb.noteAt(const Offset(-1, 10), size), isNull);
      expect(KeyboardLayout.velocityAt(const Offset(0, 200), size), 1.0);
      expect(KeyboardLayout.velocityAt(const Offset(0, 0), size), 0.35);
    });

    test('pad 4×4 kiểu MPC: dưới-trái = 36', () {
      const pad = PadLayout();
      const s = Size(400, 400);
      expect(pad.noteAt(const Offset(10, 390), s), 36);
      expect(pad.noteAt(const Offset(390, 390), s), 39);
      expect(pad.noteAt(const Offset(10, 10), s), 48);
      expect(pad.noteAt(const Offset(390, 10), s), 51);
    });

    test('tên nốt', () {
      expect(noteName(60), 'C4');
      expect(noteName(49), 'C#3');
    });
  });

  group('panel Instrument', () {
    Future<SessionHarness> open(WidgetTester tester, {int track = 2}) async {
      final h = SessionHarness(tester);
      await h.pump();
      await tester.tap(find.byKey(Key('header.name.$track'))); // chọn track nhận nốt
      await tester.tap(find.byKey(const Key('panel.tab.instrument')));
      await tester.pump();
      return h;
    }

    testWidgets('10 ngón trên bàn phím → 10 NOTE_ON khác nhau (track đang chọn); nhấc → 10 NOTE_OFF', (tester) async {
      final h = await open(tester);
      expect(h.fake.sent.last.name, 'SELECT_TRACK');
      final kb = find.byKey(const Key('instrument.keyboard'));
      expect(kb, findsOneWidget);
      final box = tester.getRect(kb);
      final whiteW = box.width / 14;
      final n = h.fake.sent.length;
      final gestures = <TestGesture>[];
      for (var i = 0; i < 10; i++) {
        gestures.add(
          await tester.startGesture(Offset(box.left + whiteW * (i + 0.5), box.bottom - 10), pointer: 10 + i),
        );
      }
      final ons = h.sentSince(n).where((s) => s.name == 'NOTE_ON').toList();
      expect(ons.length, 10);
      expect(ons.map((s) => s.i0).toSet().length, 10);
      expect(ons.every((s) => s.track == 2), isTrue);
      expect(ons.first.f0, greaterThan(0.9), reason: 'chạm gần mép dưới = mạnh');
      for (final g in gestures) {
        await g.up();
      }
      final offs = h.sentSince(n).where((s) => s.name == 'NOTE_OFF').toList();
      expect(offs.map((s) => s.i0).toSet(), ons.map((s) => s.i0).toSet());
      await h.unmount();
    });

    testWidgets('gesture bị huỷ (cancel) → NOTE_OFF, không nốt treo', (tester) async {
      final h = await open(tester);
      final box = tester.getRect(find.byKey(const Key('instrument.keyboard')));
      final n = h.fake.sent.length;
      final g = await tester.startGesture(Offset(box.left + 20, box.bottom - 10));
      await g.cancel();
      expect(h.sentSince(n).map((s) => s.name), ['NOTE_ON', 'NOTE_OFF']);
      await h.unmount();
    });

    testWidgets('trượt ngón qua phím khác → OFF cũ / ON mới', (tester) async {
      final h = await open(tester);
      final box = tester.getRect(find.byKey(const Key('instrument.keyboard')));
      final w = box.width / 14;
      final n = h.fake.sent.length;
      final g = await tester.startGesture(Offset(box.left + w * 0.5, box.bottom - 10));
      await g.moveTo(Offset(box.left + w * 1.5, box.bottom - 10));
      await g.up();
      expect(h.sentSince(n).map((s) => '${s.name} ${s.i0}'), [
        'NOTE_ON 48',
        'NOTE_OFF 48',
        'NOTE_ON 50',
        'NOTE_OFF 50',
      ]);
      await h.unmount();
    });

    testWidgets('đổi tab khi đang giữ phím → NOTE_OFF (panel đóng không để nốt treo)', (tester) async {
      final h = await open(tester);
      final box = tester.getRect(find.byKey(const Key('instrument.keyboard')));
      final g = await tester.startGesture(Offset(box.left + 20, box.bottom - 10));
      final n = h.fake.sent.length;
      await tester.tap(find.byKey(const Key('panel.tab.mixer')));
      await tester.pump();
      expect(h.sentSince(n).map((s) => s.name), contains('NOTE_OFF'));
      await g.up();
      await h.unmount();
    });

    testWidgets('track Drums (kit) → mặc định Pad; pad dưới-trái = nốt 36, velocity 0.8', (tester) async {
      final h = await open(tester, track: 0);
      final pads = find.byKey(const Key('instrument.pads'));
      expect(pads, findsOneWidget);
      final r = tester.getRect(pads);
      final n = h.fake.sent.length;
      await tester.tapAt(Offset(r.left + 10, r.bottom - 10));
      final on = h.sentSince(n).firstWhere((s) => s.name == 'NOTE_ON');
      expect([on.track, on.i0, on.f0], [0, 36, closeTo(0.8, 1e-6)]);
      await h.unmount();
    });

    testWidgets('dịch quãng +1 → phím đầu là C4 (60)', (tester) async {
      final h = await open(tester);
      await tester.tap(find.byKey(const Key('instrument.octaveUp')));
      await tester.pump();
      final box = tester.getRect(find.byKey(const Key('instrument.keyboard')));
      final n = h.fake.sent.length;
      await tester.tapAt(Offset(box.left + 5, box.bottom - 10));
      expect(h.sentSince(n).first.i0, 60);
      expect(h.c.read(sessionUiProvider).selectedTrack, 2);
      await h.unmount();
    });
  });
}
