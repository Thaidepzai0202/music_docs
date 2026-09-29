import 'dart:math' as math;

import 'package:flutter/widgets.dart';

import '../../app/theme.dart';
import '../../engine/engine_state_ticker.dart';

/// Bảng số đo live 60fps (P0-05): SR, buffer, CPU trung bình/đỉnh, xrun, voice, latency,
/// meter input + master + CPU.
///
/// Repaint: `CustomPainter(repaint: ticker)` trong `RepaintBoundary` riêng → mỗi frame chỉ vẽ lại
/// đúng vùng này, không rebuild widget, không setState (07 §6.1).
class LivePanel extends StatefulWidget {
  const LivePanel({super.key, required this.ticker, this.height = 236});

  final EngineStateTicker ticker;
  final double height;

  @override
  State<LivePanel> createState() => _LivePanelState();
}

class _LivePanelState extends State<LivePanel> {
  // Giữ painter qua các lần rebuild để cache TextPainter/Paint không bị tạo lại.
  late LivePanelPainter _painter = LivePanelPainter(widget.ticker);

  @override
  void didUpdateWidget(LivePanel old) {
    super.didUpdateWidget(old);
    if (old.ticker != widget.ticker) _painter = LivePanelPainter(widget.ticker);
  }

  @override
  Widget build(BuildContext context) {
    return RepaintBoundary(
      child: SizedBox(
        height: widget.height,
        width: double.infinity,
        child: CustomPaint(painter: _painter),
      ),
    );
  }
}

class LivePanelPainter extends CustomPainter {
  LivePanelPainter(this.ticker) : super(repaint: ticker);

  final EngineStateTicker ticker;

  // Tạo sẵn 1 lần (07 §6.5: không tạo Paint/Path trong paint()).
  final _bg = Paint()..color = AppColors.surface;
  final _slot = Paint()..color = AppColors.border;
  final _green = Paint()..color = AppColors.play;
  final _yellow = Paint()..color = AppColors.queued;
  final _red = Paint()..color = AppColors.record;
  final _peakTick = Paint()
    ..color = AppColors.textPrimary
    ..strokeWidth = 2;

  static const _valueStyle = TextStyle(color: AppColors.textPrimary, fontSize: 15, fontFeatures: AppText.tabular);
  static const _labelStyle = TextStyle(color: AppColors.textSecondary, fontSize: 12);

  final _sr = _CachedText();
  final _buf = _CachedText();
  final _cpu = _CachedText();
  final _xrun = _CachedText();
  final _voices = _CachedText();
  final _lat = _CachedText();
  final _meterNames = [
    for (final n in const ['IN', 'OUT L', 'OUT R', 'CPU']) _CachedText()..update(n.hashCode, () => n, _labelStyle),
  ];

  static const _pad = 12.0;
  static const _rowH = 26.0;
  static const _meterH = 14.0;

  @override
  void paint(Canvas canvas, Size size) {
    final s = ticker.state;
    canvas.drawRect(Offset.zero & size, _bg);

    // Cột số: chỉ format lại chuỗi + layout khi giá trị hiển thị đổi.
    _sr.update(s.sampleRate.round(), () => 'SR  ${s.sampleRate.round()} Hz', _valueStyle);
    _buf.update(s.bufferSize, () => 'Buffer  ${s.bufferSize} (${s.bufferMs.toStringAsFixed(1)} ms)', _valueStyle);
    final cpuAvg = (s.cpuLoad * 100).round();
    final cpuPeak = (s.cpuPeak * 100).round();
    _cpu.update(cpuAvg * 1000 + cpuPeak, () => 'CPU  $cpuAvg% · đỉnh $cpuPeak%', _valueStyle);
    _xrun.update(s.xrunCount, () => 'Xrun  ${s.xrunCount}', _valueStyle);
    _voices.update(s.activeVoices, () => 'Voice  ${s.activeVoices}', _valueStyle);
    _lat.update(
      s.latencyRoundTripSamples * 100000 + s.sampleRate.round(),
      () => 'Latency  ${s.latencyRoundTripSamples} smp (${s.latencyMs.toStringAsFixed(1)} ms)',
      _valueStyle,
    );
    final rows = [_sr, _buf, _cpu, _xrun, _voices, _lat];
    for (var i = 0; i < rows.length; i++) {
      rows[i].tp.paint(canvas, Offset(_pad, _pad + i * _rowH));
    }

    // Meter bên phải.
    final left = size.width * 0.52;
    final labelW = 44.0;
    final barX = left + labelW;
    final barW = size.width - barX - _pad;
    final levels = [s.inputPeak, s.masterPeak[0], s.masterPeak[1]];
    for (var i = 0; i < 4; i++) {
      final y = _pad + 4 + i * (_meterH + 22);
      _meterNames[i].tp.paint(canvas, Offset(left, y));
      final slot = Rect.fromLTWH(barX, y, barW, _meterH);
      canvas.drawRect(slot, _slot);
      if (i < 3) {
        _paintPeakMeter(canvas, slot, levels[i]);
      } else {
        _paintCpu(canvas, slot, s.cpuLoad, s.cpuPeak);
      }
    }
  }

  /// Peak tuyến tính → thang dB -60..0. Xanh tới -6 dB, vàng tới -1 dB, đỏ trên -1 dB.
  void _paintPeakMeter(Canvas canvas, Rect slot, double peak) {
    final norm = _dbNorm(peak);
    if (norm <= 0) return;
    final x6 = slot.left + slot.width * _dbNorm(0.5012); // -6 dB
    final x1 = slot.left + slot.width * _dbNorm(0.8913); // -1 dB
    final xv = slot.left + slot.width * norm;
    canvas.drawRect(Rect.fromLTRB(slot.left, slot.top, math.min(xv, x6), slot.bottom), _green);
    if (xv > x6) canvas.drawRect(Rect.fromLTRB(x6, slot.top, math.min(xv, x1), slot.bottom), _yellow);
    if (xv > x1) canvas.drawRect(Rect.fromLTRB(x1, slot.top, xv, slot.bottom), _red);
  }

  void _paintCpu(Canvas canvas, Rect slot, double avg, double peak) {
    final a = avg.clamp(0.0, 1.0);
    final paint = a < 0.6 ? _green : (a < 0.8 ? _yellow : _red);
    canvas.drawRect(Rect.fromLTWH(slot.left, slot.top, slot.width * a, slot.height), paint);
    final px = slot.left + slot.width * peak.clamp(0.0, 1.0);
    canvas.drawLine(Offset(px, slot.top - 2), Offset(px, slot.bottom + 2), _peakTick);
  }

  static double _dbNorm(double linear) {
    if (linear <= 0.001) return 0; // < -60 dB
    final db = 20 * math.log(linear) / math.ln10;
    return ((db + 60) / 60).clamp(0.0, 1.0);
  }

  @override
  bool shouldRepaint(LivePanelPainter old) => old.ticker != ticker;
}

/// TextPainter chỉ layout lại khi [key] đổi → không cấp phát chuỗi mỗi frame khi số đứng yên.
class _CachedText {
  final tp = TextPainter(textDirection: TextDirection.ltr, maxLines: 1);
  int? _key;

  void update(int key, String Function() text, TextStyle style) {
    if (key == _key) return;
    _key = key;
    tp
      ..text = TextSpan(text: text(), style: style)
      ..layout();
  }
}
