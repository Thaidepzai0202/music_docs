// Checklist 07 §6 dạng test tĩnh: quét lib/ để luật hiệu năng không bị phá âm thầm.
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

List<File> dartFiles(String dir) => Directory(dir)
    .listSync(recursive: true)
    .whereType<File>()
    .where((f) => f.path.endsWith('.dart') && !f.path.endsWith('.g.dart') && !f.path.endsWith('.freezed.dart'))
    .toList();

/// Thân của mọi hàm `void paint(Canvas …, Size …) { … }` trong [src].
Iterable<String> paintBodies(String src) sync* {
  final re = RegExp(r'void paint\(Canvas \w+, Size \w+\) \{');
  for (final m in re.allMatches(src)) {
    var depth = 1, i = m.end;
    while (i < src.length && depth > 0) {
      if (src[i] == '{') depth++;
      if (src[i] == '}') depth--;
      i++;
    }
    yield src.substring(m.end, i - 1);
  }
}

void main() {
  final files = dartFiles('lib');

  test('07 §6.4: không BackdropFilter, Opacity động, saveLayer, ShaderMask', () {
    final bad = <String>[];
    for (final f in files) {
      final src = f.readAsStringSync();
      for (final token in ['BackdropFilter', 'AnimatedOpacity', 'Opacity(', 'saveLayer(', 'ShaderMask(']) {
        if (src.contains(token)) bad.add('${f.path}: $token');
      }
    }
    expect(bad, isEmpty);
  });

  test('07 §6.4: không ClipRRect / clipRRect trong grid Session và ui_kit', () {
    final bad = [
      for (final f in files)
        if ((f.path.contains('features/session') || f.path.contains('ui_kit')) &&
            (f.readAsStringSync().contains('ClipRRect') || f.readAsStringSync().contains('clipRRect(')))
          f.path,
    ];
    expect(bad, isEmpty);
  });

  test('07 §6.5: không tạo Paint / Path mới bên trong paint()', () {
    final bad = <String>[];
    for (final f in files) {
      for (final body in paintBodies(f.readAsStringSync())) {
        for (final token in ['Paint()', 'Path()']) {
          if (body.contains(token)) bad.add('${f.path}: $token trong paint()');
        }
      }
    }
    expect(bad, isEmpty);
  });

  test('07 §6.1: dữ liệu 60Hz không rebuild widget (không ListenableBuilder/AnimatedBuilder theo ticker)', () {
    final re = RegExp(r'(ListenableBuilder|AnimatedBuilder)\([^)]*(listenable|animation): *(ticker|_ticker)\b');
    final bad = [
      for (final f in files)
        if (re.hasMatch(f.readAsStringSync())) f.path,
    ];
    expect(bad, isEmpty);
  });

  test(
    'mọi CustomPainter vẽ theo Ticker nằm trong RepaintBoundary (file có repaint: ticker phải có RepaintBoundary)',
    () {
      final bad = [
        for (final f in files)
          if (RegExp(r'super\(repaint: *(ticker|_ticker|repaint)\)').hasMatch(f.readAsStringSync()) &&
              !f.readAsStringSync().contains('RepaintBoundary'))
            f.path,
      ];
      expect(bad, isEmpty, reason: 'painter 60Hz phải có boundary riêng');
    },
  );

  test(
    '07 §5: widget không gọi engine trực tiếp — qua ProjectController, service (lib/services) hoặc PerformanceActions',
    () {
      // Ngoại lệ: ProjectController (mọi thay đổi project) và màn spike (công cụ đo M0, bỏ sau M0).
      bool exempt(String p) => p.contains('features/spike/') || p.endsWith('features/session/project_controller.dart');
      final tokens = [
        RegExp(r'\bengineProvider\b'),
        RegExp(r'\bEngineApi\b'),
        RegExp(r'\bEngineClient\b'),
        RegExp(r'\.callOk\('),
        RegExp(r'\.callJob\('),
        RegExp(r'\.getPeaks\('),
        RegExp(r"\.call\(\{'op'"),
      ];
      final bad = <String>[];
      for (final f in files) {
        final p = f.path.replaceAll(r'\', '/');
        if (!(p.contains('lib/features/') || p.contains('lib/ui_kit/')) || exempt(p)) continue;
        final src = f.readAsStringSync();
        for (final t in tokens) {
          if (t.hasMatch(src)) bad.add('$p: ${t.pattern}');
        }
      }
      expect(bad, isEmpty);
    },
  );
  test('07 §8 (l10n): không chuỗi hiển thị viết cứng trong lib/features, lib/ui_kit — dùng S.* (ARB en + vi)', () {
    final bad = <String>[];
    for (final f in files) {
      final p = f.path.replaceAll(r'\', '/');
      if (!(p.contains('lib/features/') || p.contains('lib/ui_kit/') || p.endsWith('engine_error_bus.dart'))) continue;
      bad.addAll(hardcodedDisplayStrings(f.readAsStringSync()).map((t) => '$p: "$t"'));
    }
    expect(bad, isEmpty);
  });

  test(
    'luật l10n tự kiểm: bắt tiếng Việt mọi chỗ + tiếng Anh ở vị trí hiển thị; tha key kỹ thuật, viết tắt, đơn vị',
    () {
      const src = r'''
      Text('Hello world');
      Tooltip(message: 'Delete clip');
      x = cond ? 'a' : '${n ? 'Xoá' : ''}';
      callOk('track.configure', {'kind': 'audio'});
      Text(running ? 'Stop audio' : 'Start audio');
      Text('$bpm BPM'); Text('${sr} Hz'); Text(S.foo); Text(m['name'] ?? '?');
      // Text('Comment'), bỏ qua
    ''';
      expect(hardcodedDisplayStrings(src), ['Hello world', 'Delete clip', 'Xoá', 'Stop audio', 'Start audio']);
    },
  );
}

final _viet = RegExp(
  '[àáảãạăằắẳẵặâầấẩẫậèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵđĐ'
  'ÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬÈÉẺẼẸÊỀẾỂỄỆÌÍỈĨỊÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢÙÚỦŨỤƯỪỨỬỮỰỲÝỶỸỴ]',
);
// Vị trí hiển thị: literal là đối số chữ của Text hoặc tham số tên label / tooltip / title / message / text…
// Kể cả nhánh của biểu thức `? :` ngay trong đối số đó: `Text(on ? 'Stop' : 'Start')`.
final _uiPos = RegExp(
  r'(?:\bText\(\s*|\b(?:label|tooltip|title|subtitle|message|text|content|semanticsLabel|hintText|labelText)\s*:\s*)'
  r'''(?:[^;(){}\n]*\s[?:]\s*)?$''',
);
final _abbreviation = RegExp(r'^(?:[A-Z0-9 /.:·%+×()…\-–—]{1,8}|(?:k?Hz|ms|dB|st|ct))$'); // WAV, BPM, LP/HP, Hz, dB

/// Chuỗi hiển thị viết cứng trong [src]: literal có chữ tiếng Việt (mọi chỗ, kể cả lồng trong `${…}`), hoặc literal
/// tiếng Anh ở vị trí hiển thị. Key kỹ thuật (op, tên trường JSON) không ở vị trí hiển thị nên không bị bắt.
List<String> hardcodedDisplayStrings(String src) => [
  for (final (start, text) in stringLiterals(src))
    if (_viet.hasMatch(text) ||
        (_uiPos.hasMatch(src.substring(start < 80 ? 0 : start - 80, start)) &&
            RegExp('[A-Za-z]{2,}').hasMatch(text) &&
            !_abbreviation.hasMatch(text.trim())))
      text,
];

/// Mọi literal chuỗi ở [src] — kể cả literal lồng trong `${…}` — dạng (vị trí mở nháy, phần chữ không gồm biểu
/// thức nội suy). Bỏ qua comment.
List<(int, String)> stringLiterals(String src) {
  final out = <(int, String)>[];
  (int, String) scan(int start) {
    final q = src[start];
    final triple = src.startsWith(q * 3, start);
    final raw = start > 0 && src[start - 1] == 'r';
    var j = start + (triple ? 3 : 1);
    final buf = StringBuffer();
    while (j < src.length) {
      if (triple ? src.startsWith(q * 3, j) : src[j] == q) return (j + (triple ? 3 : 1), buf.toString());
      final c = src[j];
      if (c == r'\' && !raw) {
        j += 2;
        continue;
      }
      if (c == r'$' && !raw && j + 1 < src.length && RegExp(r'[A-Za-z_]').hasMatch(src[j + 1])) {
        j++; // $tên — biểu thức, không phải chữ
        while (j < src.length && RegExp(r'\w').hasMatch(src[j])) {
          j++;
        }
        continue;
      }
      if (c == r'$' && !raw && j + 1 < src.length && src[j + 1] == '{') {
        var depth = 1;
        j += 2;
        while (depth > 0) {
          if (src[j] == "'" || src[j] == '"') {
            final (end, text) = scan(j);
            out.add((j, text)); // literal lồng trong ${…}
            j = end;
            continue;
          }
          if (src[j] == '{') depth++;
          if (src[j] == '}') depth--;
          j++;
        }
        continue;
      }
      buf.write(c);
      j++;
    }
    return (j, buf.toString());
  }

  var i = 0;
  while (i < src.length) {
    if (src.startsWith('//', i)) {
      final nl = src.indexOf('\n', i);
      i = nl < 0 ? src.length : nl;
      continue;
    }
    if (src.startsWith('/*', i)) {
      i = src.indexOf('*/', i) + 2;
      continue;
    }
    if (src[i] == "'" || src[i] == '"') {
      final (end, text) = scan(i);
      out.add((i, text));
      i = end;
      continue;
    }
    i++;
  }
  return out;
}
