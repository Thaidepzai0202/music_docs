// P2-34 (07 §4.1e): Browser kiểu thư mục như Ableton — danh mục, thư mục theo `category` + breadcrumb, tìm kiếm toàn
// thư viện (không dấu), chạm → nghe thử (preview.play, 🎧 bật / tắt), nhấn giữ → ★ (settings.json), Bản thu của tôi,
// kéo thả vào header track hoặc ô clip.
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/features/projects/demo_project.dart';
import 'package:music_looper/features/session/session_ui.dart';
import 'package:music_looper/features/settings/app_settings.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/names.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';

class _Library extends LibraryRepository {
  @override
  Future<LibraryManifest> manifest() async => testLibrary();

  @override
  Future<String> importLoop(LibraryItem loop, {required String projectDir, required String clipId}) async =>
      'audio/$clipId.wav';
}

void main() {
  final vi = lookupAppLocalizations(const Locale('vi'));

  Future<SessionHarness> browser(WidgetTester tester, {Project? project, int track = 5}) async {
    final h = SessionHarness(tester);
    await h.pump(project: project, extraOverrides: [libraryRepositoryProvider.overrideWithValue(_Library())]);
    await tester.tap(find.byKey(Key('header.name.$track')));
    await tester.tap(find.byKey(const Key('panel.tab.browser')));
    await h.settle();
    return h;
  }

  Future<void> tapKey(WidgetTester tester, SessionHarness h, String key) async {
    await tester.tap(find.byKey(Key(key)));
    await h.settle();
  }

  List<Map<String, dynamic>> previews(SessionHarness h) => [
    for (final c in h.fake.calls)
      if (c.op == 'preview.play') c.request,
  ];

  test('tìm không dấu: foldSearch bỏ dấu tiếng Việt, không phân biệt hoa thường', () {
    expect(foldSearch('Bộ Gõ Điện'), 'bo go dien');
    expect(foldSearch('Kèn & sáo'), 'ken & sao');
  });

  testWidgets('danh mục + thư mục theo category + breadcrumb: Nhạc cụ › Synth → Tone tổng hợp; breadcrumb về gốc', (
    tester,
  ) async {
    final h = await browser(tester);
    expect(find.byKey(const Key('browser.kit_808')), findsOneWidget, reason: 'mặc định mở Trống');
    expect(find.byKey(const Key('browser.click_120')), findsNothing);
    await tapKey(tester, h, 'browser.cat.instruments');
    expect(find.byKey(const Key('browser.folder.synth')), findsOneWidget);
    expect(find.byKey(const Key('browser.inst_synth')), findsNothing, reason: 'mục nằm trong thư mục con');
    await tapKey(tester, h, 'browser.folder.synth');
    expect(find.byKey(const Key('browser.inst_synth')), findsOneWidget);
    expect(find.text('Synth'), findsWidgets, reason: 'breadcrumb Nhạc cụ › Synth');
    await tapKey(tester, h, 'browser.crumb.0');
    expect(find.byKey(const Key('browser.folder.synth')), findsOneWidget);
    await tapKey(tester, h, 'browser.cat.loops');
    expect(find.byKey(const Key('browser.click_120')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('tìm kiếm trên toàn thư viện (không dấu, tên mọi ngôn ngữ, tag); không có kết quả → báo', (tester) async {
    final h = await browser(tester);
    await tester.enterText(find.byKey(const Key('browser.search')), 'bo go');
    await h.settle();
    expect(find.byKey(const Key('browser.kit_perc')), findsOneWidget);
    expect(find.byKey(const Key('browser.kit_808')), findsNothing);
    await tester.enterText(find.byKey(const Key('browser.search')), 'click');
    await h.settle();
    expect(find.byKey(const Key('browser.click_120')), findsOneWidget);
    expect(find.byKey(const Key('browser.click_100')), findsOneWidget);
    await tester.enterText(find.byKey(const Key('browser.search')), 'zzzz');
    await h.settle();
    expect(find.text(vi.browserKhongCoKetQua), findsOneWidget);
    await tapKey(tester, h, 'browser.search.clear');
    expect(find.byKey(const Key('browser.kit_808')), findsOneWidget);
    await h.unmount();
  });

  testWidgets('chạm mục → preview.play (kit: sfz, loop: audio, base mặc định library); 🎧 tắt → không nghe; rời '
      'Browser → preview.stop', (tester) async {
    final h = await browser(tester);
    await tester.tap(find.byKey(const Key('browser.kit_808')));
    await tester.pump();
    expect(previews(h).last['source'], {'kind': 'sfz', 'path': 'kits/kit_808/kit_808.sfz'});
    await tapKey(tester, h, 'browser.cat.loops');
    await tester.tap(find.byKey(const Key('browser.click_120')));
    await tester.pump();
    expect(previews(h).last['source'], {'kind': 'audio', 'file': 'loops/clip_click_4beats_120.wav'});

    await tapKey(tester, h, 'browser.autoPreview');
    expect(h.c.read(settingsProvider).autoPreview, isFalse);
    final n = previews(h).length;
    await tester.tap(find.byKey(const Key('browser.click_100')));
    await tester.pump();
    expect(previews(h).length, n);

    await tapKey(tester, h, 'panel.tab.mixer');
    expect(h.fake.calls.last.op, 'preview.stop');
    await h.unmount();
  });

  testWidgets('nhấn giữ → ★ Yêu thích (lưu settings), danh mục ★ liệt kê; nhấn giữ lần nữa → bỏ', (tester) async {
    final h = await browser(tester);
    await tester.longPress(find.byKey(const Key('browser.kit_808')));
    await h.settle();
    expect(find.text(vi.browserDaThemYeuThich('Kit 808')), findsOneWidget);
    expect(h.c.read(settingsProvider).favorites, ['kit:kit_808']);
    await tester.pump(const Duration(seconds: 5)); // SnackBar đóng (đang che cột danh mục)
    await h.settle();
    await tapKey(tester, h, 'browser.cat.favorites');
    expect(find.byKey(const Key('browser.kit_808')), findsOneWidget);
    await tester.longPress(find.byKey(const Key('browser.kit_808')));
    await h.settle();
    expect(h.c.read(settingsProvider).favorites, isEmpty);
    expect(find.text(vi.browserChuaCoYeuThich), findsOneWidget);
    await h.unmount();
  });

  testWidgets('Bản thu của tôi: nhạc cụ tự thu của project; nghe thử bằng file nguồn (base project)', (tester) async {
    final project = localizeDemo(demoProject(), vi, library: testLibrary()).copyWith(
      userInstruments: const [
        UserInstrument(id: 'i_la', name: 'Tiếng la', source: 'instruments/i_la/source.caf', rootNote: 57),
      ],
    );
    final h = await browser(tester, project: project);
    await tapKey(tester, h, 'browser.cat.recordings');
    expect(find.text('Tiếng la'), findsOneWidget);
    await tester.tap(find.byKey(const Key('browser.i_la')));
    await tester.pump();
    expect(previews(h).last['source'], {'kind': 'audio', 'file': 'instruments/i_la/source.caf', 'base': 'project'});
    await h.unmount();
  });

  testWidgets('kéo thả: kit vào header track → track nhạc cụ (header sáng viền khi kéo qua); loop vào ô → clip ở '
      'đúng ô; ô đã có clip → báo', (tester) async {
    final h = await browser(tester);
    Future<void> dragTo(String id, Offset target, {void Function()? whileOver}) async {
      final g = await tester.startGesture(tester.getCenter(find.byKey(Key('browser.drag.$id'))));
      final from = tester.getCenter(find.byKey(Key('browser.drag.$id')));
      for (var k = 1; k <= 12; k++) {
        await g.moveTo(Offset.lerp(from, target, k / 12)!);
        await tester.pump();
      }
      whileOver?.call();
      await g.up();
      await h.settle();
    }

    await dragTo(
      'kit_808',
      tester.getCenter(find.byKey(const Key('header.name.6'))),
      whileOver: () => expect(h.c.read(sessionUiProvider).dragTarget, const CellRef(6, -1)),
    );
    final t6 = h.session.project.trackAt(6)!;
    expect([t6.kind, t6.name], [TrackKind.instrument, 'Kit 808']);

    await tapKey(tester, h, 'browser.cat.loops');
    await dragTo('click_120', tester.getCenter(find.byKey(const Key('cell.4.2'))));
    expect(h.session.project.trackAt(4)!.clipAt(2), isA<AudioClip>());
    expect(h.c.read(sessionUiProvider).dragTarget, isNull);

    await tester.pump(const Duration(seconds: 5)); // SnackBar "Đã thêm" đóng
    await h.settle();
    await dragTo('click_100', tester.getCenter(find.byKey(const Key('cell.4.2'))));
    expect(find.text(vi.browserODaCoClip(3)), findsOneWidget);
    await h.unmount();
  });
}
