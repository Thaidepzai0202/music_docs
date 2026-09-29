import 'dart:ui' show Rect;

import 'package:share_plus/share_plus.dart';

/// Share sheet iOS. Tách interface để test thay bằng bản ghi lại.
abstract interface class ShareService {
  /// [origin]: vùng neo popover (iPad bắt buộc có, nếu không share sheet không hiện).
  Future<void> shareFiles(List<String> paths, {Rect? origin});
}

class SharePlusService implements ShareService {
  const SharePlusService();

  @override
  Future<void> shareFiles(List<String> paths, {Rect? origin}) async {
    await SharePlus.instance.share(ShareParams(files: [for (final p in paths) XFile(p)], sharePositionOrigin: origin));
  }
}
