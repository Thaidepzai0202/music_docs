import 'package:flutter/material.dart';

import '../features/projects/projects_screen.dart';
import '../features/session/session_screen.dart';
import '../features/spike/spike_screen.dart';

/// Route của app (07 §1). Tới hết M0 (16/10) `LoopCoreApp` vẫn mở thẳng màn spike;
/// router này dựng sẵn cho P2 và sẽ gắn vào `MaterialApp.onGenerateRoute` sau M0.
abstract final class AppRoutes {
  static const spike = '/spike';
  static const projects = '/projects';
  static const session = '/session';

  static Route<void>? onGenerateRoute(RouteSettings settings) {
    final Widget? page = switch (settings.name) {
      spike => const SpikeScreen(),
      projects => const ProjectsScreen(),
      session => const SessionScreen(),
      _ => null,
    };
    if (page == null) return null;
    return MaterialPageRoute<void>(settings: settings, builder: (_) => page);
  }
}
