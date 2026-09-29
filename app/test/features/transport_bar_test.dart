// P2-10: transport bar — play/stop, BPM (kéo + tap), quantize, metronome, count-in, vị trí, CPU/xrun.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/features/session/tap_tempo.dart';
import 'package:music_looper/features/session/widgets/transport_bar.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

void main() {
  group('TapTempo', () {
    Duration ms(int v) => Duration(milliseconds: v);

    test('4 lần chạm cách 500 ms → 120 BPM', () {
      final t = TapTempo();
      expect(t.tap(ms(0)), isNull);
      expect(t.tap(ms(500)), 120);
      expect(t.tap(ms(1000)), 120);
      expect(t.tap(ms(1500)), 120);
    });

    test('chỉ lấy 4 lần chạm gần nhất: tempo cũ không kéo trung bình', () {
      final t = TapTempo();
      for (final v in [0, 1000, 2000]) {
        t.tap(ms(v)); // 60 BPM
      }
      t.tap(ms(2500));
      t.tap(ms(3000));
      // 4 lần gần nhất lúc này: 2000, 2500, 3000, 3500 → mọi khoảng 500 ms.
      expect(t.tap(ms(3500)), 120);
    });

    test('nghỉ > 2 s → bắt đầu lại', () {
      final t = TapTempo();
      t.tap(ms(0));
      t.tap(ms(500));
      expect(t.tap(ms(5000)), isNull);
      expect(t.tap(ms(5250)), 240);
    });

    test('kẹp 20–300', () {
      final a = TapTempo();
      a.tap(ms(0));
      expect(a.tap(ms(100)), 300);
      final b = TapTempo(resetAfter: const Duration(seconds: 10));
      b.tap(ms(0));
      expect(b.tap(ms(5000)), 20);
    });
  });

  test('vị trí bar.beat đếm từ 1', () {
    expect(TransportReadoutPainter.position(0, 4), '1.1');
    expect(TransportReadoutPainter.position(3.99, 4), '1.4');
    expect(TransportReadoutPainter.position(4, 4), '2.1');
    expect(TransportReadoutPainter.position(13.5, 3), '5.2');
  });

  testWidgets('▶ ■ gửi TRANSPORT_PLAY / STOP ngay khi chạm', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    var n = h.fake.sent.length;
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('session.play'))));
    expect(h.sentSince(n).single.name, 'TRANSPORT_PLAY');
    await g.up();
    n = h.fake.sent.length;
    await tester.tap(find.byKey(const Key('session.stop')));
    expect(h.sentSince(n).single.name, 'TRANSPORT_STOP');
    await h.unmount();
  });

  testWidgets('kéo BPM: tối đa 1 lệnh SET_BPM mỗi frame; chạm đúp về 120', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    final n = h.fake.sent.length;
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('transport.bpm'))));
    for (var i = 0; i < 10; i++) {
      await g.moveBy(const Offset(0, -8)); // 10 lần move trong 1 frame
    }
    await tester.pump();
    await g.up();
    await tester.pump();
    final bpms = h.sentSince(n).where((s) => s.name == 'SET_BPM').toList();
    expect(bpms.length, 2, reason: 'gửi ngay 1 + gộp 1 ở frame sau');
    expect(bpms.last.d0, closeTo(110 + 80 / 4, 0.01)); // demo 110 BPM, kéo lên 80 px = +20 BPM
    expect(h.session.project.transport.bpm, closeTo(130, 0.01));
    expect(find.text('130.0 BPM'), findsOneWidget);

    await tester.pump(const Duration(seconds: 1));
    final c = tester.getCenter(find.byKey(const Key('transport.bpm')));
    await tester.tapAt(c);
    await tester.tapAt(c);
    await tester.pump();
    expect(h.session.project.transport.bpm, 120);
    await h.unmount();
  });

  testWidgets('TAP 4 lần cách 500 ms → SET_BPM 120', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    final at = tester.getCenter(find.byKey(const Key('transport.tap')));
    for (var i = 0; i < 4; i++) {
      final g = await tester.createGesture();
      await g.down(at, timeStamp: Duration(milliseconds: 500 * i));
      await g.up(timeStamp: Duration(milliseconds: 500 * i + 40));
    }
    await tester.pump();
    expect(h.fake.sent.last.name, 'SET_BPM');
    expect(h.fake.sent.last.d0, 120);
    expect(h.session.project.transport.bpm, 120);
    await h.unmount();
  });

  testWidgets('quantize / metronome / count-in → lệnh + model', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.quantize')));
    await h.settle();
    await tester.tap(find.byKey(const Key('quantize.quarter')));
    await h.settle();
    expect([h.fake.sent.last.name, h.fake.sent.last.i0], ['SET_QUANTIZE', 3]);
    expect(h.session.project.transport.quantize, QuantizeGrid.quarter);

    await tester.tap(find.byKey(const Key('transport.metronome')));
    await h.settle();
    await tester.tap(find.byKey(const Key('metronome.recordOnly')));
    await h.settle();
    expect([h.fake.sent.last.name, h.fake.sent.last.i0], ['METRONOME', 2]);
    expect(h.fake.sent.last.f0, closeTo(0.6, 1e-6), reason: 'giữ volume của project');
    expect(h.session.project.transport.metronome.mode, MetronomeMode.recordOnly);

    // Count-in nằm chung menu metronome (07 §2).
    await tester.tap(find.byKey(const Key('transport.metronome')));
    await h.settle();
    await tester.tap(find.byKey(const Key('countIn.2')));
    await h.settle();
    expect([h.fake.sent.last.name, h.fake.sent.last.i0], ['SET_COUNT_IN', 2]);
    expect(h.session.project.transport.countInBars, 2);
    await h.unmount();
  });

  testWidgets('✎ chuyển Perform ⇄ Edit', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    expect(h.c.read(sessionUiProvider).mode, SessionMode.edit);
    expect(find.text('Edit'), findsOneWidget);
    await tester.tap(find.byKey(const Key('transport.edit')));
    await tester.pump();
    expect(h.c.read(sessionUiProvider).mode, SessionMode.perform);
    await h.unmount();
  });
}
