import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app/app.dart';
import 'engine/engine_bootstrap.dart';
import 'engine/engine_providers.dart';
import 'features/settings/app_settings.dart';
import 'services/engine_settings.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations(const [
    DeviceOrientation.landscapeLeft,
    DeviceOrientation.landscapeRight,
  ]);
  final boot = await bootstrapEngine();
  final settings = await SettingsRepository(File('${boot.dataDir}/settings.json')).load();
  // Cài đặt nằm ở engine (quantize thu MIDI, buffer, bù độ trễ) → áp dụng lại giá trị đã lưu.
  for (final e in applyEngineSettings(boot.engine, settings)) {
    debugPrint('settings: $e');
  }
  runApp(
    ProviderScope(
      overrides: [engineBootstrapProvider.overrideWithValue(boot), initialSettingsProvider.overrideWithValue(settings)],
      child: const LoopCoreApp(),
    ),
  );
}
