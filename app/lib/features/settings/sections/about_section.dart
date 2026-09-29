import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/app_info.dart';
import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../services/service_providers.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Settings → Giới thiệu: phiên bản app + thông tin engine (`engine.info`).
class AboutSection extends ConsumerWidget {
  const AboutSection({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final boot = ref.watch(engineBootstrapProvider);
    final device = ref.watch(audioDeviceServiceProvider);
    final info = device.info();
    return SettingsPage(
      children: [
        Padding(
          padding: const EdgeInsets.only(bottom: 20),
          child: Row(
            children: [
              const Icon(Icons.grid_view_rounded, size: 48, color: AppColors.play),
              const SizedBox(width: 16),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(AppInfo.name, style: TextStyle(fontSize: 22, fontWeight: FontWeight.w600)),
                    Text(
                      S.aboutPhienBan(AppInfo.version, AppInfo.build),
                      key: const Key('about.version'),
                      style: const TextStyle(color: AppColors.textSecondary),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        SettingsGroup(
          title: S.aboutEngine,
          children: [
            SettingsRow(label: S.aboutLoai, child: Text(boot.isFake ? S.aboutEngineGiaLoopcoreFake : S.aboutLoopCore)),
            SettingsRow(
              label: S.aboutApi,
              child: Text('v${device.apiVersion}', key: const Key('about.api')),
            ),
            if (info['sampleRate'] != null)
              SettingsRow(
                label: S.aboutSampleRate,
                child: Text('${info['sampleRate']} Hz', style: AppText.numeric),
              ),
            if (info['bufferSize'] != null)
              SettingsRow(
                label: S.aboutBuffer,
                child: Text(S.aboutFrames(info['bufferSize'] as Object), style: AppText.numeric),
              ),
            if (info['device'] != null) SettingsRow(label: S.aboutThietBi, child: Text('${info['device']}')),
          ],
        ),
        Text(S.aboutDungBangFlutterEngineC, style: TextStyle(fontSize: 12, color: AppColors.textSecondary)),
      ],
    );
  }
}
