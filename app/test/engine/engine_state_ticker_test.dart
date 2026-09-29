// P2-02: EngineStateTicker + 64 ValueNotifier<ClipState>.
// DoD "đổi 1 ô → chỉ 1 ClipCell repaint": ở tầng này nghĩa là chỉ đúng 1 notifier báo.
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_state.dart';
import 'package:music_looper/engine/engine_state_ticker.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;
  late EngineStateTicker ticker;

  setUp(() {
    fake = createFakeEngine();
    ticker = EngineStateTicker(fake);
    addTearDown(ticker.dispose);
  });

  void publish(void Function(LeState s) edit) {
    final s = fake.state;
    edit(s);
    s.publishCounter = s.publishCounter + 1;
  }

  test('copy đủ trường LeState sang EngineState', () {
    publish((s) {
      s
        ..playing = 1
        ..beat = 6.5
        ..bpm = 97
        ..sampleRate = 48000
        ..bufferSize = 128
        ..latencyRoundTripSamples = 480
        ..cpuLoad = 0.25
        ..xrunCount = 3;
      s.masterPeak[1] = 0.5;
      s.trackPeak[3][0] = 0.75;
      s.trackPlayingSlot[3] = 2;
      s.trackClipProgress[3] = 0.4;
    });
    ticker.poll();
    final st = ticker.state;
    expect(st.playing, isTrue);
    expect(st.beat, 6.5);
    expect(st.bpm, 97);
    expect(st.latencyMs, 10);
    expect(st.bufferMs, closeTo(2.667, 0.001));
    expect(st.xrunCount, 3);
    expect(st.masterPeak[1], 0.5);
    expect(st.trackPeak[3 * 2], 0.75);
    expect(st.trackPlayingSlot[3], 2);
    expect(st.trackClipProgress[3], closeTo(0.4, 1e-6));
  });

  test('chỉ notify khi engine publish bản mới (publishCounter đổi)', () {
    var n = 0;
    ticker.addListener(() => n++);
    ticker.poll(); // lần đầu luôn có dữ liệu
    ticker.poll();
    ticker.poll();
    expect(n, 1);
    publish((s) => s.cpuLoad = 0.5);
    ticker.poll();
    expect(n, 2);
  });

  test('đổi 1 ô → chỉ đúng 1 ValueNotifier<ClipState> báo', () {
    ticker.poll();
    final fired = <String>[];
    for (var t = 0; t < LE_MAX_TRACKS; t++) {
      for (var c = 0; c < LE_MAX_SCENES; c++) {
        ticker.clip(t, c).addListener(() => fired.add('$t:$c'));
      }
    }
    publish((s) => s.clipState[2][5] = LeClipState.LE_CLIP_QUEUED_PLAY);
    ticker.poll();
    expect(fired, ['2:5']);
    expect(ticker.clip(2, 5).value, ClipState.queuedPlay);

    fired.clear();
    publish((s) => s.cpuLoad = 0.9); // state khác đổi, ô không đổi
    ticker.poll();
    expect(fired, isEmpty);
  });

  test('ClipState.fromLe khớp LeClipState', () {
    expect(ClipState.fromLe(LeClipState.LE_CLIP_EMPTY), ClipState.empty);
    expect(ClipState.fromLe(LeClipState.LE_CLIP_OVERDUBBING), ClipState.overdubbing);
    expect(ClipState.fromLe(LeClipState.LE_CLIP_QUEUED_RECORD), ClipState.queuedRecord);
    expect(ClipState.fromLe(99), ClipState.empty);
  });

  // Ticker đang chạy luôn xin frame mới → pumpAndSettle không dừng; đợi đủ lâu cho route chuyển xong.
  Future<void> settleRoute(WidgetTester tester) async {
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 600));
  }

  testWidgets('Ticker chỉ chạy khi màn dùng nó đang hiện; bị che thì tạm dừng', (tester) async {
    final nav = GlobalKey<NavigatorState>();
    await tester.pumpWidget(
      MaterialApp(
        navigatorKey: nav,
        home: const Scaffold(body: Text('home')),
      ),
    );
    expect(ticker.isActive, isFalse);

    nav.currentState!.push(
      MaterialPageRoute<void>(
        builder: (_) => EngineTickerScope(
          ticker: ticker,
          child: const Scaffold(body: Text('session')),
        ),
      ),
    );
    await settleRoute(tester);
    expect(ticker.isActive, isTrue);
    expect(ticker.isMuted, isFalse);

    nav.currentState!.push(MaterialPageRoute<void>(builder: (_) => const Scaffold(body: Text('settings'))));
    await settleRoute(tester);
    expect(ticker.isMuted, isTrue, reason: 'route khác che Session → TickerMode tắt');

    nav.currentState!.pop();
    await settleRoute(tester);
    expect(ticker.isMuted, isFalse);

    nav.currentState!.pop();
    await settleRoute(tester);
    expect(ticker.isActive, isFalse, reason: 'rời Session → dừng hẳn');
  });
}
