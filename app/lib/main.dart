import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app/app.dart';
import 'engine/engine_bootstrap.dart';
import 'engine/engine_providers.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations(const [
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  final boot = await bootstrapEngine();
  runApp(ProviderScope(overrides: [engineBootstrapProvider.overrideWithValue(boot)], child: const LoopCoreApp()));
}
