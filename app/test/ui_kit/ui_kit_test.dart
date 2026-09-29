// P2-15: Fader / Knob / Meter. DoD: mượt khi kéo nhiều ngón, chạm đúp về mặc định, gom lệnh theo frame.
import 'package:flutter/material.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/ui_kit/fader.dart';
import 'package:music_looper/ui_kit/frame_throttle.dart';
import 'package:music_looper/ui_kit/knob.dart';
import 'package:music_looper/ui_kit/meter.dart';

Widget host(List<Widget> children) => MaterialApp(
  home: Scaffold(
    body: Center(
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [for (final c in children) SizedBox(width: 60, height: 300, child: c)],
      ),
    ),
  ),
);

void main() {
  group('FrameThrottle', () {
    testWidgets('giá trị đầu gửi ngay, các giá trị sau trong frame gộp làm 1 ở frame kế', (tester) async {
      final got = <int>[];
      final th = FrameThrottle<int>(got.add);
      th.add(1);
      th.add(2);
      th.add(3);
      expect(got, [1], reason: 'lệnh đầu tiên không bị trễ');
      await tester.pump();
      expect(got, [1, 3]);
      await tester.pump();
      expect(got, [1, 3], reason: 'không có giá trị mới thì không gửi');
      await tester.pump();
      th.add(4);
      expect(got, [1, 3, 4]);
      th.dispose();
    });
  });

  group('Fader', () {
    testWidgets('kéo lên → tăng; onChanged ≤ 1 lần mỗi frame', (tester) async {
      final values = <double>[];
      await tester.pumpWidget(host([Fader(value: 0.5, onChanged: values.add)]));
      final g = await tester.startGesture(tester.getCenter(find.byType(Fader)));
      for (var i = 0; i < 10; i++) {
        await g.moveBy(const Offset(0, -5)); // 10 lần move trong cùng 1 frame
      }
      expect(values.length, 1);
      await tester.pump();
      expect(values.length, 2);
      expect(values.last, greaterThan(0.5));
      expect(values.last, closeTo(0.5 + 50 / 276, 1e-3)); // 276 px = hết dải (300 - 24)
      await g.up();
    });

    testWidgets('chạm đúp → về mặc định', (tester) async {
      final values = <double>[];
      var ended = <double>[];
      await tester.pumpWidget(host([Fader(value: 0.2, onChanged: values.add, onChangeEnd: (v) => ended.add(v))]));
      final c = tester.getCenter(find.byType(Fader));
      await tester.tapAt(c);
      await tester.tapAt(c);
      expect(values.single, closeTo(0.708, 1e-9));
      expect(ended.single, closeTo(0.708, 1e-9));
    });

    testWidgets('hai lần chạm cách xa nhau → không reset', (tester) async {
      final values = <double>[];
      await tester.pumpWidget(host([Fader(value: 0.2, onChanged: values.add)]));
      final c = tester.getCenter(find.byType(Fader));
      final g1 = await tester.createGesture();
      await g1.down(c, timeStamp: Duration.zero);
      await g1.up(timeStamp: const Duration(milliseconds: 50));
      final g2 = await tester.createGesture();
      await g2.down(c, timeStamp: const Duration(seconds: 2));
      await g2.up(timeStamp: const Duration(milliseconds: 2050));
      expect(values, isEmpty);
    });

    testWidgets('ngón thứ hai giữ trên fader → tinh chỉnh ×0.1', (tester) async {
      final values = <double>[];
      await tester.pumpWidget(host([Fader(value: 0.5, onChanged: values.add)]));
      final c = tester.getCenter(find.byType(Fader));
      final drag = await tester.startGesture(c, pointer: 1);
      final hold = await tester.startGesture(c + const Offset(0, 80), pointer: 2);
      await drag.moveBy(const Offset(0, -27.6));
      await tester.pump();
      expect(values.last, closeTo(0.51, 1e-3));
      await hold.up();
      await drag.moveBy(const Offset(0, -27.6));
      await tester.pump();
      expect(values.last, closeTo(0.61, 1e-3), reason: 'nhấc ngón giữ → về tốc độ thường');
      await drag.up();
    });

    testWidgets('kéo ra xa theo chiều ngang → tinh chỉnh', (tester) async {
      final values = <double>[];
      await tester.pumpWidget(host([Fader(value: 0.5, onChanged: values.add)]));
      final g = await tester.startGesture(tester.getCenter(find.byType(Fader)));
      await g.moveBy(const Offset(100, 0)); // ra ngoài 60 px
      await g.moveBy(const Offset(0, -27.6));
      await tester.pump();
      expect(values.last, closeTo(0.51, 1e-3));
      await g.up();
    });

    testWidgets('hai ngón kéo hai fader cùng lúc, độc lập', (tester) async {
      final a = <double>[], b = <double>[];
      await tester.pumpWidget(
        host([
          Fader(key: const Key('a'), value: 0.5, onChanged: a.add),
          Fader(key: const Key('b'), value: 0.5, onChanged: b.add),
        ]),
      );
      final ga = await tester.startGesture(tester.getCenter(find.byKey(const Key('a'))), pointer: 1);
      final gb = await tester.startGesture(tester.getCenter(find.byKey(const Key('b'))), pointer: 2);
      await ga.moveBy(const Offset(0, -27.6));
      await gb.moveBy(const Offset(0, 55.2));
      await tester.pump();
      expect(a.last, closeTo(0.6, 1e-3));
      expect(b.last, closeTo(0.3, 1e-3));
      await ga.up();
      await gb.up();
    });

    testWidgets('kéo không rebuild widget (chỉ repaint painter)', (tester) async {
      await tester.pumpWidget(host([Fader(value: 0.5, onChanged: (_) {})]));
      await tester.pump();
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);
      final g = await tester.startGesture(tester.getCenter(find.byType(Fader)));
      for (var i = 0; i < 5; i++) {
        await g.moveBy(const Offset(0, -10));
        await tester.pump();
      }
      await g.up();
      expect(rebuilt, isEmpty);
    });

    test('GainScale: 0 dB ≈ 0.708, 1 = +6 dB, 0 = -∞', () {
      expect(GainScale.toNorm(0), closeTo(0.708, 1e-3));
      expect(GainScale.toDb(1), closeTo(6, 1e-9));
      expect(GainScale.toDb(0), GainScale.minDb);
      expect(GainScale.toDb(GainScale.toNorm(-12)), closeTo(-12, 1e-9));
      expect(GainScale.label(-120), '-∞');
      expect(GainScale.label(-3), '-3.0');
    });
  });

  group('Knob', () {
    testWidgets('kéo dọc đổi giá trị, chạm đúp về giữa', (tester) async {
      final values = <double>[];
      await tester.pumpWidget(host([Knob(value: 0.2, onChanged: values.add)]));
      final g = await tester.startGesture(tester.getCenter(find.byType(Knob)));
      await g.moveBy(const Offset(0, -40)); // 200 px = hết dải
      await tester.pump();
      await g.up();
      expect(values.last, closeTo(0.4, 1e-3));
      await tester.pump(const Duration(seconds: 1));
      final c = tester.getCenter(find.byType(Knob));
      await tester.tapAt(c);
      await tester.tapAt(c);
      expect(values.last, 0.5);
    });
  });

  group('LevelMeter', () {
    testWidgets('mỗi lần báo chỉ làm bẩn boundary của meter, không rebuild', (tester) async {
      final tick = ChangeNotifier();
      var level = 0.0;
      await tester.pumpWidget(host([LevelMeter(repaint: tick, level: (_) => level)]));
      await tester.pump();
      final rebuilt = <String>[];
      widgets.debugOnRebuildDirtyWidget = (e, _) => rebuilt.add('${e.widget.runtimeType}');
      addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);

      final box = tester.renderObject(find.descendant(of: find.byType(LevelMeter), matching: find.byType(CustomPaint)));
      level = 0.8;
      tick.notifyListeners();
      expect(box.debugNeedsPaint, isTrue);
      var outer = box.parent;
      while (outer != null && !outer.isRepaintBoundary) {
        outer = outer.parent;
      }
      expect(outer!.debugNeedsPaint, isTrue, reason: 'boundary của meter');
      var above = outer.parent;
      while (above != null && !above.isRepaintBoundary) {
        above = above.parent;
      }
      expect(above!.debugNeedsPaint, isFalse, reason: 'không lan ra ngoài');
      await tester.pump();
      expect(rebuilt, isEmpty);
    });

    test('thang dB', () {
      expect(MeterPainter.norm(1), 1);
      expect(MeterPainter.norm(0.0005), 0);
      expect(MeterPainter.norm(0.5012), closeTo(0.9, 1e-3)); // -6 dB
    });
  });
}
