import 'package:flutter/widgets.dart';

import 'app_localizations.dart';

export 'app_localizations.dart';

/// Ngôn ngữ của app (07 §8): tiếng Việt nếu iOS chọn `vi`, mọi ngôn ngữ khác → tiếng Anh. Không có màn chọn ngôn
/// ngữ trong app — người dùng đổi riêng cho app ở Cài đặt iOS (Info.plist `CFBundleLocalizations = [en, vi]`).
abstract final class L10n {
  static const supportedLocales = [Locale('en'), Locale('vi')];

  /// Bản dịch đang dùng. [LocaleScope] đặt lại mỗi khi MaterialApp dựng (theo ngôn ngữ iOS). Mặc định `vi` cho
  /// code chạy ngoài cây widget (test thuần, log).
  static AppLocalizations current = lookupAppLocalizations(const Locale('vi'));

  /// `localeListResolutionCallback`: chỉ xét ngôn ngữ ưu tiên đầu tiên của iOS.
  static Locale resolve(List<Locale>? preferred, Iterable<Locale> supported) =>
      preferred != null && preferred.isNotEmpty && preferred.first.languageCode == 'vi'
      ? const Locale('vi')
      : const Locale('en');

  static const localizationsDelegates = AppLocalizations.localizationsDelegates;
}

/// Chuỗi hiển thị: `S.key` / `S.key(args)` (key = slug cũ của strings.dart). Chỉ dùng trong UI; code không có
/// BuildContext (controller, error bus) giữ MÃ lỗi và để UI dịch lúc hiển thị.
AppLocalizations get S => L10n.current;

/// Đặt [L10n.current] theo `Localizations` của cây (dùng trong `MaterialApp.builder`).
class LocaleScope extends StatelessWidget {
  const LocaleScope({super.key, required this.child});

  final Widget child;

  @override
  Widget build(BuildContext context) {
    L10n.current = AppLocalizations.of(context);
    return child;
  }
}
