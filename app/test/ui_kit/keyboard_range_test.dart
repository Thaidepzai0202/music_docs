// Bàn phím đánh dấu dải phím tự nhiên của nhạc cụ (manifest `range`): phím trong dải có vạch accent mảnh ở đầu phím,
// phím ngoài dải nhạt hơn (vẫn bấm được, vẫn kêu); không có range → không đánh dấu.
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/data/library_repository.dart';
import 'package:music_looper/ui_kit/keyboard_view.dart';

import '../session_harness.dart';

Future<ByteData> _render(KeyboardPainter p, Size size) async {
  final rec = ui.PictureRecorder();
  p.paint(Canvas(rec), size);
  final img = await rec.endRecording().toImage(size.width.toInt(), size.height.toInt());
  return (await img.toByteData())!;
}

/// (r, g, b) 0..255 của pixel [o].
(int, int, int) _pixel(ByteData d, int width, Offset o) {
  final i = (o.dy.toInt() * width + o.dx.toInt()) * 4;
  return (d.getUint8(i), d.getUint8(i + 1), d.getUint8(i + 2));
}

int _luma((int, int, int) c) => c.$1 + c.$2 + c.$3;

void main() {
  test('KeyboardPainter: phím ngoài range nhạt hơn, trong range có vạch accent; không range → không đánh dấu', () async {
    const size = Size(700, 100);
    const layout = KeyboardLayout(baseNote: 24); // C1..B2: 14 phím trắng, mỗi phím 50 px
    const accent = Color(0xFF00FF00);
    final held = ValueNotifier<Set<int>>(const {});
    addTearDown(held.dispose);
    final ranged = await _render(KeyboardPainter(layout: layout, held: held, accent: accent, range: (36, 84)), size);
    final plain = await _render(KeyboardPainter(layout: layout, held: held, accent: accent), size);
    // Phím trắng thứ 0 = C1 (24, ngoài dải), thứ 7 = C2 (36, trong dải). Thân phím đo ở phần dưới (không có phím đen).
    const outBody = Offset(25, 80), inBody = Offset(375, 80), outTop = Offset(25, 2), inTop = Offset(375, 2);
    expect(
      _luma(_pixel(ranged, 700, outBody)),
      lessThan(_luma(_pixel(ranged, 700, inBody))),
      reason: 'ngoài dải nhạt hơn',
    );
    final mark = _pixel(ranged, 700, inTop);
    expect(mark.$2 > 200 && mark.$1 < 100 && mark.$3 < 100, isTrue, reason: 'vạch accent ở đầu phím trong dải: $mark');
    final noMark = _pixel(ranged, 700, outTop);
    expect(noMark.$1 > 100 && noMark.$3 > 100, isTrue, reason: 'phím ngoài dải không có vạch: $noMark');
    expect(_pixel(plain, 700, outBody), _pixel(ranged, 700, inBody), reason: 'không range → mọi phím như trong dải');
    final plainTop = _pixel(plain, 700, inTop);
    expect(plainTop.$1 > 100 && plainTop.$3 > 100, isTrue, reason: 'không range → không vạch: $plainTop');
  });

  test('manifest: range [thấp, cao] → (thấp, cao); thiếu / sai → null', () {
    final m = testLibrary();
    expect(m.instrumentAt('instruments/inst_epiano/inst_epiano.sfz')!.range, (28, 100));
    expect(m.instrumentAt('kits/kit_808/kit_808.sfz')!.range, isNull);
    final bad = LibraryItem.fromJson(
      {
        'id': 'x',
        'name': 'X',
        'path': 'x.sfz',
        'range': [90, 10],
      },
      pathKey: 'path',
      kind: LibraryKind.instrument,
    );
    expect(bad.range, isNull);
  });
}
