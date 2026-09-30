import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../engine/engine_providers.dart';
import '../../model/project.dart';
import '../../ui_kit/fader.dart';
import '../../ui_kit/knob.dart';
import '../../ui_kit/meter.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../../l10n/l10n.dart';

/// Panel Mixer (P2-14): 8 strip track + strip Master.
/// Fader/knob gom lệnh theo frame, chạm đúp về mặc định, kéo nhiều ngón độc lập (ui_kit, P2-15).
/// Meter vẽ theo Ticker trong boundary riêng — không rebuild.
class MixerPanel extends StatelessWidget {
  const MixerPanel({super.key});

  /// Đệm trên/dưới và cỡ nhãn dB dùng chung cho strip track và Master → nhãn cùng baseline.
  static const verticalInset = 8.0;
  static const labelSize = 11.0;

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        for (var t = 0; t < SessionLayout.tracks; t++) Expanded(child: MixerStrip(track: t)),
        const SizedBox(width: SessionLayout.sceneColumnWidth, child: MasterStrip()),
      ],
    );
  }
}

class MixerStrip extends ConsumerWidget {
  const MixerStrip({super.key, required this.track});

  final int track;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final tr = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(track)));
    final ticker = ref.watch(engineStateTickerProvider);
    final ctl = ref.read(projectControllerProvider.notifier);
    final mixer = tr?.mixer;
    final gainDb = mixer?.gainDb ?? 0;
    final color = tr == null ? AppColors.tracks[track % 8] : parseHexColor(tr.color);
    return Container(
      margin: const EdgeInsets.symmetric(horizontal: 2, vertical: MixerPanel.verticalInset / 2),
      decoration: BoxDecoration(color: AppColors.background, borderRadius: BorderRadius.circular(6)),
      padding: const EdgeInsets.fromLTRB(4, MixerPanel.verticalInset / 2, 4, MixerPanel.verticalInset / 2),
      child: Column(
        children: [
          Row(
            children: [
              Container(width: 4, height: 14, color: color),
              const SizedBox(width: 4),
              Expanded(
                child: Text(
                  tr?.name ?? S.sessionTrackName(track + 1),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontSize: 11),
                ),
              ),
              // Monitor input chỉ có nghĩa với track audio (track instrument thu MIDI). Badge giữ đủ bề rộng (chuỗi
              // ngắn), tên track co lại trước.
              if (tr?.kind != TrackKind.instrument)
                _MonitorChip(
                  key: Key('mixer.monitor.$track'),
                  mode: tr?.monitor ?? MonitorMode.off,
                  onSelected: (m) => ctl.setMonitor(track, m),
                ),
            ],
          ),
          const SizedBox(height: 2),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceEvenly,
            children: [
              Knob(
                key: Key('mixer.pan.$track'),
                value: ((mixer?.pan ?? 0) + 1) / 2,
                bipolar: true,
                color: color,
                size: 32,
                onChanged: (v) => ctl.previewPan(track, v * 2 - 1),
                onChangeEnd: (v) => ctl.setPan(track, v * 2 - 1),
              ),
              Column(
                children: [
                  _SmallToggle(
                    key: Key('mixer.mute.$track'),
                    label: 'M',
                    on: mixer?.mute ?? false,
                    onColor: AppColors.queued,
                    onTap: () => ctl.setMute(track, !(mixer?.mute ?? false)),
                  ),
                  const SizedBox(height: 2),
                  _SmallToggle(
                    key: Key('mixer.solo.$track'),
                    label: 'S',
                    on: mixer?.solo ?? false,
                    onColor: const Color(0xFF59C3FF),
                    onTap: () => ctl.setSolo(track, !(mixer?.solo ?? false)),
                  ),
                ],
              ),
            ],
          ),
          const SizedBox(height: 4),
          Expanded(
            child: GainSection(
              faderKey: Key('mixer.fader.$track'),
              gainDb: gainDb,
              color: color,
              faderWidth: 40,
              labelSize: MixerPanel.labelSize,
              meter: LevelMeter(
                key: Key('mixer.meter.$track'),
                repaint: ticker,
                level: (ch) => ticker.state.trackPeak[track * 2 + ch],
              ),
              meterWidth: 10,
              onPreview: (db) => ctl.previewGain(track, db),
              onCommit: (db) => ctl.setGain(track, db),
            ),
          ),
        ],
      ),
    );
  }
}

class MasterStrip extends ConsumerWidget {
  const MasterStrip({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final gainDb = ref.watch(projectControllerProvider.select((s) => s?.project.master.gainDb ?? 0));
    final ticker = ref.watch(engineStateTickerProvider);
    final ctl = ref.read(projectControllerProvider.notifier);
    return Padding(
      padding: const EdgeInsets.fromLTRB(2, MixerPanel.verticalInset, 2, MixerPanel.verticalInset),
      child: Column(
        children: [
          Text(S.mixerMaster, style: const TextStyle(fontSize: 10)),
          const SizedBox(height: 4),
          Expanded(
            child: GainSection(
              faderKey: const Key('mixer.fader.master'),
              gainDb: gainDb,
              color: AppColors.textPrimary,
              faderWidth: 34,
              labelSize: MixerPanel.labelSize,
              meter: LevelMeter(
                key: const Key('mixer.meter.master'),
                repaint: ticker,
                level: (ch) => ticker.state.masterPeak[ch],
              ),
              meterWidth: 8,
              gap: 2,
              onPreview: ctl.previewMasterGain,
              onCommit: ctl.setMasterGain,
            ),
          ),
        ],
      ),
    );
  }
}

/// Meter + fader + nhãn dB. Lúc kéo: [onPreview] ≤ 1 lần/frame (chỉ gửi lệnh) và nhãn đổi trong ô cố định có
/// boundary riêng — strip không rebuild. Thả tay/chạm đúp: [onCommit] ghi model (autosave).
class GainSection extends StatefulWidget {
  const GainSection({
    super.key,
    required this.faderKey,
    required this.gainDb,
    required this.color,
    required this.faderWidth,
    required this.labelSize,
    required this.meter,
    required this.meterWidth,
    required this.onPreview,
    required this.onCommit,
    this.gap = 6,
  });

  final Key faderKey;
  final double gainDb;
  final Color color;
  final double faderWidth;
  final double labelSize;
  final Widget meter;
  final double meterWidth;
  final double gap;
  final ValueChanged<double> onPreview;
  final ValueChanged<double> onCommit;

  @override
  State<GainSection> createState() => _GainSectionState();
}

class _GainSectionState extends State<GainSection> {
  late final ValueNotifier<double> _shown = ValueNotifier(widget.gainDb);

  @override
  void didUpdateWidget(GainSection old) {
    super.didUpdateWidget(old);
    if (old.gainDb != widget.gainDb) _shown.value = widget.gainDb;
  }

  @override
  void dispose() {
    _shown.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Expanded(
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              SizedBox(width: widget.meterWidth, child: widget.meter),
              SizedBox(width: widget.gap),
              Fader(
                key: widget.faderKey,
                value: GainScale.toNorm(widget.gainDb),
                color: widget.color,
                width: widget.faderWidth,
                onChanged: (v) {
                  final db = GainScale.toDb(v);
                  widget.onPreview(db); // lệnh trước…
                  _shown.value = db; // …rồi mới vẽ nhãn
                },
                onChangeEnd: (v) => widget.onCommit(GainScale.toDb(v)),
              ),
            ],
          ),
        ),
        SizedBox(
          width: 56,
          height: widget.labelSize + 5,
          child: RepaintBoundary(
            child: ValueListenableBuilder<double>(
              valueListenable: _shown,
              builder: (_, db, _) => Text(
                GainScale.label(db),
                maxLines: 1,
                textAlign: TextAlign.center,
                style: AppText.numeric.copyWith(fontSize: widget.labelSize),
              ),
            ),
          ),
        ),
      ],
    );
  }
}

/// Chọn monitor input của track (06 §2 `tracks[].monitor`): Tắt / Tự động (nghe mic khi arm) / Luôn bật.
class _MonitorChip extends StatelessWidget {
  const _MonitorChip({super.key, required this.mode, required this.onSelected});

  final MonitorMode mode;
  final ValueChanged<MonitorMode> onSelected;

  static Map<MonitorMode, String> get _names => {
    MonitorMode.off: S.mixerMonitorTat,
    MonitorMode.auto: S.mixerMonitorTuDongKhiArm,
    MonitorMode.always: S.mixerMonitorLuonBat,
  };

  @override
  Widget build(BuildContext context) {
    final color = switch (mode) {
      MonitorMode.off => AppColors.textSecondary,
      MonitorMode.auto => AppColors.queued,
      MonitorMode.always => AppColors.play,
    };
    return PopupMenuButton<MonitorMode>(
      tooltip: '${S.mixerMonitor}: ${_names[mode]}',
      onSelected: onSelected,
      itemBuilder: (_) => [
        for (final m in MonitorMode.values)
          CheckedPopupMenuItem(key: Key('monitor.${m.name}'), value: m, checked: m == mode, child: Text(_names[m]!)),
      ],
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 3, vertical: 1),
        decoration: BoxDecoration(
          border: Border.all(color: color),
          borderRadius: BorderRadius.circular(4),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.headphones, size: 11, color: color),
            const SizedBox(width: 2),
            // Chuỗi riêng cho badge (≤ 4 ký tự: Tắt / Auto / Bật) — không bị cắt ở strip hẹp; tên đủ ở tooltip / menu.
            Text(S.monitorBadge(mode.name), maxLines: 1, style: TextStyle(fontSize: 10, color: color)),
          ],
        ),
      ),
    );
  }
}

class _SmallToggle extends StatelessWidget {
  const _SmallToggle({super.key, required this.label, required this.on, required this.onColor, required this.onTap});

  final String label;
  final bool on;
  final Color onColor;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => GestureDetector(
    behavior: HitTestBehavior.opaque,
    onTap: onTap,
    child: Container(
      width: 24,
      height: 20,
      alignment: Alignment.center,
      decoration: BoxDecoration(
        color: on ? onColor : AppColors.surface,
        borderRadius: BorderRadius.circular(3),
        border: Border.all(color: AppColors.border),
      ),
      child: Text(
        label,
        style: TextStyle(
          fontSize: 11,
          fontWeight: FontWeight.w700,
          color: on ? AppColors.background : AppColors.textSecondary,
        ),
      ),
    ),
  );
}
