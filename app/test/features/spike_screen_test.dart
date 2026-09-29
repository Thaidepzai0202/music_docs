// P0-05 DoD: mọi control gửi đúng lệnh (FakeEngineClient ghi lại), không setState cho dữ liệu 60Hz.
// P0-06: xin quyền mic trước le_audio_start, bị từ chối thì hướng dẫn mở Settings.
import 'dart:ffi';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter/widgets.dart' as widgets show debugOnRebuildDirtyWidget;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/app/app.dart';
import 'package:music_looper/engine/engine_providers.dart';
import 'package:music_looper/features/spike/live_panel.dart';
import 'package:music_looper/features/spike/spike_screen.dart';

import '../test_utils.dart';

void main() {
  late FakeEngineClient fake;

  Future<void> pumpSpike(WidgetTester tester) async {
    useIpad8Screen(tester);
    await tester.pumpWidget(ProviderScope(overrides: engineOverrides(fake), child: const LoopCoreApp()));
  }

  /// Gỡ cây widget (dừng Ticker) trước khi fake bị dispose.
  Future<void> unmount(WidgetTester tester) => tester.pumpWidget(const SizedBox());

  FakeSend lastSend() => fake.sent.last;

  setUp(() => fake = createFakeEngine());

  group('P0-06 quyền mic', () {
    testWidgets('xin quyền TRƯỚC le_audio_start', (tester) async {
      final startCountAtPermission = <int>[];
      final calls = mockEnginePlatform(
        permission: 'undetermined',
        onCall: (_) => startCountAtPermission.add(fake.audioStartCount),
      );
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.audio')));
      await tester.pump();
      expect(calls, ['micPermission', 'requestMicPermission']);
      expect(startCountAtPermission, [0, 0], reason: 'chưa được gọi le_audio_start khi đang xin quyền');
      expect(fake.audioRunning, isTrue);
      expect(find.text('Stop audio'), findsOneWidget);

      await tester.tap(find.byKey(const Key('spike.audio')));
      await tester.pump();
      expect(fake.audioRunning, isFalse);
      await unmount(tester);
    });

    testWidgets('bị từ chối → không start, dialog mở Settings', (tester) async {
      final calls = mockEnginePlatform(permission: 'denied');
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.audio')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
      expect(fake.audioStartCount, 0);
      expect(find.text('Cần quyền micro'), findsOneWidget);
      await tester.tap(find.byKey(const Key('spike.openSettings')));
      await tester.pump();
      expect(calls.last, 'openAppSettings');
      await tester.pump(const Duration(milliseconds: 300));
      await unmount(tester);
    });
  });

  group('P0-05 control → lệnh', () {
    setUp(() => mockEnginePlatform());

    testWidgets('màn hiện apiVersion = 1', (tester) async {
      await pumpSpike(tester);
      expect(find.text('apiVersion = 1'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('buffer 256 → spike.setBufferSize', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.text('256'));
      await tester.pump();
      expect(fake.calls.map((c) => c.request), anyElement(equals({'op': 'spike.setBufferSize', 'frames': 256})));
      expect(fake.readState().bufferSize, 256);
      await unmount(tester);
    });

    testWidgets('sine: bật/tắt, tần số log 50–2000 Hz, gain', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.sine.on')));
      await tester.pump();
      expect(lastSend().type, LeCommandType.LE_CMD_SPIKE_SINE);
      expect(lastSend().f0, closeTo(440, 0.01));
      expect(lastSend().f1, closeTo(0.3, 1e-9));

      tester.widget<Slider>(find.byKey(const Key('spike.sine.freq'))).onChanged!(1.0);
      await tester.pump();
      expect(lastSend().f0, closeTo(2000, 0.01));
      tester.widget<Slider>(find.byKey(const Key('spike.sine.freq'))).onChanged!(0.0);
      expect(lastSend().f0, closeTo(50, 0.01));

      tester.widget<Slider>(find.byKey(const Key('spike.sine.gain'))).onChanged!(0.5);
      await tester.pump();
      expect(lastSend().f1, 0.5);

      await tester.tap(find.byKey(const Key('spike.sine.on')));
      await tester.pump();
      expect(lastSend().f1, 0, reason: 'gain 0 = tắt sine');
      await unmount(tester);
    });

    testWidgets('tải giả lập: chỉ gửi khi số voice đổi', (tester) async {
      await pumpSpike(tester);
      final before = fake.sent.length;
      final slider = tester.widget<Slider>(find.byKey(const Key('spike.voices')));
      slider.onChanged!(64);
      await tester.pump();
      tester.widget<Slider>(find.byKey(const Key('spike.voices'))).onChanged!(64.2);
      expect(fake.sent.length, before + 1);
      expect(lastSend().type, LeCommandType.LE_CMD_SPIKE_LOAD_VOICES);
      expect(lastSend().i0, 64);
      await unmount(tester);
    });

    testWidgets('thu 4 giây → SPIKE_RECORD 4000ms, phát loop bật/tắt', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.record')));
      await tester.pump();
      expect(lastSend().type, LeCommandType.LE_CMD_SPIKE_RECORD);
      expect(lastSend().i0, 4000);
      expect(find.text('Đang thu 4 giây…'), findsOneWidget);
      await tester.pump(const Duration(seconds: 4));
      expect(find.text('Thu 4 giây'), findsOneWidget);

      await tester.tap(find.byKey(const Key('spike.loop')));
      await tester.pump();
      expect(lastSend().type, LeCommandType.LE_CMD_SPIKE_PLAY_RECORD);
      expect(lastSend().i0, 1);
      await tester.tap(find.byKey(const Key('spike.loop')));
      await tester.pump();
      expect(lastSend().i0, 0);
      await unmount(tester);
    });

    testWidgets('passthrough: cảnh báo tai nghe trước khi bật', (tester) async {
      await pumpSpike(tester);
      final before = fake.sent.length;
      await tester.tap(find.byKey(const Key('spike.passthrough')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
      expect(find.text('Cắm tai nghe trước'), findsOneWidget);
      expect(fake.sent.length, before, reason: 'chưa xác nhận thì chưa gửi');
      await tester.tap(find.byKey(const Key('spike.passthrough.confirm')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));
      expect(lastSend().type, LeCommandType.LE_CMD_SPIKE_PASSTHROUGH);
      expect(lastSend().i0, 1);

      await tester.tap(find.byKey(const Key('spike.passthrough')));
      await tester.pump();
      expect(lastSend().i0, 0, reason: 'tắt thì không cần hỏi');
      await unmount(tester);
    });

    testWidgets('đo latency → job → hiện JSON kết quả', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.latency')));
      await tester.pump(); // gửi call + setState
      await tester.pump(const Duration(milliseconds: 10)); // Timer job → JobDone → job.result
      await tester.pump();
      expect(fake.calls.map((c) => c.op), containsAllInOrder(['spike.latencyLoopback', 'job.result']));
      expect(find.textContaining('"measuredSamples": 492'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('stretch bench → 13 zone −18…+18, formant, saveDir Documents/spike', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.stretch')));
      await tester.pump();
      final req = fake.calls.firstWhere((c) => c.op == 'spike.stretchBench').request;
      expect(req['semitones'], [-18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18]);
      expect(req['formant'], true);
      expect(req['saveDir'], '/docs/spike');
      expect(req.containsKey('blockMs'), isFalse);
      expect(req.containsKey('cheaper'), isFalse);
      await tester.pump(const Duration(milliseconds: 10));
      await tester.pump();
      expect(find.textContaining('"msTotal": 1850'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('kết quả latency có dòng tóm tắt ở đầu', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.latency')));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 10));
      await tester.pump();
      expect(find.textContaining('Round-trip đo được: 10.25 ms'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('tiến độ job hiện theo %', (tester) async {
      fake = createFakeEngine(autoCompleteJobs: false);
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.latency')));
      await tester.pump();
      final id = fake.calls.last.request['op'] == 'spike.latencyLoopback' ? 1 : -1;
      fake.emit(JobProgress(jobId: id, progress: 0.4));
      await tester.pump();
      expect(find.textContaining('đang chạy… 40%'), findsOneWidget);
      fake.completeJob(id);
      await tester.pump(const Duration(milliseconds: 10));
      await tester.pump();
      expect(find.textContaining('Round-trip đo được'), findsOneWidget);
      await tester.pump();
      expect(find.text('Đo latency'), findsOneWidget, reason: 'nút phải mở lại khi job xong');
      await unmount(tester);
    });

    testWidgets('latency khi audio chưa chạy (AUDIO_DEVICE) → nhắc bấm Start audio', (tester) async {
      fake.onCall = (r) => r['op'] == 'spike.latencyLoopback'
          ? {
              'ok': false,
              'error': {'code': 'AUDIO_DEVICE', 'message': 'audio not running'},
            }
          : null;
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.latency')));
      await tester.pump();
      expect(find.textContaining('bấm Start audio trước'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('tuỳ chọn stretch: block 200/50 + cheaper', (tester) async {
      await pumpSpike(tester);
      for (final k in ['spike.stretch.fineBlock', 'spike.stretch.cheaper']) {
        await tester.ensureVisible(find.byKey(Key(k)));
        await tester.pump();
        await tester.tap(find.byKey(Key(k)));
        await tester.pump();
      }
      await tester.tap(find.byKey(const Key('spike.stretch')));
      await tester.pump();
      final req = fake.calls.firstWhere((c) => c.op == 'spike.stretchBench').request;
      expect(req['blockMs'], 200);
      expect(req['intervalMs'], 50);
      expect(req['cheaper'], true);
      await tester.pump(const Duration(milliseconds: 10));
      await tester.pump();
      expect(find.textContaining('Tổng 13 zone: 1850 ms ✓'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('op chưa hiện thực (NOT_IMPLEMENTED) → báo lỗi, không treo nút', (tester) async {
      fake.onCall = (r) => r['op'] == 'spike.latencyLoopback'
          ? {
              'ok': false,
              'error': {'code': 'NOT_IMPLEMENTED', 'message': 'P0-07 chưa nối'},
            }
          : null;
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.latency')));
      await tester.pump();
      expect(find.textContaining('NOT_IMPLEMENTED'), findsOneWidget);
      expect(find.text('Đo latency'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('session mode default/measurement + session info (P0-06)', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.text('Mode measurement'));
      await tester.pump();
      expect(fake.calls.last.request, {'op': 'spike.setSessionMode', 'mode': 'measurement'});
      expect(find.textContaining('"applied": true'), findsOneWidget);

      await tester.tap(find.byKey(const Key('spike.sessionInfo')));
      await tester.pump();
      expect(fake.calls.last.op, 'spike.sessionInfo');
      expect(find.textContaining('"allowBluetoothHFP": false'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('thu xong → log số frame từ RECORDING_FINISHED', (tester) async {
      await pumpSpike(tester);
      await tester.tap(find.byKey(const Key('spike.record')));
      await tester.pump();
      await tester.pump(const Duration(seconds: 4));
      await tester.pump();
      expect(find.textContaining('Thu xong: 192000 frame (4.00 s)'), findsOneWidget);
      await unmount(tester);
    });

    testWidgets('event route/ngắt hiện trong log', (tester) async {
      await pumpSpike(tester);
      fake.emit(const RouteChanged(wiredOrInterface: false, bluetooth: true));
      fake.emit(const AudioInterrupted(began: true));
      await tester.pump();
      expect(find.textContaining('Bluetooth=có'), findsOneWidget);
      expect(find.textContaining('Audio bị ngắt'), findsOneWidget);
      await unmount(tester);
    });
  });

  testWidgets('dữ liệu 60Hz đổi → KHÔNG rebuild widget nào (07 §6.1)', (tester) async {
    mockEnginePlatform();
    await pumpSpike(tester);
    await tester.pump();

    var rebuilds = 0;
    final rebuilt = <String>[];
    widgets.debugOnRebuildDirtyWidget = (element, builtOnce) {
      rebuilds++;
      rebuilt.add(element.widget.runtimeType.toString());
    };
    addTearDown(() => widgets.debugOnRebuildDirtyWidget = null);

    for (var i = 0; i < 30; i++) {
      fake.state
        ..publishCounter = fake.state.publishCounter + 1
        ..cpuLoad = (i % 10) / 10
        ..inputPeak = (i % 7) / 7
        ..xrunCount = i;
      fake.state.masterPeak[0] = (i % 5) / 5;
      await tester.pump(const Duration(milliseconds: 16));
    }
    expect(rebuilds, 0, reason: 'widget bị rebuild: $rebuilt');
    await unmount(tester);
  });

  // Tương đương "Highlight Repaints" chạy tự động: sau 1 lần đọc state 60Hz, chỉ boundary của
  // LivePanel bị đánh dấu cần vẽ; mọi layer phía trên (tới route) giữ nguyên.
  testWidgets('state 60Hz chỉ làm bẩn RepaintBoundary của LivePanel', (tester) async {
    mockEnginePlatform();
    await pumpSpike(tester);
    await tester.pump();

    final container = ProviderScope.containerOf(tester.element(find.byType(SpikeScreen)));
    final ticker = container.read(engineStateTickerProvider);
    final painterBox = tester.renderObject(
      find.descendant(of: find.byType(LivePanel), matching: find.byType(CustomPaint)),
    );

    // Tìm boundary gần nhất phía trên painter và boundary kế tiếp (layer bao ngoài).
    RenderObject? node = painterBox;
    while (node != null && !node.isRepaintBoundary) {
      node = node.parent;
    }
    final panelBoundary = node!;
    node = panelBoundary.parent;
    while (node != null && !node.isRepaintBoundary) {
      node = node.parent;
    }
    final outerBoundary = node!;

    fake.state
      ..publishCounter = fake.state.publishCounter + 1
      ..cpuLoad = 0.42;
    ticker.poll();

    expect(painterBox.debugNeedsPaint, isTrue, reason: 'bảng live phải vẽ lại');
    expect(panelBoundary.debugNeedsPaint, isTrue);
    expect(outerBoundary.debugNeedsPaint, isFalse, reason: 'repaint leo ra ngoài LivePanel');
    await unmount(tester);
  });
}
