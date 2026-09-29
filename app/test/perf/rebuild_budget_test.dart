// Ngân sách rebuild của cây widget (07 §6, M2 "UI 60fps khi 8 track đang phát"): đếm số lần build từng loại widget
// khi scene 8 track đang phát, đồng thời chơi phím và kéo fader. Dữ liệu 60 Hz phải đi qua painter (repaint),
// không qua build. Chạy `flutter test test/perf/rebuild_budget_test.dart` để in bảng đếm.
import 'dart:ffi';

import 'package:flutter/material.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/model/ids.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

/// Demo + track 5–8 là bản sao track Bass → cả 8 track có clip ở scene 1.
Project eightTrackProject() {
  final d = demoProject();
  final bass = d.trackAt(1)!;
  return d.copyWith(
    tracks: [
      for (final t in d.tracks)
        if (t.index < 4)
          t
        else
          bass.copyWith(
            id: newId('t'),
            index: t.index,
            name: 'Bass ${t.index + 1}',
            color: t.color,
            clips: [for (final c in bass.clips) c.copyWith(id: newId('c'))],
          ),
    ],
  );
}

void main() {
  final counts = <String, int>{};
  void reset() => counts.clear();
  int total() => counts.values.fold(0, (a, b) => a + b);
  int count(String type) => counts[type] ?? 0;

  setUp(() {
    widgets.debugOnRebuildDirtyWidget = (element, _) {
      final name = element.widget.runtimeType.toString();
      counts[name] = (counts[name] ?? 0) + 1;
    };
  });
  tearDown(() => widgets.debugOnRebuildDirtyWidget = null);

  /// Một frame 60 Hz: nhạc chạy 1/60 s (120 BPM → 1/30 beat) rồi vẽ.
  Future<void> frame(WidgetTester tester, SessionHarness h) async {
    h.fake.advanceSeconds(1 / 60);
    await tester.pump(const Duration(microseconds: 16667));
  }

  String report(String phase) {
    final top = counts.entries.toList()..sort((a, b) => b.value.compareTo(a.value));
    return '$phase: tổng ${total()} — ${top.take(12).map((e) => '${e.key}×${e.value}').join(', ')}';
  }

  testWidgets('scene 8 track đang phát + chơi phím + kéo fader: rebuild trong ngân sách', (tester) async {
    final h = SessionHarness(tester);
    await h.pump(project: eightTrackProject());
    await tester.tap(find.byKey(const Key('header.name.1'))); // Bass → bàn phím
    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();

    // Launch scene 1 → 8 ô QueuedPlay rồi Playing ở ranh giới bar.
    final g0 = await tester.startGesture(tester.getCenter(find.byKey(const Key('scene.0'))));
    await g0.up();
    for (var i = 0; i < 30; i++) {
      await frame(tester, h);
    }
    expect(h.fake.readState().trackPlayingSlot[7], 0, reason: 'cả 8 track đang phát');

    // A. Đứng yên nghe 2 giây (120 frame, đi qua ranh giới bar, clip lặp lại).
    reset();
    for (var i = 0; i < 120; i++) {
      await frame(tester, h);
    }
    debugPrint(report('A. phát 120 frame'));
    final idle = total();

    // B. Chơi phím: 12 lần chạm/nhả trong 120 frame (Listener → NOTE_ON/OFF, painter phím repaint).
    reset();
    final kb = tester.getRect(find.byKey(const Key('instrument.keyboard')));
    for (var i = 0; i < 120; i++) {
      if (i % 10 == 0) {
        final g = await tester.startGesture(Offset(kb.left + 40 + (i ~/ 10) * 50, kb.center.dy));
        await frame(tester, h);
        await g.up();
      }
      await frame(tester, h);
    }
    debugPrint(report('B. chơi phím 12 nốt'));
    final keys = total();
    final notes = h.fake.sent.where((s) => s.name == 'NOTE_ON').length;

    // C. Kéo fader track 1 suốt 60 frame.
    await tester.tap(find.byKey(const Key('panel.tab.mixer')));
    await h.settle();
    reset();
    final g = await tester.startGesture(tester.getCenter(find.byKey(const Key('mixer.fader.0'))));
    for (var i = 0; i < 60; i++) {
      await g.moveBy(const Offset(0, -1.5));
      await frame(tester, h);
    }
    await g.up();
    await frame(tester, h);
    debugPrint(report('C. kéo fader 60 frame'));
    final fader = total();

    // D. Kéo BPM suốt 60 frame (SET_BPM ≤ 1 lệnh/frame; model đổi theo → chỉ widget hiện BPM rebuild).
    reset();
    final gb = await tester.startGesture(tester.getCenter(find.byKey(const Key('transport.bpm'))));
    for (var i = 0; i < 60; i++) {
      await gb.moveBy(const Offset(0, -2));
      await frame(tester, h);
    }
    await gb.up();
    await frame(tester, h);
    debugPrint(report('D. kéo BPM 60 frame'));
    final bpm = total();

    // Ngân sách: build chỉ khi trạng thái thật sự đổi — không theo frame.
    expect(idle, lessThanOrEqualTo(40), reason: 'phát đứng yên: chỉ ô đổi trạng thái (nếu có) rebuild');
    expect(count('TrackHeader') + count('TransportBar') + count('SessionScreen'), 0);
    expect(notes, greaterThanOrEqualTo(12));
    expect(keys, lessThanOrEqualTo(40), reason: 'chạm phím không rebuild panel');
    expect(
      fader,
      lessThanOrEqualTo(60 * 2 + 30),
      reason: 'kéo fader: chỉ nhãn dB (VLB + Text) mỗi frame + 1 lần commit',
    );
    expect(count('MixerStrip'), lessThanOrEqualTo(1));
    expect(bpm, lessThanOrEqualTo(60 * 4), reason: 'kéo BPM: chỉ phần hiện BPM, không cả transport bar');
    await h.unmount();
  });
}
