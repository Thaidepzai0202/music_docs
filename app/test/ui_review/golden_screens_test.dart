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
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/ids.dart';

import '../session_harness.dart';

Future<void> loadRealFonts() async {
  final fonts = '${Platform.environment['FLUTTER_ROOT']}/bin/cache/artifacts/material_fonts';
  Future<ByteData> file(String name) async => ByteData.sublistView(File('$fonts/$name').readAsBytesSync());
  final roboto = FontLoader('Roboto');
  for (final w in ['Regular', 'Medium', 'Bold']) {
    roboto.addFont(file('Roboto-$w.ttf'));
  }
  await roboto.load();
  await (FontLoader('MaterialIcons')..addFont(file('MaterialIcons-Regular.otf'))).load();
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
      await tester.tap(find.byKey(const Key('settings.back')));
      await h.settle();
      await h.unmount();
    });

    testWidgets('[$lang] Tab Clip: piano roll chế độ Vẽ (clip Keys)', skip: skip, (tester) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale);
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
