import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../model/project.dart';
import '../../../services/service_providers.dart';
import '../app_settings.dart';
import '../settings_screen.dart';
import '../../../l10n/l10n.dart';

/// Settings → Audio (P4-13): buffer, micro (quyền / chỉ phát), mặc định khi thu, rung.
class AudioSection extends ConsumerStatefulWidget {
  const AudioSection({super.key});

  @override
  ConsumerState<AudioSection> createState() => _AudioSectionState();
}

class _AudioSectionState extends ConsumerState<AudioSection> {
  Map<String, dynamic> _info = const {};

  @override
  void initState() {
    super.initState();
    _readInfo();
  }

  void _readInfo() {
    _info = ref.read(audioDeviceServiceProvider).info();
  }

  void _setBuffer(int frames) {
    final err = ref.read(settingsProvider.notifier).setBufferSize(frames);
    setState(_readInfo);
    if (err != null) {
      ScaffoldMessenger.maybeOf(
        context,
      )?.showSnackBar(SnackBar(content: Text(S.audioKhongDoiDuocBuffer(S.errorText(err)))));
    }
  }

  @override
  Widget build(BuildContext context) {
    final s = ref.watch(settingsProvider);
    final ctl = ref.read(settingsProvider.notifier);
    final sr = (_info['sampleRate'] as num?)?.toDouble() ?? 48000;
    final actual = (_info['bufferSize'] as num?)?.toInt();
    String ms(int frames) => '${(frames / sr * 1000).toStringAsFixed(1)} ms';

    return SettingsPage(
      children: [
        SettingsGroup(
          title: S.aboutThietBi,
          children: [
            SettingsRow(
              label: S.audioBuffer,
              subtitle: S.audioNhoHonTreItHon(actual == null ? '' : S.audioDangChay(actual, ms(actual), sr.round())),
              child: SegmentedButton<int>(
                key: const Key('settings.buffer'),
                showSelectedIcon: false,
                segments: [for (final b in bufferSizeChoices) ButtonSegment(value: b, label: Text('$b'))],
                selected: {s.bufferSize},
                onSelectionChanged: (v) => _setBuffer(v.first),
              ),
            ),
            const Divider(height: 1),
            const _MicRow(),
          ],
        ),
        SettingsGroup(
          title: S.audioThuAm,
          children: [
            SettingsRow(
              label: S.audioSoBarKhiThuVao,
              subtitle: s.recordBars == 0 ? S.audioTuDoGhiChu : null,
              child: SegmentedButton<int>(
                key: const Key('settings.recordBars'),
                showSelectedIcon: false,
                segments: [
                  for (final b in const [1, 2, 4, 8]) ButtonSegment(value: b, label: Text(S.audioBars(b))),
                  ButtonSegment(value: 0, label: Text(S.audioTuDo, key: const Key('settings.recordBars.free'))),
                ],
                selected: {s.recordBars},
                onSelectionChanged: (v) => ctl.setRecordBars(v.first),
              ),
            ),
            const Divider(height: 1),
            SettingsRow(
              label: S.audioMonitorMacDinhChoTrack,
              subtitle: S.audioTuDongNgheMicroKhi,
              below: true,
              child: SegmentedButton<MonitorMode>(
                key: const Key('settings.defaultMonitor'),
                showSelectedIcon: false,
                segments: [
                  ButtonSegment(value: MonitorMode.off, label: Text(S.fxTat)),
                  ButtonSegment(value: MonitorMode.auto, label: Text(S.audioTuDong)),
                  ButtonSegment(value: MonitorMode.always, label: Text(S.audioLuonBat)),
                ],
                selected: {s.defaultMonitor},
                onSelectionChanged: (v) => ctl.setDefaultMonitor(v.first),
              ),
            ),
            const Divider(height: 1),
            SettingsRow(
              label: S.audioQuantizeNotKhiThuMidi,
              child: SegmentedButton<RecordQuantize>(
                key: const Key('settings.midiQuantize'),
                showSelectedIcon: false,
                segments: [
                  for (final q in RecordQuantize.values) ButtonSegment(value: q, label: Text(S.recordQuantize(q.name))),
                ],
                selected: {s.midiRecordQuantize},
                onSelectionChanged: (v) => ctl.setMidiRecordQuantize(v.first),
              ),
            ),
          ],
        ),
        SettingsGroup(
          title: S.audioChoi,
          children: [
            SettingsRow(
              label: S.audioRungNheKhiLaunchThu,
              child: Switch(key: const Key('settings.haptics'), value: s.haptics, onChanged: ctl.setHaptics),
            ),
          ],
        ),
      ],
    );
  }
}

/// Trạng thái micro (07 §4.0): có quyền / chỉ phát (+ Mở Cài đặt) / audio chưa bật.
class _MicRow extends ConsumerWidget {
  const _MicRow();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final audio = ref.watch(engineAudioProvider);
    return ListenableBuilder(
      listenable: audio,
      builder: (context, _) {
        final (text, color) = !audio.isRunning
            ? (S.audioAudioChuaBat, AppColors.textSecondary)
            : audio.outputOnly
            ? (S.sessionChuaCoQuyenMicroChi, AppColors.queued)
            : (S.audioDangBatThuDuoc, AppColors.play);
        return SettingsRow(
          label: S.audioMic,
          subtitle: text,
          child: audio.outputOnly
              ? OutlinedButton(
                  key: const Key('settings.mic.openSettings'),
                  onPressed: audio.openAppSettings,
                  child: Text(S.onboardingMoCaiDat),
                )
              : Icon(Icons.mic, key: const Key('settings.mic.status'), color: color),
        );
      },
    );
  }
}
