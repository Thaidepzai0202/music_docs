import 'package:flutter/material.dart';

/// Design tokens tối thiểu (07 §8). Chỉ dark theme.
abstract final class AppColors {
  static const background = Color(0xFF0E0F12);
  static const surface = Color(0xFF17191E);
  static const border = Color(0xFF262A31);
  static const textPrimary = Color(0xFFE8EAED);
  static const textSecondary = Color(0xFF9AA0A6);

  static const record = Color(0xFFFF3B30);
  static const play = Color(0xFF34C759);
  static const queued = Color(0xFFFFD60A);

  /// 8 màu track, tương phản tốt trên nền tối.
  static const tracks = <Color>[
    Color(0xFFFF7A59),
    Color(0xFFFFB547),
    Color(0xFFF5E663),
    Color(0xFF7BD88F),
    Color(0xFF59C3FF),
    Color(0xFF7A8CFF),
    Color(0xFFB98CFF),
    Color(0xFFFF7AC6),
  ];
}

abstract final class AppText {
  /// Số (BPM, dB, CPU) không nhảy bề rộng khi đổi giá trị.
  static const tabular = [FontFeature.tabularFigures()];

  static const numeric = TextStyle(color: AppColors.textPrimary, fontSize: 14, fontFeatures: tabular);
}

/// Font của theme cho chữ vẽ bằng TextPainter: CustomPainter không kế thừa DefaultTextStyle, thiếu font thì
/// flutter_test vẽ ô vuông (golden không kiểm được). Trên iPad đây vẫn là font hệ thống của Typography.
TextStyle painterFont(BuildContext context) {
  final s = Theme.of(context).textTheme.bodyMedium!;
  return TextStyle(fontFamily: s.fontFamily, fontFamilyFallback: s.fontFamilyFallback);
}

/// Font hệ thống (SF Pro trên iPad) — không khai báo fontFamily.
ThemeData buildAppTheme() {
  const scheme = ColorScheme.dark(
    surface: AppColors.surface,
    primary: AppColors.play,
    secondary: AppColors.queued,
    error: AppColors.record,
    onSurface: AppColors.textPrimary,
    outline: AppColors.border,
  );
  return ThemeData(
    colorScheme: scheme,
    scaffoldBackgroundColor: AppColors.background,
    canvasColor: AppColors.background,
    dividerColor: AppColors.border,
    // Không có hiệu ứng splash/ripple: tốn GPU và không hợp app biểu diễn.
    splashFactory: NoSplash.splashFactory,
    textTheme: const TextTheme(
      bodyMedium: TextStyle(color: AppColors.textPrimary),
      bodySmall: TextStyle(color: AppColors.textSecondary),
    ),
    appBarTheme: const AppBarTheme(
      backgroundColor: AppColors.surface,
      foregroundColor: AppColors.textPrimary,
      elevation: 0,
    ),
  );
}
