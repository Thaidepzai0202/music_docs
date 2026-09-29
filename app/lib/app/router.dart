import 'package:flutter/material.dart';

import '../features/onboarding/onboarding_screen.dart';
import '../features/projects/projects_screen.dart';
import '../features/session/session_screen.dart';
import '../features/settings/settings_screen.dart';
import '../features/spike/spike_screen.dart';

/// Route của app (07 §1). Tới hết M0 (16/10) app vẫn mở màn spike; Projects/Session vào bằng nút
/// "Projects (dev)" trên màn spike hoặc dart-define (xem [AppRoutes.initial]).
/// Lần đầu vào Projects (chưa có cờ `onboardingDone`) thì đi qua Onboarding trước ([projectsEntry]).
abstract final class AppRoutes {
  static const spike = '/spike';
  static const onboarding = '/onboarding';
  static const projects = '/projects';
  static const session = '/session';
  static const settings = '/settings';

  /// Màn mở đầu. Mặc định spike tới hết M0; chạy `--dart-define=LOOPCORE_START=projects` để vào thẳng P2.
  static String get initial => switch (const String.fromEnvironment('LOOPCORE_START')) {
    'projects' => projects,
    _ => spike,
  };

  /// Lối vào Projects: onboarding nếu chưa xem (P4-14).
  static String projectsEntry({required bool onboardingDone}) => onboardingDone ? projects : onboarding;

  /// Route mở đầu có tính cờ onboarding.
  static String initialFor({required bool onboardingDone}) =>
      initial == projects ? projectsEntry(onboardingDone: onboardingDone) : initial;

  static Route<void>? onGenerateRoute(RouteSettings settings) {
    final Widget? page = switch (settings.name) {
      spike => const SpikeScreen(),
      onboarding => const OnboardingScreen(),
      projects => const ProjectsScreen(),
      session => const SessionScreen(),
      AppRoutes.settings => const SettingsScreen(), // (tham số `settings` che tên hằng)
      _ => null,
    };
    if (page == null) return null;
    return MaterialPageRoute<void>(settings: settings, builder: (_) => page);
  }
}
