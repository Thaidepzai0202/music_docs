import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../data/library_repository.dart';

/// Tên pad của kit SFZ (07 §4.1b: piano roll track kit — mỗi hàng là một pad, chỉ các phím có sample; 06 §4: tên
/// hiện trên pad). Tên lấy từ `region_label` nếu có (được có dấu cách: "Floor Tom L"), không thì tên file sample
/// ("hat_open.wav" → "Hat open").
abstract final class SfzPads {
  /// key MIDI → tên, theo thứ tự xuất hiện trong file.
  static Map<int, String> parse(String sfz) {
    final out = <int, String>{};
    for (final region in sfz.split('<region>').skip(1)) {
      final body = region.split(RegExp('<(?:group|global|control|master|curve|effect)>')).first;
      final opcodes = <String, String>{};
      for (final line in body.split('\n')) {
        final code = line.split('//').first;
        // Giá trị kéo tới trước opcode kế tiếp ("… region_label=Open Hat group=2") hoặc hết dòng.
        for (final m in RegExp(r'(\w+)=(.*?)(?=\s+\w+=|\s*$)').allMatches(code)) {
          opcodes[m[1]!] = m[2]!.trim();
        }
      }
      final key =
          int.tryParse(opcodes['key'] ?? '') ??
          (opcodes['lokey'] != null && opcodes['lokey'] == opcodes['hikey'] ? int.tryParse(opcodes['lokey']!) : null);
      if (key == null || key < 0 || key > 127) continue;
      out.putIfAbsent(key, () => opcodes['region_label'] ?? _pretty(opcodes['sample'] ?? '$key'));
    }
    return out;
  }

  static String _pretty(String sample) {
    final base = sample.split('/').last.split('.').first.replaceAll(RegExp('[_-]+'), ' ').trim();
    return base.isEmpty ? sample : base[0].toUpperCase() + base.substring(1);
  }
}

/// Kit SFZ trong thư viện (đường dẫn tương đối như `InstrumentRef.sfz.path`) → tên pad. Lỗi đọc → rỗng.
final padNamesProvider = FutureProvider.family<Map<int, String>, String>((ref, sfzPath) async {
  try {
    return SfzPads.parse(await rootBundle.loadString('${LibraryRepository.root}/$sfzPath'));
  } on Object {
    return const {};
  }
});
