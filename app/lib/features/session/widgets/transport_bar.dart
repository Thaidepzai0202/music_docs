import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../app/router.dart';
import '../../../engine/engine_providers.dart';
import '../../../engine/engine_state_ticker.dart';
import '../../../engine/performance_actions.dart';
import '../../../model/project.dart';
import '../../export/jam_recorder.dart';
import '../../export/export_sheet.dart';
import '../../../ui_kit/drag_value.dart';
import '../../../ui_kit/frame_throttle.dart';
import '../../../ui_kit/metronome_icon.dart';
import '../project_controller.dart';
import '../session_layout.dart';
import '../session_ui.dart';
import '../tap_tempo.dart';
import '../../../l10n/l10n.dart';

/// Transport bar (P2-10, 07 §2): ◀ tên · ▶ ■ · BPM (kéo + TAP) · Q · Metro/count-in · vị trí · CPU/xrun ·
/// ● REC jam · ⋮ (Export, Cài đặt) · ✎.
class TransportBar extends ConsumerWidget {
  const TransportBar({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final name = ref.watch(projectControllerProvider.select((s) => s?.project.name ?? ''));
    final actions = ref.read(performanceActionsProvider);
    final ticker = ref.watch(engineStateTickerProvider);
    return ColoredBox(
      key: const Key('session.topBar'),
      color: AppColors.surface,
      child: Row(
        children: [
          IconButton(
            key: const Key('session.back'),
            icon: const Icon(Icons.chevron_left),
            onPressed: () => Navigator.maybePop(context),
          ),
          // Tên project co giãn theo chỗ trống (có "…"), các nút transport giữ nguyên kích thước.
          Expanded(
            child: Text(
              name,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w600),
            ),
          ),
          const SizedBox(width: 4),
          _PointerIcon(
            key: const Key('session.play'),
            icon: Icons.play_arrow,
            color: AppColors.play,
            onDown: actions.transportPlay,
          ),
          _PointerIcon(
            key: const Key('session.stop'),
            icon: Icons.stop,
            color: AppColors.textPrimary,
            onDown: actions.transportStop,
          ),
          const SizedBox(width: 8),
          const BpmControl(),
          const TapTempoButton(),
          const SizedBox(width: 8),
          const _QuantizeMenu(),
          const _MetronomeMenu(),
          const SizedBox(width: 8),
          SizedBox(
            width: 124,
            height: SessionLayout.topBarHeight,
            child: RepaintBoundary(
              child: CustomPaint(
                key: const Key('transport.readout'),
                painter: TransportReadoutPainter(ticker, font: painterFont(context)),
              ),
            ),
          ),
          const JamButton(),
          // Export + Cài đặt chung một nút để thanh không tràn trên iPad 8 (1080 pt).
          PopupMenuButton<String>(
            key: const Key('transport.more'),
            tooltip: S.transportThem,
            icon: const Icon(Icons.more_vert, size: 20),
            onSelected: (v) => switch (v) {
              'export' => showExportSheet(context),
              _ => Navigator.pushNamed(context, AppRoutes.settings),
            },
            itemBuilder: (_) => [
              PopupMenuItem(
                key: Key('more.export'),
                value: 'export',
                child: ListTile(leading: const Icon(Icons.ios_share), title: Text(S.exportMenu)),
              ),
              PopupMenuItem(
                key: Key('transport.settings'),
                value: 'settings',
                child: ListTile(leading: Icon(Icons.settings), title: Text(S.projectsCaiDat)),
              ),
            ],
          ),
          const _EditToggle(),
          const SizedBox(width: 4),
        ],
      ),
    );
  }
}

/// REC master (P3-20): ghi buổi jam. Đang ghi → nút đỏ kèm thời gian; dừng → SnackBar có "Chia sẻ".
class JamButton extends ConsumerWidget {
  const JamButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final jam = ref.watch(jamRecorderProvider);
    return Tooltip(
      message: jam.recording ? S.exportDungGhiJam : S.transportGhiBuoiJamMaster,
      child: InkWell(
        key: const Key('transport.jam'),
        borderRadius: BorderRadius.circular(18),
        onTap: () {
          final rec = ref.read(jamRecorderProvider.notifier);
          final messenger = ScaffoldMessenger.maybeOf(context);
          if (!jam.recording) {
            final err = rec.start();
            if (err != null) {
              messenger?.showSnackBar(SnackBar(content: Text(S.transportKhongGhiDuocJam(S.errorText(err)))));
            }
            return;
          }
          final r = rec.stop();
          if (r == null) return;
          final dropped = r.droppedFrames > 0
              ? S.jamDroppedMs(r.droppedMs(ref.read(engineStateTickerProvider).state.sampleRate))
              : null;
          messenger?.showSnackBar(
            SnackBar(
              // Rớt khung khi ghi → thêm dòng cảnh báo trong cùng SnackBar (không chặn).
              content: Text([S.transportDaGhi(r.fileName, formatSeconds(r.seconds)), ?dropped].join('\n')),
              action: SnackBarAction(label: S.exportChiaSe, onPressed: () => shareExport(context, ref, r)),
            ),
          );
        },
        child: Container(
          height: 32,
          padding: const EdgeInsets.symmetric(horizontal: 8),
          decoration: BoxDecoration(
            color: jam.recording ? AppColors.record : Colors.transparent,
            border: Border.all(color: AppColors.record),
            borderRadius: BorderRadius.circular(16),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(
                Icons.fiber_manual_record,
                size: 14,
                color: jam.recording ? AppColors.textPrimary : AppColors.record,
              ),
              if (jam.recording) ...[
                const SizedBox(width: 4),
                ElapsedText(
                  since: jam.startedAt!,
                  width: 38,
                  height: 16,
                  style: const TextStyle(fontSize: 12, fontFeatures: AppText.tabular),
                ),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _PointerIcon extends StatelessWidget {
  const _PointerIcon({super.key, required this.icon, required this.color, required this.onDown});

  final IconData icon;
  final Color color;
  final VoidCallback onDown;

  @override
  Widget build(BuildContext context) => Listener(
    behavior: HitTestBehavior.opaque,
    onPointerDown: (_) => onDown(),
    child: Padding(
      padding: const EdgeInsets.all(10),
      child: Icon(icon, color: color),
    ),
  );
}

/// BPM: kéo dọc (4 px ≈ 1 BPM, tinh chỉnh ×0.1), chạm đúp về 120. Gửi tối đa 1 lệnh/frame.
class BpmControl extends ConsumerStatefulWidget {
  const BpmControl({super.key});

  static double toNorm(double bpm) => (bpm - TapTempo.minBpm) / (TapTempo.maxBpm - TapTempo.minBpm);
  static double fromNorm(double n) => TapTempo.minBpm + n * (TapTempo.maxBpm - TapTempo.minBpm);

  @override
  ConsumerState<BpmControl> createState() => _BpmControlState();
}

class _BpmControlState extends ConsumerState<BpmControl> {
  // Tạo ngay trong initState (không `late` lười): khi đang chờ vòng đầu build không chạm tới nó, và dispose()
  // không được dùng `ref`.
  late final DragValue _drag;

  @override
  void initState() {
    super.initState();
    _drag = DragValue(
      initial: BpmControl.toNorm(ref.read(projectControllerProvider)?.project.transport.bpm ?? 120),
      defaultValue: BpmControl.toNorm(120),
      onChanged: (n) => ref.read(projectControllerProvider.notifier).setBpm(BpmControl.fromNorm(n)),
    );
  }

  @override
  void dispose() {
    _drag.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    ref.listen(projectControllerProvider.select((s) => s?.project.transport.bpm), (_, bpm) {
      if (bpm != null) _drag.syncFrom(BpmControl.toNorm(bpm));
    });
    final waiting = ref.watch(projectControllerProvider.select((s) => s?.waitingFirstLoop ?? false));
    if (waiting) {
      // Pedal mode chờ vòng đầu (07 §3.1b): chưa có BPM → không kéo được; vòng đầu sẽ quyết định.
      // Chiếm cả chỗ của nút TAP (ẩn khi chờ) → 2 dòng gọn, không làm top bar tràn.
      return Container(
        key: const Key('transport.bpm.waiting'),
        width: 150,
        height: 36,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: AppColors.background,
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: AppColors.border),
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(S.transportBpmTrong, style: AppText.numeric.copyWith(fontSize: 13, height: 1.1)),
            Text(
              S.transportChoVongDau,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 10, height: 1.1, color: AppColors.textSecondary),
            ),
          ],
        ),
      );
    }
    return _drag.wrap(
      pixelsForFullRange: 4 * (TapTempo.maxBpm - TapTempo.minBpm),
      child: Container(
        key: const Key('transport.bpm'),
        width: 96,
        height: 36,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: AppColors.background,
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: AppColors.border),
        ),
        child: ValueListenableBuilder<double>(
          valueListenable: _drag.value,
          builder: (_, n, _) => Text(S.transportBpm(BpmControl.fromNorm(n).toStringAsFixed(1)), style: AppText.numeric),
        ),
      ),
    );
  }
}

class TapTempoButton extends ConsumerStatefulWidget {
  const TapTempoButton({super.key});

  @override
  ConsumerState<TapTempoButton> createState() => _TapTempoButtonState();
}

class _TapTempoButtonState extends ConsumerState<TapTempoButton> {
  final _tap = TapTempo();

  @override
  Widget build(BuildContext context) {
    // Chờ vòng đầu: BPM do vòng đầu quyết định → không có tap tempo (ô BPM chiếm chỗ này).
    if (ref.watch(projectControllerProvider.select((s) => s?.waitingFirstLoop ?? false))) {
      return const SizedBox.shrink();
    }
    return Listener(
      key: const Key('transport.tap'),
      behavior: HitTestBehavior.opaque,
      onPointerDown: (e) {
        final bpm = _tap.tap(e.timeStamp);
        if (bpm != null) ref.read(projectControllerProvider.notifier).setBpm(bpm);
      },
      child: Container(
        margin: const EdgeInsets.only(left: 6),
        padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(6),
          border: Border.all(color: AppColors.border),
        ),
        child: Text(S.transportTap, style: const TextStyle(fontSize: 12, fontWeight: FontWeight.w700)),
      ),
    );
  }
}

class _QuantizeMenu extends ConsumerWidget {
  const _QuantizeMenu();

  static Map<QuantizeGrid, String> get labels => {for (final g in QuantizeGrid.values) g: S.quantizeGrid(g.name)};

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final q = ref.watch(projectControllerProvider.select((s) => s?.project.transport.quantize ?? QuantizeGrid.bar1));
    return PopupMenuButton<QuantizeGrid>(
      key: const Key('transport.quantize'),
      tooltip: S.transportQuantize,
      onSelected: (v) => ref.read(projectControllerProvider.notifier).setQuantize(v),
      itemBuilder: (_) => [
        for (final e in labels.entries)
          PopupMenuItem(key: Key('quantize.${e.key.name}'), value: e.key, child: Text(e.value)),
      ],
      child: _MenuChip(text: S.transportQuantizeChip(labels[q]!)),
    );
  }
}

/// Metronome + count-in chung một menu (07 §2 chỉ có "♩ Metro" trên thanh; count-in ít đổi lúc chơi).
/// Metronome (chế độ + âm lượng), count-in và nhịp chung một menu (07 §2: thanh trên chỉ có "♩").
class _MetronomeMenu extends ConsumerWidget {
  const _MetronomeMenu();

  static Map<MetronomeMode, String> get labels => {for (final m in MetronomeMode.values) m: S.metronomeMode(m.name)};

  /// Nhịp chọn được (06 §2 `transport.timeSignature`).
  static const signatures = [(4, 4), (3, 4), (6, 8)];

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final metro =
        ref.watch(projectControllerProvider.select((s) => s?.project.transport.metronome)) ?? const Metronome();
    final m = metro.mode;
    final countIn = ref.watch(projectControllerProvider.select((s) => s?.project.transport.countInBars ?? 0));
    final sig = ref.watch(projectControllerProvider.select((s) => s?.project.transport.timeSignature)) ?? const [4, 4];
    final sigText = '${sig[0]}/${sig[1]}';
    final tempoMode =
        ref.watch(projectControllerProvider.select((s) => s?.project.transport.tempoMode)) ?? TempoMode.fixed;
    final waiting = ref.watch(projectControllerProvider.select((s) => s?.waitingFirstLoop ?? false));
    final ctl = ref.read(projectControllerProvider.notifier);
    return PopupMenuButton<Object>(
      key: const Key('transport.metronome'),
      tooltip: S.transportMetronomeCountInNhip,
      onSelected: (v) {
        if (v is TempoMode) ctl.setTempoMode(v);
        if (v is MetronomeMode) ctl.setMetronome(v);
        if (v is int) ctl.setCountIn(v);
        if (v is (int, int)) ctl.setTimeSignature(v.$1, v.$2);
      },
      itemBuilder: (_) => [
        // 07 §3.1b: BPM cố định | Vòng đầu quyết định BPM (pedal mode).
        for (final t in TempoMode.values)
          CheckedPopupMenuItem<Object>(
            key: Key('tempoMode.${t.name}'),
            value: t,
            checked: t == tempoMode,
            child: Text(S.tempoMode(t.name)),
          ),
        const PopupMenuDivider(),
        for (final e in labels.entries)
          CheckedPopupMenuItem<Object>(
            key: Key('metronome.${e.key.name}'),
            value: e.key,
            checked: e.key == m,
            child: Text(S.transportMetronomeItem(e.value)),
          ),
        _VolumeEntry(
          initial: metro.volume,
          onPreview: ctl.previewMetronomeVolume,
          onCommit: (v) =>
              ctl.setMetronome(ref.read(projectControllerProvider)?.project.transport.metronome.mode ?? m, volume: v),
        ),
        const PopupMenuDivider(),
        for (final v in const [0, 1, 2])
          CheckedPopupMenuItem<Object>(
            key: Key('countIn.$v'),
            value: v,
            checked: v == countIn,
            child: Text(v == 0 ? S.transportKhongDemVao : S.transportDemVaoBar(v)),
          ),
        const PopupMenuDivider(),
        for (final (n, d) in signatures)
          CheckedPopupMenuItem<Object>(
            key: Key('timeSig.$n-$d'),
            value: (n, d),
            checked: sig[0] == n && sig[1] == d,
            child: Text(S.transportNhip(n, d)),
          ),
      ],
      child: _MenuChip(
        leading: const MetronomeIcon(),
        // Chờ vòng đầu: metronome tắt (engine không click) → chip hiện "Tắt".
        text: [
          labels[waiting ? MetronomeMode.off : m],
          if (countIn > 0 && !waiting) '$countIn',
          if (sigText != '4/4') sigText,
        ].join(' · '),
        active: m != MetronomeMode.off && !waiting,
      ),
    );
  }
}

/// Dòng âm lượng metronome trong menu ♩: kéo → METRONOME f0 ≤ 1 lần/frame (chỉ lệnh); thả tay → lưu
/// `transport.metronome.volume`.
class _VolumeEntry extends PopupMenuEntry<Object> {
  const _VolumeEntry({required this.initial, required this.onPreview, required this.onCommit});

  final double initial;
  final ValueChanged<double> onPreview;
  final ValueChanged<double> onCommit;

  @override
  double get height => 48;

  @override
  bool represents(Object? value) => false;

  @override
  State<_VolumeEntry> createState() => _VolumeEntryState();
}

class _VolumeEntryState extends State<_VolumeEntry> {
  late double _v = widget.initial;
  late final FrameThrottle<double> _throttle = FrameThrottle(widget.onPreview);

  @override
  void dispose() {
    _throttle.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 12),
      child: Row(
        children: [
          const Icon(Icons.volume_up, size: 18, color: AppColors.textSecondary),
          Expanded(
            child: Slider(
              key: const Key('metronome.volume'),
              value: _v,
              onChanged: (v) {
                _throttle.add(v); // lệnh trước…
                setState(() => _v = v); // …rồi mới vẽ
              },
              onChangeEnd: widget.onCommit,
            ),
          ),
          SizedBox(width: 36, child: Text('${(_v * 100).round()}%', style: AppText.numeric.copyWith(fontSize: 12))),
        ],
      ),
    );
  }
}

class _MenuChip extends StatelessWidget {
  const _MenuChip({required this.text, this.active = false, this.leading});

  final String text;
  final bool active;
  final Widget? leading;

  @override
  Widget build(BuildContext context) => Container(
    margin: const EdgeInsets.symmetric(horizontal: 2),
    padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 8),
    decoration: BoxDecoration(
      color: active ? AppColors.play.withValues(alpha: 0.2) : null,
      borderRadius: BorderRadius.circular(6),
      border: Border.all(color: AppColors.border),
    ),
    child: leading == null
        ? Text(text, style: const TextStyle(fontSize: 13))
        : Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              leading!,
              const SizedBox(width: 4),
              Text(text, style: const TextStyle(fontSize: 13)),
            ],
          ),
  );
}

class _EditToggle extends ConsumerWidget {
  const _EditToggle();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final edit = ref.watch(sessionUiProvider.select((u) => u.mode == SessionMode.edit));
    return TextButton.icon(
      key: const Key('transport.edit'),
      style: TextButton.styleFrom(
        foregroundColor: edit ? AppColors.background : AppColors.textPrimary,
        backgroundColor: edit ? AppColors.queued : null,
      ),
      onPressed: () => ref.read(sessionUiProvider.notifier).toggleMode(),
      icon: const Icon(Icons.edit, size: 18),
      label: Text(S.sessionMode(edit ? 'edit' : 'perform')),
    );
  }
}

/// Vị trí "bar.beat" + CPU/xrun, vẽ theo Ticker — 60Hz mà không rebuild (07 §6.1).
class TransportReadoutPainter extends CustomPainter {
  TransportReadoutPainter(this.ticker, {this.font = const TextStyle()})
    : _posStyle = font.merge(_pos0),
      _cpuStyle = font.merge(_cpu0),
      super(repaint: ticker);

  final EngineStateTicker ticker;

  /// [painterFont] của theme.
  final TextStyle font;
  final TextStyle _posStyle;
  final TextStyle _cpuStyle;
  final _pos = TextPainter(textDirection: TextDirection.ltr, maxLines: 1);
  final _cpu = TextPainter(textDirection: TextDirection.ltr, maxLines: 1);
  int? _posKey;
  int? _cpuKey;

  static const _pos0 = TextStyle(
    color: AppColors.textPrimary,
    fontSize: 18,
    fontWeight: FontWeight.w600,
    fontFeatures: AppText.tabular,
  );
  static const _cpu0 = TextStyle(color: AppColors.textSecondary, fontSize: 12, fontFeatures: AppText.tabular);

  /// "bar.beat", đếm từ 1 (beat 0 = 1.1).
  static String position(double beat, int beatsPerBar) {
    final bpb = beatsPerBar <= 0 ? 4 : beatsPerBar;
    final whole = beat < 0 ? 0 : beat.floor();
    return '${whole ~/ bpb + 1}.${whole % bpb + 1}';
  }

  @override
  void paint(Canvas canvas, Size size) {
    final s = ticker.state;
    final whole = s.beat < 0 ? 0 : s.beat.floor();
    final posKey = whole * 64 + s.beatsPerBar;
    if (posKey != _posKey) {
      _posKey = posKey;
      _pos
        ..text = TextSpan(text: position(s.beat, s.beatsPerBar), style: _posStyle)
        ..layout();
    }
    final cpu = (s.cpuLoad * 100).round();
    final cpuKey = cpu * 1000000 + s.xrunCount;
    if (cpuKey != _cpuKey) {
      _cpuKey = cpuKey;
      _cpu
        ..text = TextSpan(text: S.transportCpu(cpu, s.xrunCount), style: _cpuStyle)
        ..layout();
    }
    _pos.paint(canvas, Offset(0, (size.height - _pos.height) / 2));
    _cpu.paint(canvas, Offset(size.width - _cpu.width - 4, (size.height - _cpu.height) / 2));
  }

  @override
  bool shouldRepaint(TransportReadoutPainter old) => old.ticker != ticker || old.font != font;
}
