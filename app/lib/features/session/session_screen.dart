import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../data/data_providers.dart';
import '../../engine/app_error.dart';
import '../../engine/engine_error_bus.dart';
import '../../engine/engine_providers.dart';
import '../../engine/engine_state_ticker.dart';
import '../../engine/keep_screen_on.dart';
import 'project_controller.dart';
import 'session_layout.dart';
import 'widgets/bottom_panel.dart';
import 'widgets/loop_button.dart';
import 'widgets/clip_cell.dart';
import 'widgets/scene_column.dart';
import 'widgets/track_header.dart';
import 'widgets/transport_bar.dart';
import '../../l10n/l10n.dart';

/// Màn Session (P2-06): transport 56 · header 64 · grid 8×8 + cột scene 56 · panel 260/44 (07 §2).
///
/// Repaint boundary: transport bar (readout 60Hz riêng) · header từng track (meter riêng) · từng
/// ClipCell · từng nút scene · panel · banner. Ticker state chỉ chạy khi màn này hiển thị (P2-02).
class SessionScreen extends ConsumerStatefulWidget {
  const SessionScreen({super.key});

  @override
  ConsumerState<SessionScreen> createState() => _SessionScreenState();
}

class _SessionScreenState extends ConsumerState<SessionScreen> {
  final _panelCollapsed = ValueNotifier<bool>(false);
  late final KeepScreenOn _keepScreenOn = KeepScreenOn(ref.read(engineStateTickerProvider));
  late final _ticker = ref.read(engineStateTickerProvider);
  bool _wasPlaying = false;

  @override
  void initState() {
    super.initState();
    _keepScreenOn; // màn hình luôn sáng khi transport chạy (P2-13)
    _ticker.addListener(_onTick);
  }

  /// Transport vừa dừng → đọc lại tempoState (pedal mode: dừng + hết clip → chờ vòng đầu). Chỉ khi cờ đổi,
  /// không phải mỗi frame; ticker chỉ chạy khi Session hiển thị (P2-02).
  void _onTick() {
    if (!mounted) return;
    final p = _ticker.state.playing;
    if (p == _wasPlaying) return;
    _wasPlaying = p;
    if (!p) ref.read(projectControllerProvider.notifier).refreshTempo();
  }

  @override
  void dispose() {
    _ticker.removeListener(_onTick);
    _keepScreenOn.dispose();
    _panelCollapsed.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final hasProject = ref.watch(projectControllerProvider.select((s) => s != null));
    final ticker = ref.watch(engineStateTickerProvider);
    if (!hasProject) {
      return Scaffold(
        appBar: AppBar(title: Text(S.sessionTitle)),
        body: Center(child: Text(S.sessionChuaMoProject)),
      );
    }
    return EngineTickerScope(
      ticker: ticker,
      child: Scaffold(
        body: SafeArea(
          child: Stack(
            children: [
              Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  const RepaintBoundary(
                    child: SizedBox(height: SessionLayout.topBarHeight, child: TransportBar()),
                  ),
                  SizedBox(
                    key: const Key('session.headerRow'),
                    height: SessionLayout.headerHeight,
                    child: Row(
                      children: [
                        for (var t = 0; t < SessionLayout.tracks; t++)
                          Expanded(
                            child: RepaintBoundary(child: TrackHeader(track: t)),
                          ),
                        // Nút ● LOOP + ■ Dừng track + ↶ Hoàn tác (07 §3.1b): luôn thấy, dù panel mở hay thu gọn.
                        const SizedBox(
                          width: SessionLayout.sceneColumnWidth,
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              LoopButton(size: SessionLayout.loopButtonSize),
                              SizedBox(height: 2),
                              Row(
                                mainAxisAlignment: MainAxisAlignment.center,
                                children: [TrackStopButton(), SizedBox(width: 4), LoopUndoButton()],
                              ),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ),
                  const Expanded(child: _ClipGrid()),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.end,
                    children: [
                      Expanded(
                        child: RepaintBoundary(child: BottomPanel(collapsed: _panelCollapsed)),
                      ),
                      const SizedBox(
                        key: Key('session.stopAll'),
                        width: SessionLayout.sceneColumnWidth,
                        height: SessionLayout.panelCollapsedHeight,
                        child: StopAllButton(),
                      ),
                    ],
                  ),
                ],
              ),
              const Positioned(
                top: SessionLayout.topBarHeight + 4,
                left: 0,
                right: 0,
                child: RepaintBoundary(child: _Banner()),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// Grid 8×8 + cột scene. Cột/hàng dùng Expanded → khi panel thu gọn, ô giãn ra bằng layout,
/// không widget nào rebuild (P2-06 DoD).
class _ClipGrid extends StatefulWidget {
  const _ClipGrid();

  @override
  State<_ClipGrid> createState() => _ClipGridState();
}

class _ClipGridState extends State<_ClipGrid> {
  // Dựng cột ô MỘT lần: khi panel co / giãn / mở rộng, LayoutBuilder chỉ đổi SizedBox bên ngoài, cùng instance
  // cột ô → không ClipCell nào rebuild (P2-06), chỉ layout lại.
  late final Widget _column = KeyedSubtree(key: sessionGridKey, child: _gridColumn());

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, c) {
        const minH = SessionLayout.minCellHeight * SessionLayout.scenes;
        final fits = c.maxHeight >= minH;
        // Panel mở rộng (⤢): hàng vẫn ≥ 44 pt → grid cuộn dọc thay vì bóp ô.
        return SingleChildScrollView(
          key: const Key('session.gridScroll'),
          physics: fits ? const NeverScrollableScrollPhysics() : null,
          child: SizedBox(height: fits ? c.maxHeight : minH, child: _column),
        );
      },
    );
  }

  Widget _gridColumn() {
    return Column(
      key: const Key('session.grid'),
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (var s = 0; s < SessionLayout.scenes; s++)
          Expanded(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var t = 0; t < SessionLayout.tracks; t++)
                  Expanded(
                    child: ClipCell(key: Key('cell.$t.$s'), track: t, slot: s),
                  ),
                SizedBox(
                  width: SessionLayout.sceneColumnWidth,
                  child: RepaintBoundary(
                    child: SceneButton(key: Key('scene.$s'), scene: s),
                  ),
                ),
              ],
            ),
          ),
      ],
    );
  }
}

/// Banner không chặn thao tác (07 §4.3, P2-13): tiến độ mở project, lỗi mở project, lỗi autosave,
/// và thông báo của EngineErrorBus (lỗi engine, ngắt audio, Bluetooth). Mỗi dòng đóng được.
class _Banner extends ConsumerWidget {
  const _Banner();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final errors = ref.watch(projectControllerProvider.select((s) => s?.errors ?? const <AppError>[]));
    final progress = ref.watch(projectControllerProvider.select((s) => s?.progress));
    final bus = ref.watch(engineErrorBusProvider);
    final saveError = ref.watch(autosaveProvider).lastError;
    final audio = ref.watch(engineAudioProvider);
    return ListenableBuilder(
      listenable: Listenable.merge([bus, saveError, audio]),
      builder: (context, _) {
        final rows = <Widget>[
          if (progress != null) _row(S.sessionDangMoProject(progress.done, progress.total), BannerLevel.info),
          // 07 §4.0: từ chối quyền mic → chỉ phát, khoá nút thu. Không đóng được: hết khi có quyền.
          if (audio.outputOnly)
            _row(
              S.sessionChuaCoQuyenMicroChi,
              BannerLevel.warning,
              key: const Key('banner.mic'),
              // Chưa hỏi quyền → hỏi ngay; đã từ chối → chỉ bật lại được trong Cài đặt iOS.
              actionLabel: audio.micPermission == MicPermission.undetermined
                  ? S.sessionChoPhepMicro
                  : S.onboardingMoCaiDat,
              onAction: audio.micPermission == MicPermission.undetermined ? audio.ensureMic : audio.openAppSettings,
            ),
          for (final m in bus.messages)
            _row(m.text, m.level, key: Key('banner.${m.key ?? m.id}'), onClose: () => bus.dismiss(m.id)),
          if (errors.isNotEmpty)
            _row(
              S.sessionLoiKhiMoProject(errors.length, S.errorText(errors.first.code)),
              BannerLevel.error,
              key: const Key('banner.openErrors'),
              onClose: () => ref.read(projectControllerProvider.notifier).clearErrors(),
            ),
          if (saveError.value case final code?)
            _row(S.sessionKhongLuuDuocProject(S.errorText(code)), BannerLevel.error, key: const Key('banner.save')),
        ];
        if (rows.isEmpty) return const SizedBox.shrink();
        return Align(
          alignment: Alignment.topCenter,
          child: Container(
            key: const Key('session.banner'),
            margin: const EdgeInsets.symmetric(horizontal: 120),
            padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
            decoration: BoxDecoration(
              color: const Color(0xF0222428),
              borderRadius: BorderRadius.circular(6),
              border: Border.all(color: AppColors.border),
            ),
            child: Column(mainAxisSize: MainAxisSize.min, children: rows),
          ),
        );
      },
    );
  }

  static Widget _row(
    String text,
    BannerLevel level, {
    Key? key,
    VoidCallback? onClose,
    String? actionLabel,
    VoidCallback? onAction,
  }) {
    final color = switch (level) {
      BannerLevel.info => AppColors.textSecondary,
      BannerLevel.warning => AppColors.queued,
      BannerLevel.error => AppColors.record,
    };
    return Row(
      key: key,
      mainAxisSize: MainAxisSize.min,
      children: [
        Icon(level == BannerLevel.info ? Icons.hourglass_top : Icons.warning_amber, size: 16, color: color),
        const SizedBox(width: 8),
        Flexible(child: Text(text, style: const TextStyle(fontSize: 13))),
        if (actionLabel != null)
          TextButton(
            key: const Key('banner.action'),
            style: TextButton.styleFrom(visualDensity: VisualDensity.compact),
            onPressed: onAction,
            child: Text(actionLabel),
          ),
        if (onClose != null)
          IconButton(
            visualDensity: VisualDensity.compact,
            iconSize: 16,
            onPressed: onClose,
            icon: const Icon(Icons.close),
          ),
      ],
    );
  }
}
