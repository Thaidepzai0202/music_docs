import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../model/project.dart';
import '../fx/fx_specs.dart';
import '../fx/param_knob.dart';
import '../session/project_controller.dart';
import '../../l10n/l10n.dart';

/// Dải knob ADSR (giây; S 0..1). Sàn khớp `engine/src/dsp/Adsr.h` (attack ≥ 0.5 ms, decay/release ≥ 5 ms).
/// Mặc định = `Envelope()` của 06 §2.
const envelopeSpecs = [
  ParamSpec(0, 'A', min: 0.0005, max: 5, defaultValue: 0.005, log: true, unit: 's'),
  ParamSpec(1, 'D', min: 0.005, max: 5, defaultValue: 0.2, log: true, unit: 's'),
  ParamSpec(2, 'S', min: 0, max: 1, defaultValue: 0.8, unit: '%'),
  ParamSpec(3, 'R', min: 0.005, max: 10, defaultValue: 0.3, log: true, unit: 's'),
];

double envelopeValue(Envelope e, int id) => switch (id) {
  0 => e.a,
  1 => e.d,
  2 => e.s,
  _ => e.r,
};

Envelope withEnvelopeValue(Envelope e, int id, double v) => switch (id) {
  0 => e.copyWith(a: v),
  1 => e.copyWith(d: v),
  2 => e.copyWith(s: v),
  _ => e.copyWith(r: v),
};

/// Khối chỉnh nhạc cụ tự thu (P3-07): Natural/Classic (`instrument.setMode`) + 4 knob ADSR
/// (`instrument.setEnvelope`, lúc kéo ≤ 1 lần/frame; thả tay mới ghi `userInstruments[].envelope`).
class UserInstrumentEditor extends ConsumerWidget {
  const UserInstrumentEditor({super.key, required this.instrumentId, required this.color});

  final String instrumentId;
  final Color color;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final inst = ref.watch(
      projectControllerProvider.select((s) {
        for (final u in s?.project.userInstruments ?? const <UserInstrument>[]) {
          if (u.id == instrumentId) return u;
        }
        return null;
      }),
    );
    if (inst == null) {
      return Center(
        child: Text(S.instrumentKhongTimThayNhacCu, style: TextStyle(fontSize: 12, color: AppColors.textSecondary)),
      );
    }
    final ctl = ref.read(projectControllerProvider.notifier);
    return Container(
      key: const Key('instrument.editor'),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      padding: const EdgeInsets.fromLTRB(8, 6, 8, 6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(inst.name, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(fontSize: 13)),
          const SizedBox(height: 6),
          SegmentedButton<InstrumentMode>(
            key: const Key('instrument.mode'),
            showSelectedIcon: false,
            style: const ButtonStyle(visualDensity: VisualDensity.compact),
            segments: [
              ButtonSegment(value: InstrumentMode.natural, label: Text(S.instrumentMode('natural'))),
              ButtonSegment(value: InstrumentMode.classic, label: Text(S.instrumentMode('classic'))),
            ],
            selected: {inst.mode},
            onSelectionChanged: (s) => ctl.setInstrumentMode(instrumentId, s.first),
          ),
          const Spacer(),
          RepaintBoundary(
            child: Row(
              children: [
                for (final spec in envelopeSpecs)
                  Expanded(
                    child: ParamKnob(
                      key: Key('instrument.env.${spec.name.toLowerCase()}'),
                      spec: spec,
                      value: envelopeValue(inst.envelope, spec.id),
                      color: color,
                      size: 36,
                      // Lúc kéo: gửi envelope với các giá trị khác lấy từ model.
                      onPreview: (v) => ctl.previewEnvelope(instrumentId, withEnvelopeValue(inst.envelope, spec.id, v)),
                      onCommit: (v) => ctl.setEnvelope(instrumentId, withEnvelopeValue(inst.envelope, spec.id, v)),
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
