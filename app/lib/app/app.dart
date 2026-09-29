import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../features/settings/app_settings.dart';
import '../l10n/l10n.dart';

import 'engine_lifecycle_scope.dart';
import 'router.dart';
import 'theme.dart';

class LoopCoreApp extends ConsumerWidget {
  const LoopCoreApp({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    // Chỉ đọc 1 lần (initialRoute không đổi sau khi app đã dựng).
    final onboardingDone = ref.read(initialSettingsProvider).onboardingDone;
    return MaterialApp(
      title: 'Music Looper',
      debugShowCheckedModeBanner: false,
      theme: buildAppTheme(),
      // 07 §8: en + vi theo ngôn ngữ iOS (vi → tiếng Việt, còn lại → tiếng Anh). Không có màn chọn ngôn ngữ.
      localizationsDelegates: L10n.localizationsDelegates,
      supportedLocales: L10n.supportedLocales,
      localeListResolutionCallback: L10n.resolve,
      initialRoute: AppRoutes.initialFor(onboardingDone: onboardingDone),
      onGenerateRoute: AppRoutes.onGenerateRoute,
      // Vòng đời engine + autosave cho mọi màn. Màn spike giữ audio khi xuống nền (keepRunningInBackground).
      builder: (context, child) => LocaleScope(child: EngineLifecycleScope(child: child!)),
    );
  }
}
