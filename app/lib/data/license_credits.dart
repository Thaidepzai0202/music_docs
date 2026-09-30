/// Dòng ghi công trong file giấy phép nội dung: đoạn trích dẫn (`> …`) ngay dưới tiêu đề "## Ghi công" (content/LICENSES
/// do agent 80 viết, P2-35) → một đoạn văn thường: bỏ `**`, `[chữ](url)` → "chữ (url)", `<url>` → url. null nếu không có.
/// Lấy nguyên văn từ file để câu ghi công chỉ nằm một chỗ (CC BY 3.0 của Salamander bắt buộc ghi công).
String? licenseCredit(String markdown) {
  final lines = markdown.split('\n');
  final start = lines.indexWhere((l) => RegExp(r'^##\s+Ghi công').hasMatch(l.trim()));
  if (start < 0) return null;
  final quote = <String>[];
  for (final l in lines.skip(start + 1)) {
    final t = l.trim();
    if (t.startsWith('>')) {
      quote.add(t.substring(1).trim());
    } else if (quote.isNotEmpty || t.startsWith('#')) {
      break;
    }
  }
  if (quote.isEmpty) return null;
  return quote
      .join(' ')
      .replaceAll('**', '')
      .replaceAllMapped(RegExp(r'\[([^\]]+)\]\(([^)]+)\)'), (m) => '${m[1]} (${m[2]})')
      .replaceAllMapped(RegExp(r'<(https?://[^>]+)>'), (m) => m[1]!)
      .replaceAll(RegExp(r'\s+'), ' ')
      .trim();
}
