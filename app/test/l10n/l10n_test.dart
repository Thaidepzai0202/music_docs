// 07 §8: en + vi. Hai ARB cùng key + cùng placeholder; chọn ngôn ngữ theo iOS (vi → vi, còn lại → en);
// Info.plist khai CFBundleLocalizations + câu xin quyền micro có bản en/vi; lỗi hiện theo mã đã dịch.
import 'dart:convert';
import 'dart:io';

import 'package:flutter/widgets.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/session/project_controller.dart';
import 'package:music_looper/l10n/l10n.dart';

import '../session_harness.dart';

Map<String, dynamic> readArb(String locale) =>
    jsonDecode(File('lib/l10n/app_$locale.arb').readAsStringSync()) as Map<String, dynamic>;

/// Tên placeholder ở mọi cấp của thông điệp ICU (bỏ qua thân nhánh plural/select như `{1 bar}`).
Set<String> icuPlaceholders(String icu) {
  final out = <String>{};
  var i = 0;

  int argument(int j) {
    var k = j;
    while (icu[k] != ',' && icu[k] != '}') {
      k++;
    }
    out.add(icu.substring(j, k).trim());
    if (icu[k] == '}') return k + 1;
    k = icu.indexOf(',', k + 1) + 1; // bỏ "plural"/"select"
    while (true) {
      while (icu[k].trim().isEmpty) {
        k++;
      }
      if (icu[k] == '}') return k + 1;
      k = icu.indexOf('{', k) + 1; // selector rồi thân nhánh
      var depth = 1;
      while (depth > 0) {
        if (icu[k] == '{') {
          // placeholder lồng trong nhánh
          if (depth == 1) {
            k = argument(k + 1);
            continue;
          }
          depth++;
        } else if (icu[k] == '}') {
          depth--;
        }
        k++;
      }
    }
  }

  while (i < icu.length) {
    if (icu[i] == '{') {
      i = argument(i + 1);
      continue;
    }
    i++;
  }
  return out;
}

void main() {
  final en = readArb('en');
  final vi = readArb('vi');
  Iterable<String> keys(Map<String, dynamic> arb) => arb.keys.where((k) => !k.startsWith('@'));

  test('hai ARB cùng tập key (en là file mẫu)', () {
    expect(keys(vi).toSet(), keys(en).toSet());
    expect(keys(en).length, greaterThan(300));
  });

  test('cùng placeholder từng key; khai báo @key.placeholders khớp chuỗi mẫu', () {
    final bad = <String>[];
    for (final k in keys(en)) {
      final e = icuPlaceholders(en[k] as String), v = icuPlaceholders(vi[k] as String);
      if (e.difference(v).isNotEmpty || v.difference(e).isNotEmpty) bad.add('$k: en $e ≠ vi $v');
      final declared = ((en['@$k'] as Map?)?['placeholders'] as Map?)?.keys.toSet() ?? <String>{};
      if (!declared.containsAll(e)) bad.add('$k: thiếu khai báo ${e.difference(declared)}');
    }
    expect(bad, isEmpty);
  });

  test('parser ICU: bỏ qua thân nhánh, bắt placeholder lồng', () {
    expect(icuPlaceholders('{n, plural, =1{1 bar} other{{n} bars}} · {x}'), {'n', 'x'});
    expect(icuPlaceholders('{g, select, none{None} other{{g}}}'), {'g'});
  });

  test('chọn ngôn ngữ theo iOS: vi → vi, mọi ngôn ngữ khác (hoặc không có) → en', () {
    const supported = L10n.supportedLocales;
    expect(L10n.resolve(const [Locale('vi', 'VN')], supported), const Locale('vi'));
    expect(L10n.resolve(const [Locale('fr'), Locale('vi')], supported), const Locale('en'));
    expect(L10n.resolve(const [Locale('en', 'US')], supported), const Locale('en'));
    expect(L10n.resolve(null, supported), const Locale('en'));
    expect(L10n.resolve(const [], supported), const Locale('en'));
  });

  test('lỗi theo mã: có bản dịch cho mọi LeError + mã lạ vẫn ra câu (không hiện message engine)', () {
    final e = lookupAppLocalizations(const Locale('en')), v = lookupAppLocalizations(const Locale('vi'));
    for (final code in [
      'INVALID_ARG',
      'NOT_IMPLEMENTED',
      'AUDIO_DEVICE',
      'DISK_FULL',
      'OVERDUB_UNSUPPORTED',
      'CELL_EMPTY',
    ]) {
      expect(e.errorText(code), isNot(contains(code)), reason: code);
      expect(v.errorText(code), isNot(contains(code)), reason: code);
    }
    expect(e.errorText('WHATEVER'), 'Engine error (WHATEVER)');
    expect(v.linkDangNoiThietBi(2), 'Đang nối: 2 thiết bị');
    expect(e.linkDangNoiThietBi(1), 'Connected: 1 peer');
    expect(e.linkDangNoiThietBi(3), 'Connected: 3 peers');
  });

  for (final locale in L10n.supportedLocales) {
    testWidgets('[${locale.languageCode}] autosave lỗi: banner hiện câu đã dịch theo mã, không hiện message hệ thống', (
      tester,
    ) async {
      final h = SessionHarness(tester);
      await h.pump(locale: locale);
      h.repo.failSaveWith = const FileSystemException('write failed', '/p/project.json.tmp', OSError('No space', 28));
      h.c.read(projectControllerProvider.notifier).setBpm(99);
      await tester.pump(const Duration(seconds: 3));
      await tester.pump();
      final l = lookupAppLocalizations(locale);
      expect(find.text(l.sessionKhongLuuDuocProject(l.errorText('DISK_FULL'))), findsOneWidget);
      expect(find.textContaining('No space'), findsNothing);
      expect(find.textContaining('write failed'), findsNothing);
      h.repo.failSaveWith = null;
      await h.unmount();
    });
  }

  test('tên bản nhân bản theo ngôn ngữ', () {
    expect(lookupAppLocalizations(const Locale('en')).projectsBanSao('Jam'), 'Jam (copy)');
    expect(lookupAppLocalizations(const Locale('vi')).projectsBanSao('Jam'), 'Jam (bản sao)');
  });

  test('iOS: CFBundleLocalizations = [en, vi]; câu xin quyền micro có bản en và vi', () {
    final plist = File('ios/Runner/Info.plist').readAsStringSync();
    expect(plist, contains('<key>CFBundleLocalizations</key>'));
    expect(RegExp(r'<string>en</string>\s*<string>vi</string>').hasMatch(plist), isTrue);
    for (final l in ['en', 'vi']) {
      final strings = File('ios/Runner/$l.lproj/InfoPlist.strings').readAsStringSync();
      expect(strings, contains('"NSMicrophoneUsageDescription"'), reason: l);
    }
    final pbx = File('ios/Runner.xcodeproj/project.pbxproj').readAsStringSync();
    expect(pbx, contains('vi.lproj/InfoPlist.strings'));
  });
}
