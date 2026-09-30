// 07 §8: ảnh golden các màn chính ở mỗi ngôn ngữ (en, vi) để người review xem bằng mắt: nhãn dịch có vừa chỗ,
// có bị cắt "…" chỗ quan trọng không. Nạp font thật (Roboto + Material Icons từ SDK Flutter) thay font ô vuông mặc
// định của flutter_test. Font render khác nhau giữa các OS → chỉ so ảnh trên macOS.
// Chữ vẽ bằng TextPainter lấy font của theme (`painterFont`), ký hiệu dùng Icon / icon vẽ → mọi nhãn đều đọc được.
// Project demo được tạo theo ngôn ngữ của test (tên clip/track dịch lúc tạo).
// Tạo lại ảnh: flutter test --update-goldens test/ui_review/golden_screens_test.dart
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/ids.dart';

import '../session_harness.dart';
import 'real_fonts.dart';

/// Manifest thật, trả ngay (rootBundle trong test đọc bất đồng bộ → golden chụp vòng xoay).
class _GoldenLibrary extends LibraryRepository {
  @override
  Future<LibraryManifest> manifest() async => testLibrary();
}

void main() {
  final skip = !Platform.isMacOS || Platform.environment['FLUTTER_ROOT'] == null;

  setUpAll(() async {
    if (!skip) await loadRealFonts();
  });

  for (final locale in L10n.supportedLocales) {
    final lang = locale.languageCode;

    testWidgets('[$lang] Session (tab Mixer) + Settings › Audio', skip: skip, (tester) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale);
      await tester.tap(find.byKey(const Key('panel.tab.mixer')));
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/session_$lang.png'));

      await tester.tap(find.byKey(const Key('transport.more')));
      await h.settle();
      await tester.tap(find.byKey(const Key('transport.settings')));
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/settings_$lang.png'));
      // P2-35: Giấy phép — nhóm Ghi công (Salamander CC BY 3.0, VSCO-2 CE) hiện thẳng câu chữ. Đọc sẵn file giấy phép
      // bằng I/O thật (rootBundle giữ cache) để màn dựng xong trong thời gian ảo của test.
      await tester.runAsync(() async {
        for (final f in ['BIG-RUSTY-DRUMS.md', 'SALAMANDER-GRAND-PIANO.md', 'SYNTHESIZED.md', 'VSCO-2-CE.md']) {
          await rootBundle.loadString('assets/licenses/content/$f');
        }
      });
      await tester.tap(find.byKey(const Key('settings.nav.licenses')));
      for (var i = 0; i < 20 && find.byKey(const Key('licenses.credit.VSCO-2-CE.md')).evaluate().isEmpty; i++) {
        await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 50)));
        await tester.pump();
      }
      await h.settle();
      expect(find.byKey(const Key('licenses.credit.SALAMANDER-GRAND-PIANO.md')), findsOneWidget);
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/licenses_$lang.png'));
      await tester.tap(find.byKey(const Key('settings.back')));
      await h.settle();
      await h.unmount();
    });

    testWidgets('[$lang] Tab Clip: piano roll chế độ Vẽ (clip Keys)', skip: skip, (tester) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale, extraOverrides: [libraryRepositoryProvider.overrideWithValue(_GoldenLibrary())]);
      await tester.tap(find.byKey(const Key('transport.edit')));
      await tester.pump();
      await tester.tap(find.byKey(const Key('cell.2.0')));
      await h.settle();
      await tester.tap(find.byKey(const Key('midi.mode.draw')));
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/pianoroll_$lang.png'));
      await tester.tap(find.byKey(const Key('session.panel.expand'))); // ⤢ panel ~70% màn hình
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/pianoroll_expanded_$lang.png'));
      // P2-33: bàn phím dưới piano roll, ⇥ Step bật (con trỏ trên lưới, Nghỉ / ⌫).
      await tester.tap(find.byKey(const Key('midi.keyboard')));
      await h.settle();
      await tester.tap(find.byKey(const Key('clip.step')));
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/pianoroll_keyboard_$lang.png'));
      await h.unmount();
    });

    testWidgets('[$lang] Tab Browser: danh mục + thư mục + tìm kiếm + 🎧 (07 §4.1e)', skip: skip, (tester) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale, extraOverrides: [libraryRepositoryProvider.overrideWithValue(_GoldenLibrary())]);
      await tester.tap(find.byKey(const Key('header.name.5')));
      await tester.tap(find.byKey(const Key('panel.tab.browser')));
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/browser_$lang.png'));
      await h.unmount();
    });

    testWidgets('[$lang] Session project mới: pedal mode chờ vòng đầu (top bar) + nút LOOP', skip: skip, (
      tester,
    ) async {
      final h = SessionHarness(tester);
      await h.pump(project: newProject('Pedal'), locale: locale);
      await h.settle();
      await expectLater(find.byType(MaterialApp), matchesGoldenFile('goldens/session_pedal_$lang.png'));
      await h.unmount();
    });
  }
}
