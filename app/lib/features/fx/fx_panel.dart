import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../model/project.dart';
import '../session/project_controller.dart';
import '../session/session_layout.dart';
import '../session/session_ui.dart';
import '../session/widgets/track_picker.dart';
import 'fx_specs.dart';
import 'param_knob.dart';
import '../../l10n/l10n.dart';

/// Tab FX (P3-16): 3 slot của track đang chọn + chuỗi master cố định (EQ3 + trần limiter).
///
/// - Thêm/đổi loại → `fx.set` (tham số mặc định 04 §9), xoá → `fx.remove` (slot sau dồn lên).
/// - Knob: `LE_CMD_FX_PARAM` ≤ 1 lần/frame lúc kéo, ghi model khi thả tay. Bypass: `LE_CMD_FX_BYPASS`.
/// Mỗi card là một RepaintBoundary: kéo knob ở slot này không vẽ lại slot khác.
class FxPanel extends ConsumerWidget {
  const FxPanel({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final track = ref.watch(sessionUiProvider.select((u) => u.selectedTrack));
    return Padding(
      padding: const EdgeInsets.fromLTRB(12, 6, 12, 8),
      child: Column(
        children: [
          SizedBox(
            height: 36,
            child: Row(
              children: [
                Flexible(
                  child: TrackPicker(key: const Key('fx.track'), track: track),
                ),
                const SizedBox(width: 12),
                Flexible(
                  child: Text(
                    S.fxKeoDocDeChinhCham,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(fontSize: 12, color: AppColors.textSecondary),
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: 6),
          Expanded(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                for (var i = 0; i < fxSlotsPerTrack; i++) ...[
                  Expanded(
                    child: RepaintBoundary(
                      child: FxSlotCard(key: ValueKey('fx.slot.$track.$i'), track: track, index: i),
                    ),
                  ),
                  const SizedBox(width: 8),
                ],
                const SizedBox(width: 220, child: RepaintBoundary(child: MasterEqCard())),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// Một slot FX: trống (nút thêm ở slot trống đầu tiên) hoặc FX với header (loại ▾, bypass, xoá) + tham số.
class FxSlotCard extends ConsumerWidget {
  const FxSlotCard({super.key, required this.track, required this.index});

  final int track;
  final int index;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final fx = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(track)?.fx)) ?? const [];
    final hex = ref.watch(projectControllerProvider.select((s) => s?.project.trackAt(track)?.color));
    final color = hex == null ? AppColors.tracks[track % 8] : parseHexColor(hex);
    final slot = index < fx.length ? fx[index] : null;
    final ctl = ref.read(projectControllerProvider.notifier);

    return Container(
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      padding: const EdgeInsets.fromLTRB(8, 4, 4, 6),
      child: slot == null
          ? _EmptySlot(index: index, canAdd: index == fx.length, onAdd: (t) => ctl.setFx(track, index, t))
          : Column(
              children: [
                SizedBox(
                  height: 36,
                  child: Row(
                    children: [
                      Text('${index + 1}', style: const TextStyle(fontSize: 12, color: AppColors.textSecondary)),
                      const SizedBox(width: 6),
                      Flexible(
                        child: _TypeMenu(
                          key: Key('fx.type.$index'),
                          current: slot.type,
                          onSelected: (t) {
                            if (t != slot.type) ctl.setFx(track, index, t);
                          },
                        ),
                      ),
                      const Spacer(),
                      _BypassButton(
                        key: Key('fx.bypass.$index'),
                        active: !slot.bypass,
                        color: color,
                        onTap: () => ctl.setFxBypass(track, index, !slot.bypass),
                      ),
                      IconButton(
                        key: Key('fx.remove.$index'),
                        tooltip: S.fxXoaFx,
                        visualDensity: VisualDensity.compact,
                        icon: const Icon(Icons.close, size: 18),
                        onPressed: () => ctl.removeFx(track, index),
                      ),
                    ],
                  ),
                ),
                Expanded(
                  // Đổi loại FX → dựng lại knob (id tham số mang nghĩa khác).
                  child: KeyedSubtree(
                    key: ValueKey(slot.type),
                    child: _Params(
                      index: index,
                      slot: slot,
                      color: slot.bypass ? AppColors.textSecondary : color,
                      onPreview: (id, v) => ctl.previewFxParam(track, index, id, v),
                      onCommit: (id, v) => ctl.setFxParam(track, index, id, v),
                    ),
                  ),
                ),
              ],
            ),
    );
  }
}

class _EmptySlot extends StatelessWidget {
  const _EmptySlot({required this.index, required this.canAdd, required this.onAdd});

  final int index;
  final bool canAdd;
  final ValueChanged<FxType> onAdd;

  @override
  Widget build(BuildContext context) {
    if (!canAdd) {
      return Center(
        child: Text(S.fxSlotTrong(index + 1), style: const TextStyle(fontSize: 12, color: AppColors.textSecondary)),
      );
    }
    return Center(
      child: PopupMenuButton<FxType>(
        key: Key('fx.add.$index'),
        tooltip: S.fxThemFx,
        onSelected: onAdd,
        itemBuilder: (_) => [
          for (final t in FxType.values)
            PopupMenuItem(key: Key('fx.pick.${t.name}'), value: t, child: Text(fxTypeLabel(t))),
        ],
        child: Padding(
          padding: EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [Icon(Icons.add, size: 18), SizedBox(width: 4), Text(S.fxThemFx)],
          ),
        ),
      ),
    );
  }
}

class _TypeMenu extends StatelessWidget {
  const _TypeMenu({super.key, required this.current, required this.onSelected});

  final FxType current;
  final ValueChanged<FxType> onSelected;

  @override
  Widget build(BuildContext context) {
    return PopupMenuButton<FxType>(
      tooltip: S.fxDoiLoaiFx,
      onSelected: onSelected,
      itemBuilder: (_) => [
        for (final t in FxType.values)
          PopupMenuItem(key: Key('fx.pick.${t.name}'), value: t, child: Text(fxTypeLabel(t))),
      ],
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Flexible(
            child: Text(
              fxTypeLabel(current),
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontWeight: FontWeight.w600),
            ),
          ),
          const Icon(Icons.arrow_drop_down, size: 20),
        ],
      ),
    );
  }
}

/// Nút bật/tắt FX: sáng = FX đang chạy, tối = bypass.
class _BypassButton extends StatelessWidget {
  const _BypassButton({super.key, required this.active, required this.color, required this.onTap});

  final bool active;
  final Color color;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return IconButton(
      tooltip: active ? S.fxBypass : S.fxBatLai,
      visualDensity: VisualDensity.compact,
      onPressed: onTap,
      icon: Icon(Icons.power_settings_new, size: 20, color: active ? color : AppColors.textSecondary),
    );
  }
}

/// Hàng điều khiển tham số theo bảng 04 §9: knob, chọn (LP/HP/BP, nhịp delay), bật/tắt (ping-pong).
class _Params extends StatelessWidget {
  const _Params({
    required this.index,
    required this.slot,
    required this.color,
    required this.onPreview,
    required this.onCommit,
  });

  final int index;
  final FxSlot slot;
  final Color color;
  final void Function(int id, double v) onPreview;
  final void Function(int id, double v) onCommit;

  @override
  Widget build(BuildContext context) {
    final specs = fxSpecs[slot.type]!;
    return Row(
      children: [
        for (final spec in specs)
          Expanded(
            child: Center(
              child: switch (spec.control) {
                ParamControl.knob => ParamKnob(
                  key: Key('fx.param.$index.${spec.id}'),
                  spec: spec,
                  value: fxParam(slot, spec),
                  color: color,
                  size: 38,
                  onPreview: (v) => onPreview(spec.id, v),
                  onCommit: (v) => onCommit(spec.id, v),
                ),
                ParamControl.choice => _Choice(
                  key: Key('fx.param.$index.${spec.id}'),
                  spec: spec,
                  value: fxParam(slot, spec).round(),
                  color: color,
                  onSelected: (i) => onCommit(spec.id, i.toDouble()),
                ),
                ParamControl.toggle => _Toggle(
                  key: Key('fx.param.$index.${spec.id}'),
                  spec: spec,
                  on: fxParam(slot, spec) >= 0.5,
                  color: color,
                  onChanged: (on) => onCommit(spec.id, on ? 1 : 0),
                ),
              },
            ),
          ),
      ],
    );
  }
}

/// Chọn một trong danh sách (kiểu filter, nhịp delay 1/16…1/2 có chấm/triplet).
class _Choice extends StatelessWidget {
  const _Choice({super.key, required this.spec, required this.value, required this.color, required this.onSelected});

  final ParamSpec spec;
  final int value;
  final Color color;
  final ValueChanged<int> onSelected;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(spec.label, maxLines: 1, overflow: TextOverflow.ellipsis, style: _label),
        const SizedBox(height: 4),
        PopupMenuButton<int>(
          tooltip: spec.label,
          onSelected: onSelected,
          itemBuilder: (_) => [
            for (var i = 0; i < spec.choices.length; i++)
              PopupMenuItem(key: Key('fx.choice.$i'), value: i, child: Text(spec.choices[i])),
          ],
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
            decoration: BoxDecoration(
              border: Border.all(color: color),
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(spec.format(value.toDouble()), maxLines: 1, style: const TextStyle(fontSize: 13)),
          ),
        ),
      ],
    );
  }
}

class _Toggle extends StatelessWidget {
  const _Toggle({super.key, required this.spec, required this.on, required this.color, required this.onChanged});

  final ParamSpec spec;
  final bool on;
  final Color color;
  final ValueChanged<bool> onChanged;

  @override
  Widget build(BuildContext context) {
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(spec.label, maxLines: 1, overflow: TextOverflow.ellipsis, style: _label),
        const SizedBox(height: 4),
        GestureDetector(
          onTap: () => onChanged(!on),
          child: Container(
            width: 44,
            height: 30,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: on ? color : Colors.transparent,
              border: Border.all(color: color),
              borderRadius: BorderRadius.circular(6),
            ),
            child: Text(
              on ? S.fxToggleOn : S.fxToggleOff,
              style: TextStyle(fontSize: 12, color: on ? AppColors.background : AppColors.textPrimary),
            ),
          ),
        ),
      ],
    );
  }
}

/// Chuỗi master cố định (04 §9/§11): EQ3 (`master.eq3`) + trần limiter (`master.limiterCeilingDb`).
class MasterEqCard extends ConsumerWidget {
  const MasterEqCard({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final master = ref.watch(projectControllerProvider.select((s) => s?.project.master)) ?? const Master();
    final ctl = ref.read(projectControllerProvider.notifier);
    final specs = fxSpecs[FxType.eq3]!;
    return Container(
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      padding: const EdgeInsets.fromLTRB(6, 4, 6, 6),
      child: Column(
        children: [
          SizedBox(
            height: 36,
            child: Row(
              children: [
                Expanded(
                  child: Text(
                    S.fxMasterCard,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(fontWeight: FontWeight.w600),
                  ),
                ),
                // Bypass chỉ cho EQ3 (limiter luôn bật, 04 §9).
                _BypassButton(
                  key: const Key('fx.master.eqBypass'),
                  active: !master.eq3Bypass,
                  color: AppColors.textPrimary,
                  onTap: () => ctl.setMasterEqBypass(!master.eq3Bypass),
                ),
              ],
            ),
          ),
          Expanded(
            child: Row(
              children: [
                for (final spec in specs)
                  Expanded(
                    child: Center(
                      child: ParamKnob(
                        key: Key('fx.master.${spec.id}'),
                        spec: spec,
                        value: spec.id < master.eq3.length ? master.eq3[spec.id] : 0,
                        color: master.eq3Bypass ? AppColors.textSecondary : AppColors.textPrimary,
                        size: 30,
                        onPreview: (v) => ctl.previewMasterEq(spec.id, v),
                        onCommit: (v) => ctl.setMasterEq(spec.id, v),
                      ),
                    ),
                  ),
                Expanded(
                  child: Center(
                    child: ParamKnob(
                      key: const Key('fx.master.limiter'),
                      spec: limiterCeilingSpec,
                      value: master.limiterCeilingDb,
                      color: AppColors.record,
                      size: 30,
                      onPreview: ctl.previewLimiterCeiling,
                      onCommit: ctl.setLimiterCeiling,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

const _label = TextStyle(fontSize: 11, color: AppColors.textSecondary);
