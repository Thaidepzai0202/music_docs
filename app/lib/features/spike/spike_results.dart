/// Tóm tắt kết quả job spike cho người đo trên iPad (ghi vào 08 §8 / spike-report).
/// Hàm thuần để test được; JSON đầy đủ vẫn in bên dưới.
library;

import '../../l10n/l10n.dart';

String _n(Object? v, [int digits = 1]) => v is num ? v.toStringAsFixed(digits) : '?';

/// `spike.latencyLoopback` → {ok, measuredSamples, measuredMs, reportedSamples, reportedMs, runs, score,
/// validRuns, spreadSamples, spreadMs, inputPeak, sampleRate}.
String formatLatencyResult(Map<String, dynamic> r) {
  if (r['ok'] != true) {
    return S.spikeKhongDoDuocMicKhong(_n(r['inputPeak'], 3));
  }
  final runs = (r['runs'] as List?)?.length ?? 5;
  final spread = r['spreadMs'];
  final ok = spread is num && spread <= 1.0;
  return S.spikeRoundTripDoDuocMs(
    _n(r['measuredMs'], 2),
    r['measuredSamples'] ?? '?',
    _n(r['reportedMs'], 2),
    r['reportedSamples'] ?? '?',
    _n(spread, 2),
    ok ? '✓ (≤ 1 ms)' : S.spike1MsDoLai,
    r['validRuns'] ?? '?',
    runs,
  );
}

/// `spike.stretchBench` → {msTotal, msPerZone, semitones, files, msSetup, msWrite, formant, …}.
String formatStretchResult(Map<String, dynamic> r) {
  final total = r['msTotal'];
  final semis = (r['semitones'] as List?) ?? const [];
  final perZone = (r['msPerZone'] as List?) ?? const [];
  final files = (r['files'] as List?) ?? const [];
  final pass = total is num && total < 3000;
  final zones = [
    for (var i = 0; i < perZone.length; i++) '${i < semis.length ? _signed(semis[i]) : '#$i'}: ${_n(perZone[i], 0)}',
  ].join(' · ');
  return S.spikeTongZoneMsFormantNsetup(
    perZone.length,
    _n(total, 0),
    pass ? '✓ (< 3 s)' : '✗ (≥ 3 s)',
    r['formant'] == true ? S.spikeBat : S.spikeTat,
    _n(r['msSetup'], 0),
    _n(r['msWrite'], 0),
    zones,
    files.length,
  );
}

String _signed(Object? s) => s is num && s > 0 ? '+$s' : '$s';
