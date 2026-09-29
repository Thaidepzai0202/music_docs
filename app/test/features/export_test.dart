// P3-20: ghi buổi jam (REC master ở transport + tab trong sheet) và export scene (job có tiến độ) → share sheet.
// File nằm ở Documents/Exports/.
import 'dart:io';

import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/export/jam_recorder.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/services/service_providers.dart';
import 'package:music_looper/services/share_service.dart';
import 'package:music_looper/features/session/project_controller.dart';

import '../session_harness.dart';

class RecordingShare implements ShareService {
  final calls = <(List<String>, Rect?)>[];

  @override
  Future<void> shareFiles(List<String> paths, {Rect? origin}) async => calls.add((paths, origin));
}

void main() {
  late Directory exports;
  late RecordingShare share;

  setUp(() {
    exports = Directory.systemTemp.createTempSync('exports_');
    share = RecordingShare();
  });
  tearDown(() => exports.deleteSync(recursive: true));

  Future<SessionHarness> pump(WidgetTester tester, {void Function(FakeEngineClient)? setup}) async {
    final h = SessionHarness(tester);
    h.fake.audioStart(); // như app thật: audio đã chạy khi vào Session (export.jamStart cần audio, 05 §3)
    setup?.call(h.fake);
    await h.pump(
      extraOverrides: [exportsDirProvider.overrideWithValue(exports), shareServiceProvider.overrideWithValue(share)],
    );
    return h;
  }

  Future<void> openSheet(WidgetTester tester, SessionHarness h, {bool sceneTab = false}) async {
    await tester.tap(find.byKey(const Key('transport.more')));
    await h.settle();
    await tester.tap(find.byKey(const Key('more.export')));
    await h.settle();
    if (sceneTab) {
      await tester.tap(find.byKey(const Key('export.tab.scene')));
      await h.settle();
    }
  }

  testWidgets('REC master ở transport: jamStart → đồng hồ chạy → jamStop → SnackBar "Chia sẻ" → share sheet', (
    tester,
  ) async {
    final h = await pump(tester);
    await tester.tap(find.byKey(const Key('transport.jam')));
    await tester.pump();
    final start = h.fake.calls.last.request;
    expect(start['op'], 'export.jamStart');
    expect(start['path'] as String, matches(RegExp('^${RegExp.escape(exports.path)}/jam_\\d{8}_\\d{6}\\.wav\$')));
    expect(h.fake.jamRunning, isTrue);

    h.fake.advanceSeconds(3);
    await tester.pump(const Duration(seconds: 1));
    await tester.tap(find.byKey(const Key('transport.jam')));
    await tester.pump();
    expect(h.fake.calls.last.op, 'export.jamStop');
    expect(File(start['path'] as String).existsSync(), isTrue);
    expect(find.textContaining('Đã ghi jam_'), findsOneWidget);
    expect(find.textContaining('(0:03)'), findsOneWidget);

    await tester.pump(const Duration(milliseconds: 500)); // SnackBar hiện xong mới chạm được
    await tester.tap(find.text('Chia sẻ'));
    await tester.pump();
    expect(share.calls.single.$1, [start['path']]);
    await tester.pump(const Duration(seconds: 5)); // SnackBar tự đóng
    await h.unmount();
  });

  testWidgets('tab "Ghi buổi jam" trong sheet: REC → dừng → mở share sheet ngay, hiện dòng kết quả', (tester) async {
    final h = await pump(tester);
    await openSheet(tester, h);
    await tester.tap(find.byKey(const Key('export.jam.rec')));
    await tester.pump();
    expect(find.byKey(const Key('export.jam.elapsed')), findsOneWidget);
    h.fake.advanceSeconds(65);
    await tester.tap(find.byKey(const Key('export.jam.rec')));
    await tester.pump();
    expect(share.calls, hasLength(1));
    expect(share.calls.single.$2, isNotNull, reason: 'iPad cần điểm neo popover');
    expect(find.byKey(const Key('export.jam.result')), findsOneWidget);
    expect(find.textContaining('1:05'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('export scene: chọn bar/định dạng/stems → export.scene job → tiến độ → xong → share mọi file', (
    tester,
  ) async {
    final h = await pump(tester, setup: (f) => f.manualJobOps.add('export.scene'));
    await openSheet(tester, h, sceneTab: true);
    await tester.tap(find.text('2').last);
    await tester.tap(find.text('M4A'));
    await tester.tap(find.byKey(const Key('export.scene.stems')));
    await tester.pump();
    await tester.tap(find.widgetWithText(FilledButton, 'Export'));
    await tester.pump();

    final req = h.fake.calls.lastWhere((c) => c.op == 'export.scene').request;
    expect(req['scene'], 0);
    expect(req['bars'], 2);
    expect(req['format'], 'm4a');
    expect(req['stems'], isTrue);
    expect(req['path'] as String, startsWith('${exports.path}/'));
    expect(req['path'] as String, endsWith('.m4a'));
    expect(find.byKey(const Key('export.scene.progress')), findsOneWidget);

    final id = h.fake.lastJobId;
    // JobTracker được tạo trong runAsync lúc mở project → event đi qua zone thật.
    Future<void> realTick() => tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    h.fake.emit(JobProgress(jobId: id, progress: 0.5));
    await realTick();
    await tester.pump();
    expect(find.text('50%'), findsOneWidget);
    final result = {
      'file': req['path'],
      'seconds': 4.0,
      'stems': ['${exports.path}/a_t1.m4a', '${exports.path}/a_t2.m4a'],
    };
    h.fake.completeJob(id, result: result);
    await realTick();
    await tester.pump();
    await realTick();
    await tester.pump();
    expect(share.calls.single.$1, [req['path'], ...(result['stems']! as List)]);
    expect(find.byKey(const Key('export.scene.result')), findsOneWidget);
    expect(find.textContaining('+ 2 stem'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('export scene bị clip (clippedSamples > 0) → SnackBar gợi ý giảm gain master, không chặn', (
    tester,
  ) async {
    final h = await pump(tester, setup: (f) => f.exportClippedSamples = 1200);
    await openSheet(tester, h, sceneTab: true);
    await tester.tap(find.widgetWithText(FilledButton, 'Export'));
    await tester.pump();
    // Job giả tự xong bằng Timer (zone test) → JOB_DONE tới JobTracker (zone thật): chạy xen kẽ cả hai.
    for (var i = 0; i < 6; i++) {
      await tester.pump(const Duration(milliseconds: 50));
      await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    }
    await tester.pump();
    expect(find.text(S.exportClippedSamples(1200)), findsOneWidget);
    expect(share.calls, hasLength(1), reason: 'vẫn mở share sheet');
    await tester.pump(const Duration(seconds: 5));
    await h.unmount();
  });

  testWidgets('ghi jam rớt khung (droppedFrames > 0) → SnackBar thêm dòng "Mất X ms…"', (tester) async {
    final h = await pump(tester, setup: (f) => f.jamDroppedFrames = 4800); // 4800 frame @48 kHz = 100 ms
    await tester.tap(find.byKey(const Key('transport.jam')));
    await tester.pump();
    h.fake.advanceSeconds(2);
    await tester.tap(find.byKey(const Key('transport.jam')));
    await tester.pump();
    expect(find.textContaining(S.jamDroppedMs(100)), findsOneWidget);
    await tester.pump(const Duration(seconds: 5));
    await h.unmount();
  });

  testWidgets('huỷ export: job.cancel, không báo lỗi; scene chưa có clip → nút Export tắt', (tester) async {
    final h = await pump(tester, setup: (f) => f.manualJobOps.add('export.scene'));
    await openSheet(tester, h, sceneTab: true);
    await tester.tap(find.widgetWithText(FilledButton, 'Export'));
    await tester.pump();
    await tester.tap(find.byKey(const Key('export.scene.cancel')));
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 10)));
    await tester.pump();
    // Sau JOB_FAILED(JOB_CANCELLED), JobTracker hỏi thêm job.result để lấy message → bỏ qua lệnh đó.
    expect(h.fake.calls.lastWhere((c) => c.op != 'job.result').op, 'job.cancel');
    expect(find.byKey(const Key('export.scene.progress')), findsNothing);
    expect(find.textContaining('lỗi'), findsNothing);
    expect(share.calls, isEmpty);

    await tester.tap(find.byKey(const Key('export.scene.pick')));
    await h.settle();
    await tester.tap(find.textContaining('7. ').last); // scene 7 trong demo không có clip
    await h.settle();
    expect(tester.widget<FilledButton>(find.widgetWithText(FilledButton, 'Export')).onPressed, isNull);
    expect(find.text('Scene này chưa có clip'), findsOneWidget);
    await h.unmount();
  });

  testWidgets('đóng project khi đang ghi jam → tự jamStop', (tester) async {
    final h = await pump(tester);
    await tester.tap(find.byKey(const Key('transport.jam')));
    await tester.pump();
    expect(h.fake.jamRunning, isTrue);
    h.c.read(projectControllerProvider.notifier).close();
    await tester.pump();
    expect(h.fake.jamRunning, isFalse);
    expect(h.fake.calls.map((c) => c.op), contains('export.jamStop'));
    await tester.pumpWidget(const SizedBox());
  });

  test('tên file: bỏ ký tự cấm, dấu thời gian, m:ss', () {
    expect(safeFileName(' My: Jam/1 '), 'My_Jam1');
    expect(safeFileName('///'), 'export');
    expect(fileStamp(DateTime(2026, 9, 29, 7, 5, 3)), '20260929_070503');
    expect(formatSeconds(65.4), '1:05');
  });
}
