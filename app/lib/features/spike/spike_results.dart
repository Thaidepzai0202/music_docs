/// Tóm tắt kết quả job spike cho người đo trên iPad (ghi vào 08 §8 / spike-report).
/// Hàm thuần để test được; JSON đầy đủ vẫn in bên dưới.
library;

String _n(Object? v, [int digits = 1]) => v is num ? v.toStringAsFixed(digits) : '?';

/// `spike.latencyLoopback` → {ok, measuredSamples, measuredMs, reportedSamples, reportedMs, runs, score,
/// validRuns, spreadSamples, spreadMs, inputPeak, sampleRate}.
String formatLatencyResult(Map<String, dynamic> r) {
  if (r['ok'] != true) {
    return 'Không đo được: mic không nghe rõ chirp (inputPeak = ${_n(r['inputPeak'], 3)}).\n'
        'Tăng âm lượng loa, bỏ tai nghe, đo trong phòng yên tĩnh rồi thử lại.';
  }
  final runs = (r['runs'] as List?)?.length ?? 5;
  final spread = r['spreadMs'];
  final ok = spread is num && spread <= 1.0;
  return 'Round-trip đo được: ${_n(r['measuredMs'], 2)} ms (${r['measuredSamples']} smp)\n'
      'iOS báo: ${_n(r['reportedMs'], 2)} ms (${r['reportedSamples']} smp)\n'
      'Lệch giữa các lần: ${_n(spread, 2)} ms ${ok ? '✓ (≤ 1 ms)' : '✗ (> 1 ms, đo lại)'} · '
      'hợp lệ ${r['validRuns']}/$runs';
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
  return 'Tổng ${perZone.length} zone: ${_n(total, 0)} ms ${pass ? '✓ (< 3 s)' : '✗ (≥ 3 s)'} · '
      'formant ${r['formant'] == true ? 'bật' : 'tắt'}\n'
      'Setup ${_n(r['msSetup'], 0)} ms · ghi file ${_n(r['msWrite'], 0)} ms\n'
      'ms/zone  $zones\n'
      '${files.length} file WAV: app Files → Trên iPad → Music Looper → spike';
}

String _signed(Object? s) => s is num && s > 0 ? '+$s' : '$s';
