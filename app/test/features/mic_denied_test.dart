// 07 §4.0: từ chối quyền mic → chế độ chỉ phát: banner + "Mở Cài đặt", khoá nút thu audio (thu MIDI vẫn được);
// quay lại app đã có quyền → bật lại input, banner biến mất.
import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/engine/engine_providers.dart';

import '../session_harness.dart';

void main() {
  testWidgets('chỉ phát: banner, arm track audio + "Thu âm mới" bị khoá; cấp quyền rồi quay lại → mở khoá', (
    tester,
  ) async {
    final h = SessionHarness(tester);
    await h.pump();
    var permission = 'denied';
    final platformCalls = <String>[];
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger.setMockMethodCallHandler(EnginePlatform.channel, (
      call,
    ) async {
      platformCalls.add(call.method);
      return switch (call.method) {
        'micPermission' => permission,
        'openAppSettings' => true,
        _ => null,
      };
    });
    final audio = h.c.read(engineAudioProvider);
    await audio.start();
    await tester.pump();

    expect(h.fake.audioRunning, isTrue);
    expect(h.fake.inputEnabled, isFalse);
    expect(find.text('Chưa có quyền micro: chỉ phát được, chưa thu được'), findsOneWidget);
    await tester.tap(find.byKey(const Key('banner.action')));
    await tester.pump();
    expect(platformCalls, contains('openAppSettings'));

    // Track 5 (audio): arm bị khoá. Track 0 (instrument, thu MIDI): vẫn arm được.
    final before = h.fake.sent.length;
    await tester.tap(find.byKey(const Key('header.arm.5')));
    await tester.pump();
    expect(h.sentSince(before).where((s) => s.name == 'TRACK_ARM'), isEmpty);
    await tester.tap(find.byKey(const Key('header.arm.0')));
    await tester.pump();
    expect(h.sentSince(before).where((s) => s.name == 'TRACK_ARM').single.track, 0);

    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();
    expect(tester.widget<OutlinedButton>(find.byKey(const Key('instrument.recordNew'))).onPressed, isNull);

    // Người dùng bật mic trong Cài đặt rồi quay lại app.
    permission = 'granted';
    await audio.onForeground();
    await tester.pump();
    expect(h.fake.inputEnabled, isTrue);
    expect(find.text('Chưa có quyền micro: chỉ phát được, chưa thu được'), findsNothing);
    expect(tester.widget<OutlinedButton>(find.byKey(const Key('instrument.recordNew'))).onPressed, isNotNull);
    await tester.tap(find.byKey(const Key('header.arm.5')));
    await tester.pump();
    expect(h.fake.sent.last.name, 'TRACK_ARM');
    expect(h.fake.sent.last.track, 5);
    await h.unmount();
  });
}
