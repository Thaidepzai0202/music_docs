// Mixer strip (tab Mixer): không chuỗi nào bị cắt ("…") ở cả en và vi, textScale 1.0 — project demo + mọi chế độ
// monitor của track audio (badge Tắt / Auto / Bật).
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/mixer/mixer_panel.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/l10n/l10n.dart';
import 'package:music_looper/model/project.dart';

import '../session_harness.dart';
import 'real_fonts.dart';

void main() {
  final skip = !realFontsAvailable;
  setUpAll(() async {
    if (!skip) await loadRealFonts(); // đo bằng font thật (font test là ô vuông, rộng hơn nhiều)
  });

  for (final locale in L10n.supportedLocales) {
    testWidgets('[${locale.languageCode}] mixer strip: mọi chữ hiện đủ, không "…"', skip: skip, (tester) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale);
      await tester.tap(find.byKey(const Key('panel.tab.mixer')));
      await h.settle();
      for (final mode in MonitorMode.values) {
        for (var t = 4; t < 8; t++) {
          h.c.read(projectControllerProvider.notifier).setMonitor(t, mode);
        }
        await h.settle();
        final cut = [
          for (final p in tester.renderObjectList<RenderParagraph>(
            find.descendant(of: find.byType(MixerPanel), matching: find.byType(RichText)),
          ))
            if (p.didExceedMaxLines) p.text.toPlainText(),
        ];
        expect(cut, isEmpty, reason: 'monitor ${mode.name}: chữ bị cắt $cut');
      }
      await h.unmount();
    });
  }
}
