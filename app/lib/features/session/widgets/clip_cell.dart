import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app/theme.dart';
import '../../../engine/engine_providers.dart';
import '../../../engine/engine_state.dart';
import '../../../engine/engine_state_ticker.dart';
import '../../../engine/performance_actions.dart';
import '../../../model/project.dart';
import '../project_controller.dart';
import '../session_layout.dart';
import '../session_ui.dart';
import 'clip_menu.dart';

/// Dữ liệu model của 1 ô — so sánh bằng giá trị để `select` chỉ rebuild khi ô này đổi.
@immutable
final class ClipCellInfo {
  const ClipCellInfo({
    required this.hasClip,
    required this.name,
    required this.color,
    required this.armed,
    required this.missing,
  });

  factory ClipCellInfo.of(ProjectSession? s, int track, int slot) {
    final t = s?.project.trackAt(track);
    final c = t?.clipAt(slot);
    return ClipCellInfo(
      hasClip: c != null,
      name: c?.name ?? '',
      color: t == null ? AppColors.tracks[track % 8] : parseHexColor(t.color),
      armed: s?.armed.contains(track) ?? false,
      missing: c is AudioClip && (s?.missingClipIds.contains(c.id) ?? false),
    );
  }

  final bool hasClip;
  final String name;
  final Color color;
  final bool armed;
  final bool missing;

  @override
  bool operator ==(Object other) =>
      other is ClipCellInfo &&
      other.hasClip == hasClip &&
      other.name == name &&
      other.color == color &&
      other.armed == armed &&
      other.missing == missing;

  @override
  int get hashCode => Object.hash(hasClip, name, color, armed, missing);
}

/// Ô clip (P2-07). Perform: **pointer-down** → lệnh engine ngay (07 §3.1), không dùng onTap.
///
/// Rebuild: chỉ khi `ValueNotifier<ClipState>` của CHÍNH ô này đổi hoặc model của ô đổi.
/// Repaint: RepaintBoundary riêng; chỉ nghe Ticker khi cần chuyển động (queued nhấp nháy theo beat,
/// thanh tiến độ khi phát/thu). Ô đứng yên không vẽ lại.
class ClipCell extends ConsumerWidget {
  const ClipCell({super.key, required this.track, required this.slot});

  final int track;
  final int slot;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final ticker = ref.watch(engineStateTickerProvider);
    final info = ref.watch(projectControllerProvider.select((s) => ClipCellInfo.of(s, track, slot)));
    final cell = CellRef(track, slot);
    final ui = ref.watch(
      sessionUiProvider.select(
        (u) => (edit: u.mode == SessionMode.edit, selected: u.selected == cell, dropTarget: u.dragTarget == cell),
      ),
    );
    final content = RepaintBoundary(
      child: ValueListenableBuilder<ClipState>(
        valueListenable: ticker.clip(track, slot),
        builder: (context, state, _) {
          // Model có clip nhưng engine chưa nạp (ví dụ engine P0) → coi như Stopped.
          final effective = state == ClipState.empty && info.hasClip ? ClipState.stopped : state;
          return CustomPaint(
            painter: ClipCellPainter(
              state: effective,
              color: info.color,
              armed: info.armed,
              missing: info.missing,
              ticker: ClipCellPainter.animates(effective) ? ticker : null,
              track: track,
              slot: slot,
              selected: ui.selected,
              dropTarget: ui.dropTarget,
            ),
            child: info.name.isEmpty
                ? const SizedBox.expand()
                : Padding(
                    padding: const EdgeInsets.fromLTRB(8, 6, 8, 10),
                    child: Align(
                      alignment: Alignment.topLeft,
                      child: Text(
                        info.name,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(fontSize: 13, color: AppColors.textPrimary),
                      ),
                    ),
                  ),
          );
        },
      ),
    );
    if (!ui.edit) {
      // Perform: pointer-down → lệnh ngay; nhấn giữ không làm gì thêm (07 §3.1).
      return Listener(behavior: HitTestBehavior.opaque, onPointerDown: (_) => _onPointerDown(ref), child: content);
    }
    // Edit: chạm để chọn (mở panel Clip), nhấn giữ mở menu.
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: () => ref.read(sessionUiProvider.notifier).select(cell),
      onLongPressStart: (d) {
        ref.read(sessionUiProvider.notifier).select(cell);
        showClipMenu(context, ref, cell, d.globalPosition);
      },
      // Kéo clip sang ô khác (P2-21): thả xuống → hỏi Di chuyển / Copy.
      onPanUpdate: info.hasClip
          ? (d) => ref.read(sessionUiProvider.notifier).dragOver(cellAtGlobal(d.globalPosition), d.globalPosition)
          : null,
      onPanEnd: info.hasClip
          ? (_) {
              final ui = ref.read(sessionUiProvider.notifier);
              final target = ui.endDrag();
              if (target != null && target != cell) showDropMenu(context, ref, cell, target, ui.lastDragPosition);
            }
          : null,
      child: content,
    );
  }

  void _onPointerDown(WidgetRef ref) {
    // Đọc model tại thời điểm chạm (không dùng giá trị cũ lúc build).
    final session = ref.read(projectControllerProvider);
    final info = ClipCellInfo.of(session, track, slot);
    final actions = ref.read(performanceActionsProvider);
    final engineState = ref.read(engineStateTickerProvider).clip(track, slot).value;
    final free = actions.defaultRecordBars == 0 || (session?.waitingFirstLoop ?? false);
    if (!info.hasClip && (engineState == ClipState.queuedRecord || (engineState == ClipState.recording && free))) {
      actions.recordStop(track); // 07 §3.1b: chạm lần hai chốt bản thu tự do (hoặc huỷ thu đang chờ)
    } else if (info.hasClip) {
      actions.launchClip(track, slot);
    } else if (info.armed) {
      actions.recordClip(track, slot);
    } else {
      actions.stopTrack(track);
    }
  }
}

/// Vẽ 1 ô theo 8 trạng thái ở 07 §2 (+ thiếu file). Paint tạo sẵn (07 §6.5), không Path mới trong paint().
class ClipCellPainter extends CustomPainter {
  ClipCellPainter({
    required this.state,
    required this.color,
    required this.armed,
    required this.missing,
    required this.ticker,
    required this.track,
    required this.slot,
    this.selected = false,
    this.dropTarget = false,
  }) : super(repaint: ticker);

  final ClipState state;
  final Color color;
  final bool armed;
  final bool missing;

  /// Chỉ khác null khi ô cần vẽ lại theo frame.
  final EngineStateTicker? ticker;
  final int track;
  final int slot;

  /// Ô đang chọn ở chế độ Edit → viền trắng.
  final bool selected;

  /// Đang kéo clip đè lên ô này → viền vàng.
  final bool dropTarget;

  static bool animates(ClipState s) => switch (s) {
    ClipState.queuedPlay ||
    ClipState.queuedStop ||
    ClipState.queuedRecord ||
    ClipState.playing ||
    ClipState.recording ||
    ClipState.overdubbing => true,
    _ => false,
  };

  /// Nhấp nháy bám theo phách (07 §2): sáng ở nửa đầu mỗi beat. Transport dừng → beat đứng yên → không nháy.
  static bool blinkOn(double beat) => (beat % 1.0) < 0.5;

  static final _base = Paint()..color = AppColors.surface;
  static final _border = Paint()
    ..color = AppColors.border
    ..style = PaintingStyle.stroke
    ..strokeWidth = 1;
  static final _recordBorder = Paint()
    ..color = AppColors.record
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2;
  static final _recordFill = Paint()..color = AppColors.record;
  static final _progress = Paint()..color = const Color(0xE6FFFFFF);
  static final _glyph = Paint()..color = const Color(0xE6FFFFFF);
  static final _stripe = Paint()
    ..color = const Color(0xCCFF3B30)
    ..strokeWidth = 3;
  static final _selectedBorder = Paint()
    ..color = AppColors.textPrimary
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2;
  static final _dropBorder = Paint()
    ..color = AppColors.queued
    ..style = PaintingStyle.stroke
    ..strokeWidth = 3;
  static final _missingFill = Paint()..color = const Color(0xFF3A3D42);
  static final _warn = Paint()
    ..color = AppColors.queued
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2
    ..strokeJoin = StrokeJoin.round;
  static final _armCircle = Paint()
    ..color = AppColors.record
    ..style = PaintingStyle.stroke
    ..strokeWidth = 2;
  final _fill = Paint();

  @override
  void paint(Canvas canvas, Size size) {
    final r = RRect.fromRectAndRadius((Offset.zero & size).deflate(2), const Radius.circular(4));
    final st = ticker?.state;
    final on = st == null || blinkOn(st.beat);
    canvas.drawRRect(r, _base);
    _paintState(canvas, size, r, st, on);
    if (selected) canvas.drawRRect(r.deflate(1), _selectedBorder);
    if (dropTarget) canvas.drawRRect(r.deflate(1.5), _dropBorder);
  }

  void _paintState(Canvas canvas, Size size, RRect r, EngineState? st, bool on) {
    if (missing) {
      canvas.drawRRect(r, _missingFill);
      _drawWarning(canvas, size);
      return;
    }

    switch (state) {
      case ClipState.empty:
        canvas.drawRRect(r, _border);
        if (armed) canvas.drawCircle(size.center(Offset.zero), 8, _armCircle);
      case ClipState.stopped:
        _fillWith(canvas, r, 0.4);
      case ClipState.queuedPlay:
        _fillWith(canvas, r, on ? 1.0 : 0.4);
        _drawPlayGlyph(canvas, size);
      case ClipState.queuedStop:
        _fillWith(canvas, r, on ? 1.0 : 0.4);
        _drawStopGlyph(canvas, size);
      case ClipState.playing:
        _fillWith(canvas, r, 1.0);
        _drawProgress(canvas, r, st);
      case ClipState.queuedRecord:
        if (on) canvas.drawRRect(r.deflate(1), _recordBorder);
      case ClipState.recording:
        canvas.drawRRect(r, _recordFill);
        _drawProgress(canvas, r, st);
      case ClipState.overdubbing:
        _fillWith(canvas, r, 1.0);
        _drawStripes(canvas, r.outerRect);
        _drawProgress(canvas, r, st);
    }
  }

  /// Sọc chéo đỏ (overdub). Tự cắt đoạn thẳng theo khung thay vì clipRRect (07 §6.4).
  void _drawStripes(Canvas canvas, Rect box) {
    final h = box.height;
    for (var x = -h; x < box.width; x += 12) {
      // Điểm trên đường: (x + k·h, h − k·h), k ∈ [0,1]; giữ phần có 0 ≤ X ≤ width.
      final k0 = x < 0 ? -x / h : 0.0;
      final k1 = (box.width - x) / h < 1 ? (box.width - x) / h : 1.0;
      if (k1 <= k0) continue;
      canvas.drawLine(
        box.topLeft + Offset(x + k0 * h, h - k0 * h),
        box.topLeft + Offset(x + k1 * h, h - k1 * h),
        _stripe,
      );
    }
  }

  void _fillWith(Canvas canvas, RRect r, double alpha) {
    _fill.color = color.withValues(alpha: alpha);
    canvas.drawRRect(r, _fill);
  }

  void _drawProgress(Canvas canvas, RRect r, EngineState? st) {
    if (st == null || st.trackPlayingSlot[track] != slot) return;
    final p = st.trackClipProgress[track].clamp(0.0, 1.0);
    canvas.drawRect(Rect.fromLTWH(r.left, r.bottom - 4, r.width * p, 4), _progress);
  }

  void _drawPlayGlyph(Canvas canvas, Size size) {
    // Tam giác nhỏ góc phải dưới, vẽ bằng 3 đường (không tạo Path).
    final o = Offset(size.width - 16, size.height - 16);
    for (var i = 0; i < 8; i++) {
      canvas.drawLine(o + Offset(i * 0.9, i * 0.5), o + Offset(i * 0.9, 8 - i * 0.5), _glyph);
    }
  }

  void _drawStopGlyph(Canvas canvas, Size size) {
    canvas.drawRect(Rect.fromLTWH(size.width - 16, size.height - 16, 8, 8), _glyph);
  }

  void _drawWarning(Canvas canvas, Size size) {
    final c = size.center(Offset.zero);
    final a = c + const Offset(0, -9), b = c + const Offset(-9, 7), d = c + const Offset(9, 7);
    canvas
      ..drawLine(a, b, _warn)
      ..drawLine(b, d, _warn)
      ..drawLine(d, a, _warn)
      ..drawLine(c + const Offset(0, -3), c + const Offset(0, 2), _warn);
  }

  @override
  bool shouldRepaint(ClipCellPainter old) =>
      old.state != state ||
      old.color != color ||
      old.armed != armed ||
      old.missing != missing ||
      old.ticker != ticker ||
      old.track != track ||
      old.slot != slot ||
      old.selected != selected ||
      old.dropTarget != dropTarget;
}
