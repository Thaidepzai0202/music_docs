/// Tap tempo (P2-10): BPM = trung bình khoảng cách giữa tối đa 4 lần chạm gần nhất, kẹp 20–300.
/// Nghỉ quá [resetAfter] giữa hai lần chạm thì bắt đầu đếm lại.
class TapTempo {
  TapTempo({this.resetAfter = const Duration(seconds: 2)});

  final Duration resetAfter;
  final _taps = <Duration>[];

  static const minBpm = 20.0;
  static const maxBpm = 300.0;
  static const window = 4;

  /// [at]: thời điểm chạm (timeStamp của pointer event). Trả BPM khi đã có ≥ 2 lần chạm.
  double? tap(Duration at) {
    if (_taps.isNotEmpty && at - _taps.last > resetAfter) _taps.clear();
    _taps.add(at);
    if (_taps.length > window) _taps.removeAt(0);
    if (_taps.length < 2) return null;
    final spanUs = (_taps.last - _taps.first).inMicroseconds;
    if (spanUs <= 0) return null;
    final intervalUs = spanUs / (_taps.length - 1);
    return (60e6 / intervalUs).clamp(minBpm, maxBpm);
  }

  void reset() => _taps.clear();
}
