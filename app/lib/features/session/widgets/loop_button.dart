import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../engine/engine_state.dart';
import '../../../engine/engine_state_ticker.dart';
import '../../../engine/performance_actions.dart';
import '../../../l10n/l10n.dart';
import '../project_controller.dart';
import '../session_ui.dart';

/// Nút ● LOOP (07 §3.1b, pedal mode) cho track đang chọn; ô đích = ô đang chọn của track đó, không có thì −1
/// (engine tự chọn). Engine giữ máy trạng thái (05 §2 `LE_CMD_LOOP_BUTTON`): nút chỉ gửi lệnh, KHÔNG tự tính.
///
/// Chỉ một cử chỉ: chạm (pointer-down) → arm track nếu chưa arm (track audio lần đầu → hỏi quyền mic), rồi gửi
/// LOOP_BUTTON. Dừng track / hoàn tác là 2 nút riêng bên dưới ([TrackStopButton], [LoopUndoButton]) — chạm đúp /
/// nhấn giữ trên LOOP đã bỏ vì pointer-down luôn gửi LOOP trước (07 §3.1b, sửa 29/09).
///
/// Màu / nhãn / vòng tiến độ vẽ theo Ticker trong RepaintBoundary riêng — không rebuild 60 Hz.
class LoopButton extends ConsumerStatefulWidget {
  const LoopButton({super.key, this.size = 72});

  final double size;

  @override
  ConsumerState<LoopButton> createState() => _LoopButtonState();
}

class _LoopButtonState extends ConsumerState<LoopButton> {
  void _down(PointerDownEvent _) {
    final (:track, :slot) = loopTarget(ref);
    final actions = ref.read(performanceActionsProvider);
    final session = ref.read(projectControllerProvider);
    if (session == null) return;
    if (session.armed.contains(track)) {
      actions.loopButton(track, slot); // đường nóng: lệnh ngay trong pointer-down
    } else {
      // Arm trước (track audio lần đầu → hộp xin quyền mic), rồi mới LOOP. Từ chối quyền → không làm gì.
      ref.read(projectControllerProvider.notifier).armForRecording(track).then((ok) {
        if (ok) actions.loopButton(track, slot);
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    final slot = ref.watch(
      sessionUiProvider.select((u) => u.selected != null && u.selected!.track == track ? u.selected!.slot : -1),
    );
    final ticker = ref.watch(engineStateTickerProvider);
    return Semantics(
      button: true,
      label: S.loopButtonTooltip,
      child: Listener(
        key: const Key('session.loop'),
        behavior: HitTestBehavior.opaque,
        onPointerDown: _down,
        child: RepaintBoundary(
          child: CustomPaint(
            size: Size.square(widget.size),
            painter: LoopButtonPainter(ticker: ticker, track: track, slot: slot, font: painterFont(context)),
          ),
        ),
      ),
    );
  }
}

/// Track đang chọn + ô đang chọn của track đó (−1 = để engine tự chọn).
({int track, int slot}) loopTarget(WidgetRef ref) {
  final ui = ref.read(sessionUiProvider);
  final sel = ui.selected;
  return (track: ui.selectedTrack, slot: sel != null && sel.track == ui.selectedTrack ? sel.slot : -1);
}

/// ■ Dừng track đang chọn (`CLIP_STOP`, theo quantize) — 44 pt, dưới nút LOOP.
class TrackStopButton extends ConsumerWidget {
  const TrackStopButton({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) => _SmallSquare(
    key: const Key('session.trackStop'),
    icon: Icons.stop,
    label: S.loopDungTrack,
    onDown: () => ref.read(performanceActionsProvider).stopTrack(ref.read(sessionUiProvider).selectedTrack),
  );
}

/// ↶ Hoàn tác cho ô đang phát / đang chọn của track đang chọn: có lớp overdub → `clip.undoOverdub`; không có →
/// hỏi "Xoá clip?" rồi `clip.clear`. Chỉ bật khi track có clip.
class LoopUndoButton extends ConsumerWidget {
  const LoopUndoButton({super.key});

  Future<void> _press(BuildContext context, WidgetRef ref) async {
    final (:track, :slot) = loopTarget(ref);
    final ticker = ref.read(engineStateTickerProvider);
    final session = ref.read(projectControllerProvider);
    final s = slot >= 0 && session?.project.trackAt(track)?.clipAt(slot) != null
        ? slot
        : ticker.state.trackPlayingSlot[track];
    final clip = s >= 0 ? session?.project.trackAt(track)?.clipAt(s) : null;
    if (clip == null) return;
    final ctl = ref.read(projectControllerProvider.notifier);
    if (ctl.hasUndo(track, s)) {
      ctl.undoOverdub(track, s);
      return;
    }
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(S.loopXoaClipTitle(clip.name)),
        content: Text(S.loopXoaClipBody),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(S.exportHuy)),
          FilledButton(
            key: const Key('loop.confirmDelete'),
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(S.loopXoa),
          ),
        ],
      ),
    );
    if (ok == true) ctl.deleteClip(track, s);
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    final hasClip = ref.watch(
      projectControllerProvider.select((s) => (s?.project.trackAt(track)?.clips.isNotEmpty) ?? false),
    );
    return _SmallSquare(
      key: const Key('session.loopUndo'),
      icon: Icons.undo,
      label: S.loopHoanTac,
      onDown: hasClip ? () => _press(context, ref) : null,
    );
  }
}

class _SmallSquare extends StatelessWidget {
  const _SmallSquare({super.key, required this.icon, required this.label, required this.onDown});

  final IconData icon;
  final String label;
  final VoidCallback? onDown;

  @override
  Widget build(BuildContext context) => Semantics(
    button: true,
    enabled: onDown != null,
    label: label,
    child: Listener(
      behavior: HitTestBehavior.opaque,
      onPointerDown: onDown == null ? null : (_) => onDown!(),
      child: Container(
        width: 44,
        height: 44,
        decoration: BoxDecoration(
          color: AppColors.surface,
          borderRadius: BorderRadius.circular(8),
          border: Border.all(color: AppColors.border),
        ),
        child: Icon(icon, size: 22, color: onDown == null ? AppColors.border : AppColors.textPrimary),
      ),
    ),
  );
}

/// Vẽ nút LOOP theo trạng thái ô đích: đỏ khi thu, xanh khi phát, cam khi overdub, vàng nhấp nháy khi chờ;
/// vòng tiến độ của loop. Paint / TextPainter tạo sẵn (07 §6.5).
class LoopButtonPainter extends CustomPainter {
  LoopButtonPainter({required this.ticker, required this.track, required this.slot, TextStyle font = const TextStyle()})
    : _labels = {
        for (final k in const ['idle', 'rec', 'play', 'dub'])
          k: TextPainter(
            text: TextSpan(
              text: S.loopButtonLabel(k),
              style: font.merge(
                const TextStyle(fontSize: 13, fontWeight: FontWeight.w800, color: AppColors.textPrimary),
              ),
            ),
            textDirection: TextDirection.ltr,
          )..layout(),
      },
      super(repaint: ticker);

  final EngineStateTicker ticker;
  final int track;
  final int slot;
  final Map<String, TextPainter> _labels;

  static const overdubColor = Color(0xFFFF9F0A);
  final _fill = Paint();
  final _ring = Paint()
    ..style = PaintingStyle.stroke
    ..strokeWidth = 4
    ..strokeCap = StrokeCap.round;
  final _track = Paint()
    ..style = PaintingStyle.stroke
    ..strokeWidth = 4
    ..color = AppColors.border;
  final _dot = Paint()..color = AppColors.record;

  /// Trạng thái ô đích: ô chọn, hoặc ô track đang phát / thu.
  ClipState state() {
    final s = slot >= 0 ? slot : ticker.state.trackPlayingSlot[track];
    return s >= 0 ? ticker.clip(track, s).value : ClipState.empty;
  }

  @override
  void paint(Canvas canvas, Size size) {
    final st = state();
    final c = size.center(Offset.zero);
    final r = size.shortestSide / 2 - 3;
    final (Color bg, String label) = switch (st) {
      ClipState.recording => (AppColors.record, 'rec'),
      ClipState.overdubbing => (overdubColor, 'dub'),
      ClipState.playing => (AppColors.play, 'play'),
      ClipState.queuedRecord ||
      ClipState.queuedPlay ||
      ClipState.queuedStop => (ticker.state.beat % 1 < 0.5 ? AppColors.queued : AppColors.surface, 'idle'),
      _ => (AppColors.surface, 'idle'),
    };
    _fill.color = bg;
    canvas.drawCircle(c, r, _fill);
    canvas.drawCircle(c, r, _track);
    final active = st == ClipState.recording || st == ClipState.overdubbing || st == ClipState.playing;
    if (active) {
      final p = ticker.state.trackClipProgress[track].clamp(0.0, 1.0);
      _ring.color = AppColors.textPrimary;
      canvas.drawArc(Rect.fromCircle(center: c, radius: r), -math.pi / 2, 2 * math.pi * p, false, _ring);
    }
    final tp = _labels[label]!;
    if (label == 'idle') {
      // "● LOOP": chấm đỏ trên, chữ dưới.
      canvas.drawCircle(c.translate(0, -8), 7, _dot);
      tp.paint(canvas, c.translate(-tp.width / 2, 4));
    } else {
      tp.paint(canvas, c.translate(-tp.width / 2, -tp.height / 2));
    }
  }

  @override
  bool shouldRepaint(LoopButtonPainter old) => old.ticker != ticker || old.track != track || old.slot != slot;
}
