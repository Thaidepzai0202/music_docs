import 'package:flutter_test/flutter_test.dart';
import 'package:music_looper/features/spike/spike_results.dart';

void main() {
  test('latency ok: ms đo/báo, độ lệch ≤ 1 ms, số lần hợp lệ', () {
    final s = formatLatencyResult({
      'ok': true,
      'measuredSamples': 492,
      'measuredMs': 10.25,
      'reportedSamples': 470,
      'reportedMs': 9.79,
      'runs': [1, 2, 3, 4, 5],
      'validRuns': 5,
      'spreadMs': 0.04,
    });
    expect(s, contains('Round-trip đo được: 10.25 ms (492 smp)'));
    expect(s, contains('iOS báo: 9.79 ms (470 smp)'));
    expect(s, contains('0.04 ms ✓'));
    expect(s, contains('hợp lệ 5/5'));
  });

  test('latency lệch > 1 ms → nhắc đo lại', () {
    expect(formatLatencyResult({'ok': true, 'spreadMs': 1.7, 'validRuns': 3}), contains('✗ (> 1 ms, đo lại)'));
  });

  test('latency ok:false → gợi ý mic không nghe thấy, kèm inputPeak', () {
    final s = formatLatencyResult({'ok': false, 'inputPeak': 0.0021});
    expect(s, contains('inputPeak = 0.002'));
    expect(s, contains('bỏ tai nghe'));
  });

  test('stretch: tổng, đạt/không đạt < 3 s, ms từng zone có dấu', () {
    final s = formatStretchResult({
      'msTotal': 1850,
      'msPerZone': [140, 142, 151],
      'semitones': [-12, 0, 12],
      'files': ['a.wav', 'b.wav', 'c.wav'],
      'msSetup': 12,
      'msWrite': 40,
      'formant': true,
    });
    expect(s, contains('Tổng 3 zone: 1850 ms ✓ (< 3 s)'));
    expect(s, contains('-12: 140 · 0: 142 · +12: 151'));
    expect(s, contains('3 file WAV'));
    expect(formatStretchResult({'msTotal': 3200, 'msPerZone': []}), contains('✗ (≥ 3 s)'));
  });
}
