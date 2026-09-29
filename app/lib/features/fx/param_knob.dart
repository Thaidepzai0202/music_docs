import 'package:flutter/material.dart';

import '../../app/theme.dart';
import '../../ui_kit/knob.dart';
import 'fx_specs.dart';

/// Knob theo [ParamSpec]: nhãn trên, giá trị dưới.
///
/// Đang kéo: [onPreview] ≤ 1 lần/frame (Knob gom theo frame) — chỉ gửi lệnh engine, không đổi model.
/// Thả tay / chạm đúp: [onCommit] với giá trị cuối (gửi lệnh + ghi model → autosave).
/// Chữ giá trị đổi theo từng frame trong boundary riêng (ô kích thước cố định); card chứa knob không rebuild
/// hay vẽ lại khi kéo.
class ParamKnob extends StatefulWidget {
  const ParamKnob({
    super.key,
    required this.spec,
    required this.value,
    required this.onPreview,
    required this.onCommit,
    this.color = AppColors.queued,
    this.size = 40,
  });

  final ParamSpec spec;
  final double value;
  final ValueChanged<double> onPreview;
  final ValueChanged<double> onCommit;
  final Color color;
  final double size;

  @override
  State<ParamKnob> createState() => _ParamKnobState();
}

class _ParamKnobState extends State<ParamKnob> {
  late final ValueNotifier<double> _shown = ValueNotifier(widget.value);

  @override
  void didUpdateWidget(ParamKnob old) {
    super.didUpdateWidget(old);
    if (old.value != widget.value) _shown.value = widget.value;
  }

  @override
  void dispose() {
    _shown.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final spec = widget.spec;
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(spec.label, maxLines: 1, overflow: TextOverflow.ellipsis, style: _label),
        const SizedBox(height: 2),
        Knob(
          value: spec.normalize(widget.value),
          defaultValue: spec.normalize(spec.defaultValue),
          color: widget.color,
          size: widget.size,
          onChanged: (n) {
            final v = spec.denormalize(n);
            widget.onPreview(v); // lệnh trước…
            _shown.value = v; // …rồi mới vẽ chữ
          },
          onChangeEnd: (n) => widget.onCommit(spec.denormalize(n)),
        ),
        const SizedBox(height: 2),
        // Ô cố định (ràng buộc chặt) → chữ đổi độ dài không kéo layout/paint của card.
        SizedBox(
          width: widget.size + 14,
          height: 15,
          child: RepaintBoundary(
            child: ValueListenableBuilder<double>(
              valueListenable: _shown,
              builder: (_, v, _) => Text(
                spec.format(v),
                maxLines: 1,
                overflow: TextOverflow.ellipsis,
                textAlign: TextAlign.center,
                style: _value,
              ),
            ),
          ),
        ),
      ],
    );
  }

  static const _label = TextStyle(fontSize: 11, color: AppColors.textSecondary);
  static const _value = TextStyle(fontSize: 11, fontFeatures: [FontFeature.tabularFigures()]);
}
