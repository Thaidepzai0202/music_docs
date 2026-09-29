// P2-13: EngineErrorBus (banner) + KeepScreenOn (màn hình luôn sáng khi transport chạy).
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_error_bus.dart';
import 'package:music_looper/engine/engine_state_ticker.dart';
import 'package:music_looper/engine/keep_screen_on.dart';

import '../session_harness.dart';
import 'package:music_looper/l10n/l10n.dart';

import '../test_utils.dart';

void main() {
  group('EngineErrorBus', () {
    late FakeEngineClient fake;
    late EngineErrorBus bus;
    setUp(() {
      fake = createFakeEngine();
      bus = EngineErrorBus(fake.events);
      addTearDown(bus.dispose);
    });

    test('LE_EVT_ERROR → lỗi, đóng được', () async {
      fake.emit(const EngineErrorEvent(errorCode: LeError.LE_ERR_DISK_FULL));
      await Future<void>.delayed(Duration.zero);
      expect(
        bus.messages.single.text,
        contains(S.errorText('DISK_FULL')),
        reason: 'hiện chữ đã dịch theo mã, không mã thô',
      );
      expect(bus.messages.single.level, BannerLevel.error);
      bus.dismiss(bus.messages.single.id);
      expect(bus.messages, isEmpty);
    });

    test('MEMORY_WARNING → banner nhẹ "Đã giải phóng bộ nhớ" (info); critical → vàng; cùng key thì thay thế', () async {
      fake.emit(const MemoryWarning(megabytes: 142));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages.single.text, S.bannerDaGiaiPhongBoNho('142'));
      expect(bus.messages.single.level, BannerLevel.info);
      fake.emit(const MemoryWarning(megabytes: 96, critical: true));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages.single.text, S.bannerDaGiaiPhongBoNho('96'));
      expect(bus.messages.single.level, BannerLevel.warning);
      // memory.pressure của engine (Fake giống engine) cũng đi qua đây.
      fake.call({'op': 'memory.pressure', 'level': 'warning'});
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages.single.level, BannerLevel.info);
    });

    test('ngắt audio: hiện khi bắt đầu, tự gỡ khi hết', () async {
      fake.emit(const AudioInterrupted(began: true));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages.single.key, 'interrupt');
      fake.emit(const AudioInterrupted(began: false));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages, isEmpty);
    });

    test('Bluetooth: cùng key thì thay thế, đổi route thì gỡ', () async {
      fake.emit(const RouteChanged(wiredOrInterface: false, bluetooth: true));
      fake.emit(const RouteChanged(wiredOrInterface: false, bluetooth: true));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages.length, 1);
      fake.emit(const RouteChanged(wiredOrInterface: true, bluetooth: false));
      await Future<void>.delayed(Duration.zero);
      expect(bus.messages, isEmpty);
    });

    test('giữ tối đa 5 thông báo', () {
      for (var i = 0; i < 8; i++) {
        bus.post('lỗi $i');
      }
      expect(bus.messages.length, 5);
      expect(bus.messages.first.text, 'lỗi 3');
    });
  });

  test('KeepScreenOn: chỉ gọi native khi cờ playing ĐỔI; dispose thì tắt', () {
    final fake = createFakeEngine();
    final ticker = EngineStateTicker(fake);
    addTearDown(ticker.dispose);
    final calls = <bool>[];
    final k = KeepScreenOn(ticker, setKeepOn: (on) async => calls.add(on));
    void publish({required bool playing}) {
      fake.state
        ..playing = playing ? 1 : 0
        ..publishCounter = fake.state.publishCounter + 1;
      ticker.poll();
    }

    publish(playing: false);
    publish(playing: true);
    publish(playing: true);
    publish(playing: true);
    publish(playing: false);
    publish(playing: true);
    expect(calls, [true, false, true]);
    k.dispose();
    expect(calls, [true, false, true, false]);
  });

  testWidgets('banner Session: lỗi engine hiện và đóng được bằng ×', (tester) async {
    final h = SessionHarness(tester);
    await h.pump();
    h.fake.emit(const EngineErrorEvent(errorCode: LeError.LE_ERR_AUDIO_DEVICE));
    await tester.pump();
    expect(find.textContaining(S.errorText('AUDIO_DEVICE')), findsOneWidget);
    await tester.tap(find.descendant(of: find.byKey(const Key('session.banner')), matching: find.byIcon(Icons.close)));
    await tester.pump();
    expect(find.textContaining(S.errorText('AUDIO_DEVICE')), findsNothing);
    await h.unmount();
  });
}
