import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../model/project.dart';
import '../../../ui_kit/meter.dart';
import '../project_controller.dart';
import '../session_layout.dart';
import '../session_ui.dart';
import 'track_menu.dart';
import '../../../l10n/l10n.dart';

@immutable
final class _HeaderInfo {
  const _HeaderInfo(this.name, this.color, this.mute, this.solo, this.armed, this.isAudio);

  factory _HeaderInfo.of(ProjectSession? s, int t) {
    final tr = s?.project.trackAt(t);
    return _HeaderInfo(
      tr?.name ?? S.sessionTrackName(t + 1),
      tr == null ? AppColors.tracks[t % 8] : parseHexColor(tr.color),
      tr?.mixer.mute ?? false,
      tr?.mixer.solo ?? false,
      s?.armed.contains(t) ?? false,
      tr?.kind != TrackKind.instrument,
    );
  }

  final String name;
  final Color color;
  final bool mute;
  final bool solo;
  final bool armed;

  /// Track audio thu bằng mic (track instrument thu MIDI).
  final bool isAudio;

  @override
  bool operator ==(Object other) =>
      other is _HeaderInfo &&
      other.name == name &&
      other.color == color &&
      other.mute == mute &&
      other.solo == solo &&
      other.armed == armed &&
      other.isAudio == isAudio;
  @override
  int get hashCode => Object.hash(name, color, mute, solo, armed, isAudio);
}

/// Header track (P2-08): tên, meter L/R, Arm ●, M, S. Cao 64 pt (07 §2).
/// Meter vẽ theo Ticker trong RepaintBoundary riêng — không rebuild (07 §6.1).
class TrackHeader extends ConsumerWidget {
  const TrackHeader({super.key, required this.track});

  final int track;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final info = ref.watch(projectControllerProvider.select((s) => _HeaderInfo.of(s, track)));
    final ticker = ref.watch(engineStateTickerProvider);
    final ctl = ref.read(projectControllerProvider.notifier);
    final selected = ref.watch(sessionUiProvider.select((u) => u.selectedTrack == track));
    // Đang kéo mục Browser vào header này (07 §4.1e) → viền vàng.
    final dropping = ref.watch(sessionUiProvider.select((u) => u.dragTarget == CellRef(track, -1)));
    // Chỉ phát (thiếu quyền mic): khoá arm của track audio (07 §4.0).
    final armLocked = info.isAudio && !ref.watch(canRecordAudioProvider);
    return Container(
      decoration: BoxDecoration(
        color: selected ? const Color(0xFF22262D) : AppColors.surface,
        border: dropping
            ? Border.all(color: AppColors.queued, width: 2)
            : Border(
                top: BorderSide(color: info.color, width: 3),
                right: const BorderSide(color: AppColors.background, width: 2),
              ),
      ),
      padding: const EdgeInsets.fromLTRB(6, 4, 6, 4),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          GestureDetector(
            key: Key('header.name.$track'),
            behavior: HitTestBehavior.opaque,
            onTap: () => ref.read(sessionUiProvider.notifier).selectTrack(track), // nhận nốt pad/bàn phím
            // Nhấn giữ: đổi tên / đổi loại track (header không phải đường nóng, không sợ chạm nhầm lúc diễn).
            onLongPressStart: (d) => showTrackMenu(context, ref, track, d.globalPosition),
            child: Text(
              info.name,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600, color: AppColors.textPrimary),
            ),
          ),
          const SizedBox(height: 4),
          Expanded(
            child: Row(
              children: [
                Expanded(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(vertical: 6),
                    child: LevelMeter(
                      key: Key('header.meter.$track'),
                      repaint: ticker,
                      axis: Axis.horizontal,
                      level: (ch) => ticker.state.trackPeak[track * 2 + ch],
                    ),
                  ),
                ),
                const SizedBox(width: 4),
                _Toggle(
                  key: Key('header.arm.$track'),
                  icon: Icons.fiber_manual_record, // icon thay ký tự "●": không phụ thuộc glyph của font
                  on: info.armed,
                  onColor: AppColors.record,
                  enabled: !armLocked,
                  // Arm track audio lần đầu → hỏi quyền mic (07 §4.0); bỏ arm thì làm ngay.
                  onTap: () => info.armed ? ctl.setArm(track, false) : ctl.armForRecording(track),
                ),
                _Toggle(
                  key: Key('header.mute.$track'),
                  label: 'M',
                  on: info.mute,
                  onColor: AppColors.queued,
                  onTap: () => ctl.setMute(track, !info.mute),
                ),
                _Toggle(
                  key: Key('header.solo.$track'),
                  label: 'S',
                  on: info.solo,
                  onColor: const Color(0xFF59C3FF),
                  onTap: () => ctl.setSolo(track, !info.solo),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _Toggle extends StatelessWidget {
  const _Toggle({
    super.key,
    this.label = '',
    this.icon,
    required this.on,
    required this.onColor,
    required this.onTap,
    this.enabled = true,
  });

  final String label;
  final IconData? icon;
  final bool on;
  final Color onColor;
  final VoidCallback onTap;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final fg = on ? AppColors.background : (enabled ? AppColors.textSecondary : AppColors.border);
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: enabled ? onTap : null,
      child: Container(
        width: 26,
        height: 28,
        margin: const EdgeInsets.only(left: 2),
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: on ? onColor : AppColors.background,
          borderRadius: BorderRadius.circular(4),
          border: Border.all(color: enabled ? AppColors.border : AppColors.background),
        ),
        child: icon != null
            ? Icon(icon, size: 12, color: fg)
            : Text(
                label,
                style: TextStyle(fontSize: 12, fontWeight: FontWeight.w700, color: fg),
              ),
      ),
    );
  }
}
