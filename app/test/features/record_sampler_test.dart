// P3-03: sheet Record-to-Sampler (07 §4.2). DoD: confidence thấp → bắt buộc chọn nốt;
// hiển thị "A3 +12 cent"; huỷ giữa chừng không làm rò file.
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/model/project.dart';

import 'package:music_looper/l10n/l10n.dart';

import '../session_harness.dart';

void main() {
  late Directory tmp;
  setUp(() {
    tmp = Directory.systemTemp.createTempSync('rec_sampler_');
    addTearDown(() {
      if (tmp.existsSync()) tmp.deleteSync(recursive: true);
    });
  });

  Future<SessionHarness> openSheet(WidgetTester tester, {void Function(SessionHarness h)? setup}) async {
    final h = SessionHarness(tester);
    setup?.call(h);
    await h.pump(dir: tmp.path);
    await tester.tap(find.byKey(const Key('header.name.2')));
    await tester.tap(find.byKey(const Key('panel.tab.instrument')));
    await h.settle();
    await tester.tap(find.byKey(const Key('instrument.recordNew')));
    await h.settle();
    return h;
  }

  Directory instrumentsDir() => Directory('${tmp.path}/instruments');
  List<FileSystemEntity> leftovers() => instrumentsDir().existsSync() ? instrumentsDir().listSync() : const [];

  Future<void> recordToReview(SessionHarness h, {double seconds = 4.2}) async {
    await h.tester.tap(find.byKey(const Key('rec.start')));
    await h.tester.pump();
    h.fake.advanceSeconds(seconds); // tới 4 giây → engine tự dừng, phát RECORDING_FINISHED(-2,-2)
    await h.tester.pump();
    await h.tester.pump(const Duration(milliseconds: 20)); // job analyze xong
    await h.tester.pump();
  }

  testWidgets('thu → tự dừng 4 s → "A3 +12 cent" → Classic → tạo (có tiến độ) → nhạc cụ gắn vào track', (tester) async {
    final h = await openSheet(tester);
    await recordToReview(h);
    final start = h.fake.calls.firstWhere((c) => c.op == 'capture.start').request;
    expect(start['maxSeconds'], 4);
    expect(start['path'] as String, startsWith('${tmp.path}/instruments/i_'));
    expect(h.fake.audioRunning, isTrue, reason: 'bật audio (có quyền mic) trước khi thu');
    expect(find.text('A3 +12 cent'), findsOneWidget);
    expect(find.byKey(const Key('rec.lowConfidence')), findsNothing);

    await tester.tap(find.text('Classic'));
    await tester.pump();
    await tester.tap(find.byKey(const Key('rec.create')));
    await tester.pump();
    expect(find.byKey(const Key('rec.progress')), findsOneWidget);
    await tester.pump(const Duration(milliseconds: 20));
    await h.settle();

    expect(find.byKey(const Key('rec.create')), findsNothing, reason: 'sheet đóng khi tạo xong');
    final create = h.fake.calls.firstWhere((c) => c.op == 'instrument.createFromRecording').request;
    expect(create['mode'], 'classic');
    expect(create.containsKey('rootNote'), isFalse, reason: 'tin cậy cao, không đổi nốt → để engine dùng nốt dò được');
    expect(create['trimStartSample'], lessThan(create['trimEndSample'] as int));
    final ui = h.session.project.userInstruments.single;
    expect([ui.rootNote, ui.cents, ui.mode], [57, 12.0, InstrumentMode.classic]);
    expect(ui.source, 'instruments/${ui.id}/source.caf');
    expect(h.session.project.trackAt(2)!.instrument, InstrumentRef.user(id: ui.id));
    expect(h.fake.calls.last.request['op'], 'track.setInstrument');
    expect(File('${tmp.path}/${ui.source}').existsSync(), isTrue, reason: 'giữ file khi thành công');
    await h.unmount();
  });

  testWidgets('confidence thấp → nút Tạo bị khoá tới khi chọn nốt; tạo kèm rootNote đã chọn', (tester) async {
    final h = await openSheet(tester, setup: (h) => h.fake.captureConfidence = 0.3);
    await recordToReview(h);
    expect(find.byKey(const Key('rec.lowConfidence')), findsOneWidget);
    expect(tester.widget<FilledButton>(find.byKey(const Key('rec.create'))).onPressed, isNull);
    // Engine không dò được → rootNote −1 (05 §3): bắt đầu chọn từ C4, không phải nốt −1.
    expect(find.text('C4 +0 cent'), findsOneWidget);
    await tester.tap(find.byKey(const Key('rec.noteUp')));
    await tester.pump();
    expect(find.text('C#4 +0 cent'), findsOneWidget);
    expect(tester.widget<FilledButton>(find.byKey(const Key('rec.create'))).onPressed, isNotNull);
    await tester.tap(find.byKey(const Key('rec.create')));
    await tester.pump(const Duration(milliseconds: 20));
    await h.settle();
    final create = h.fake.calls.firstWhere((c) => c.op == 'instrument.createFromRecording').request;
    expect(create['rootNote'], 61);
    expect(h.session.project.userInstruments.single.rootNote, 61);
    await h.unmount();
  });

  testWidgets('mẫu im lặng (analyze.silent) → báo không nghe thấy tiếng, xoá file, về bước thu', (tester) async {
    final h = await openSheet(tester, setup: (h) => h.fake.captureSilent = true);
    await recordToReview(h);
    expect(find.text(S.samplerKhongNgheThayTieng), findsOneWidget);
    expect(find.byKey(const Key('rec.start')), findsOneWidget, reason: 'thu lại được ngay');
    expect(find.byKey(const Key('rec.create')), findsNothing);
    expect(leftovers(), isEmpty, reason: 'không rò file im lặng');
    expect(h.fake.calls.where((c) => c.op == 'instrument.createFromRecording'), isEmpty);
    await h.unmount();
  });

  testWidgets('engine báo PITCH_NOT_DETECTED → quay lại bước xem, bắt chọn nốt', (tester) async {
    final h = await openSheet(tester, setup: (h) => h.fake.captureConfidence = 0.7);
    await recordToReview(h);
    h.fake.captureConfidence = 0.2; // engine đổi ý lúc render
    await tester.tap(find.byKey(const Key('rec.create')));
    await tester.pump(const Duration(milliseconds: 20));
    await tester.pump();
    expect(find.textContaining('Không dò được cao độ'), findsOneWidget);
    expect(tester.widget<FilledButton>(find.byKey(const Key('rec.create'))).onPressed, isNull);
    expect(h.session.project.userInstruments, isEmpty);
    await tester.tap(find.byKey(const Key('rec.cancel')));
    await h.settle();
    expect(leftovers(), isEmpty);
    await h.unmount();
  });

  testWidgets('bấm dừng trước 4 s → capture.stop → phân tích', (tester) async {
    final h = await openSheet(tester);
    await tester.tap(find.byKey(const Key('rec.start')));
    await tester.pump();
    h.fake.advanceSeconds(2);
    await tester.tap(find.byKey(const Key('rec.stop')));
    await tester.pump(const Duration(milliseconds: 20));
    await tester.pump();
    expect(h.fake.calls.map((c) => c.op), containsAllInOrder(['capture.start', 'capture.stop', 'capture.analyze']));
    expect(find.text('A3 +12 cent'), findsOneWidget);
    await tester.tap(find.byKey(const Key('rec.cancel')));
    await h.settle();
    await h.unmount();
  });

  testWidgets('huỷ lúc đang thu → capture.stop + xoá thư mục nhạc cụ (không rò file)', (tester) async {
    final h = await openSheet(tester);
    await tester.tap(find.byKey(const Key('rec.start')));
    await tester.pump();
    h.fake.advanceSeconds(1);
    await tester.tap(find.byKey(const Key('rec.cancel')));
    await h.settle();
    expect(h.fake.calls.last.request['op'], 'capture.stop');
    expect(h.fake.captureRunning, isFalse);
    expect(leftovers(), isEmpty);
    expect(h.session.project.userInstruments, isEmpty);
    await h.unmount();
  });

  testWidgets('huỷ lúc đang tạo → job.cancel + xoá file', (tester) async {
    final h = await openSheet(tester);
    await recordToReview(h);
    await tester.tap(find.byKey(const Key('rec.create')));
    await tester.pump(); // job vừa tạo, chưa xong (Timer 0 chưa chạy)
    expect(find.byKey(const Key('rec.progress')), findsOneWidget);
    await tester.tap(find.byKey(const Key('rec.cancel')));
    await h.settle();
    expect(h.fake.calls.map((c) => c.op), contains('job.cancel'));
    expect(leftovers(), isEmpty);
    expect(h.session.project.userInstruments, isEmpty, reason: 'job xong sau khi huỷ cũng không được gắn nhạc cụ');
    expect(find.byKey(const Key('session.grid')), findsOneWidget, reason: 'không pop nhầm màn Session');
    await h.unmount();
  });

  testWidgets('chạm ra ngoài đóng sheet ở bước xem lại → vẫn xoá file', (tester) async {
    final h = await openSheet(tester);
    await recordToReview(h);
    expect(leftovers(), isNotEmpty, reason: 'đã có source.caf');
    await tester.tapAt(const Offset(540, 40)); // vùng tối phía trên sheet
    await h.settle();
    expect(find.byKey(const Key('rec.create')), findsNothing);
    expect(leftovers(), isEmpty);
    await h.unmount();
  });
}
