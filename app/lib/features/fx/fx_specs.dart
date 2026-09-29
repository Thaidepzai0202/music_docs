import 'dart:math' as math;

import '../../model/project.dart';
import '../../l10n/l10n.dart';

/// Kiểu điều khiển của một tham số (FX, envelope) trên UI.
enum ParamControl { knob, choice, toggle }

/// Một tham số có dải + thang (tuyến tính/log) + cách hiển thị. Bảng FX bên dưới khớp `engine/src/dsp/Fx.cpp`
/// (04 §9) — đổi bên đó thì đổi ở đây.
final class ParamSpec {
  const ParamSpec(
    this.id,
    this.name, {
    required this.min,
    required this.max,
    required this.defaultValue,
    this.log = false,
    this.control = ParamControl.knob,
    this.choices = const [],
    this.unit = '',
  });

  final int id;

  /// Token kỹ thuật (vd `cutoff`, `mode`) — chữ hiển thị qua l10n [label] (select `fxParam`).
  final String name;
  String get label => S.fxParam(name);
  final double min;
  final double max;
  final double defaultValue;

  /// Knob theo thang log (cutoff, thời gian, ratio).
  final bool log;
  final ParamControl control;

  /// Nhãn cho [ParamControl.choice]: giá trị = chỉ số trong danh sách.
  final List<String> choices;
  final String unit;

  String get key => '$id';

  double clamp(double v) => v.clamp(min, max).toDouble();

  /// Giá trị thật → 0..1 cho Knob.
  double normalize(double v) {
    final c = clamp(v);
    if (log) return math.log(c / min) / math.log(max / min);
    return (c - min) / (max - min);
  }

  /// 0..1 → giá trị thật.
  double denormalize(double n) {
    final t = n.clamp(0.0, 1.0);
    if (log) return min * math.pow(max / min, t);
    return min + (max - min) * t;
  }

  String format(double v) {
    switch (control) {
      case ParamControl.choice:
        return choices[v.round().clamp(0, choices.length - 1)];
      case ParamControl.toggle:
        return v >= 0.5 ? S.fxBat : S.fxTat;
      case ParamControl.knob:
        break;
    }
    return switch (unit) {
      'Hz' => v >= 1000 ? '${(v / 1000).toStringAsFixed(v >= 10000 ? 0 : 1)} kHz' : '${v.round()} Hz',
      '%' => '${(v * 100).round()}%',
      'dB' => '${v > 0 ? '+' : ''}${v.toStringAsFixed(1)} dB',
      'ms' => v < 10 ? '${v.toStringAsFixed(1)} ms' : '${v.round()} ms',
      's' => v < 1 ? '${(v * 1000).round()} ms' : '${v.toStringAsFixed(v < 10 ? 2 : 1)} s',
      ':1' => '${v.toStringAsFixed(1)}:1',
      _ => v.toStringAsFixed(2),
    };
  }
}

/// Nhịp delay (04 §9) — chỉ số khớp `DelayFx::kNoteBeats`: triplet · thường · chấm cho 1/16, 1/8, 1/4, rồi 1/2T, 1/2.
const delayNoteLabels = ['1/16T', '1/16', '1/16.', '1/8T', '1/8', '1/8.', '1/4T', '1/4', '1/4.', '1/2T', '1/2'];

const Map<FxType, List<ParamSpec>> fxSpecs = {
  FxType.filter: [
    ParamSpec(0, 'mode', min: 0, max: 2, defaultValue: 0, control: ParamControl.choice, choices: ['LP', 'HP', 'BP']),
    ParamSpec(1, 'cutoff', min: 20, max: 20000, defaultValue: 20000, log: true, unit: 'Hz'),
    ParamSpec(2, 'reso', min: 0.5, max: 10, defaultValue: 0.707, log: true),
  ],
  FxType.delay: [
    ParamSpec(0, 'rate', min: 0, max: 10, defaultValue: 4, control: ParamControl.choice, choices: delayNoteLabels),
    ParamSpec(1, 'feedback', min: 0, max: 0.95, defaultValue: 0.35, unit: '%'),
    ParamSpec(2, 'mix', min: 0, max: 1, defaultValue: 0.3, unit: '%'),
    ParamSpec(3, 'pingpong', min: 0, max: 1, defaultValue: 0, control: ParamControl.toggle),
  ],
  FxType.reverb: [
    ParamSpec(0, 'size', min: 0, max: 1, defaultValue: 0.5, unit: '%'),
    ParamSpec(1, 'damping', min: 0, max: 1, defaultValue: 0.5, unit: '%'),
    ParamSpec(2, 'width', min: 0, max: 1, defaultValue: 1, unit: '%'),
    ParamSpec(3, 'mix', min: 0, max: 1, defaultValue: 0.25, unit: '%'),
  ],
  FxType.eq3: [
    ParamSpec(0, 'low', min: -15, max: 15, defaultValue: 0, unit: 'dB'),
    ParamSpec(1, 'mid', min: -15, max: 15, defaultValue: 0, unit: 'dB'),
    ParamSpec(2, 'high', min: -15, max: 15, defaultValue: 0, unit: 'dB'),
  ],
  FxType.comp: [
    ParamSpec(0, 'thresh', min: -60, max: 0, defaultValue: -18, unit: 'dB'),
    ParamSpec(1, 'ratio', min: 1, max: 20, defaultValue: 4, log: true, unit: ':1'),
    ParamSpec(2, 'attack', min: 0.1, max: 100, defaultValue: 10, log: true, unit: 'ms'),
    ParamSpec(3, 'release', min: 10, max: 1000, defaultValue: 100, log: true, unit: 'ms'),
    ParamSpec(4, 'makeup', min: 0, max: 24, defaultValue: 0, unit: 'dB'),
  ],
};

/// Tên hiển thị của loại FX.
String fxTypeLabel(FxType t) => S.fxType(t.name);

/// Tham số mặc định đầy đủ của [type] (lưu hết vào project để mở lại không phụ thuộc mặc định engine).
Map<String, double> fxDefaultParams(FxType type) => {for (final p in fxSpecs[type]!) p.key: p.defaultValue};

/// Giá trị tham số [spec] của [slot]; thiếu trong file → mặc định.
double fxParam(FxSlot slot, ParamSpec spec) => slot.params[spec.key] ?? spec.defaultValue;

/// Số slot FX mỗi track (04 §9).
const fxSlotsPerTrack = 3;

/// Master cố định: gain → EQ3 → Limiter (04 §9, §11), không có slot người dùng. Chỉnh bằng `FX_PARAM track −1`:
/// slot 0 = EQ3 (param 0/1/2 dB), slot 1 = Limiter (param 0 = ceiling dB, 1 = release ms).
const masterEqSlot = 0;
const masterLimiterSlot = 1;

/// Trần limiter master (`master.limiterCeilingDb`, 06 §2): −12..0 dB, mặc định −0.3.
const limiterCeilingSpec = ParamSpec(0, 'ceiling', min: -12, max: 0, defaultValue: -0.3, unit: 'dB');
