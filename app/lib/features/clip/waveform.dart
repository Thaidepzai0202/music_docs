import 'dart:math' as math;
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app/theme.dart';
import '../../services/peaks_service.dart';
import '../../services/service_providers.dart';

/// 3 mức peaks của PeakBuilder (04 §5.7): sample mỗi cặp (min, max).
const peakLevelSamples = [256, 2048, 16384];

/// Chọn mức thô nhất vẫn còn ≥ 1 điểm cho mỗi pixel.
int peakLevelFor({required int totalSamples, required double widthPx}) {
  final sppNeeded = totalSamples / math.max(1.0, widthPx);
  for (var l = peakLevelSamples.length - 1; l >= 0; l--) {
    if (peakLevelSamples[l] <= sppNeeded) return l;
  }
  return 0;
}

@immutable
final class WaveformKey {
  const WaveformKey(this.clipId, this.zoom, [this.revision = 0]);
  final String clipId;
  final int zoom;

  /// Tăng sau mỗi lượt overdub audio → picture cũ không còn khớp, lấy peaks mới.
  final int revision;
  @override
  bool operator ==(Object other) =>
      other is WaveformKey && other.clipId == clipId && other.zoom == zoom && other.revision == revision;
  @override
  int get hashCode => Object.hash(clipId, zoom, revision);
}

final class WaveformPicture {
  WaveformPicture(this.picture, this.size, this.totalSamples);
  final ui.Picture picture;
  final Size size;

  /// Ước lượng độ dài file (số cặp × sample/cặp) — dùng để đặt vùng loop lên hình.
  final int totalSamples;
}

/// Cache `ui.Picture` của waveform theo (clipId, zoom) (P2-20, 07 §6.3): vẽ từ peaks MỘT lần,
/// cuộn/zoom lại chỉ dùng picture có sẵn. Giữ tối đa [capacity] hình (LRU).
class WaveformCache {
  WaveformCache(this._peaks, {this.capacity = 24});

  final PeaksService _peaks;
  final int capacity;
  final _items = <WaveformKey, WaveformPicture>{};
  int builds = 0;

  WaveformPicture? get({
    required String clipId,
    required int zoom,
    required Size size,
    required int estimatedSamples,
    int revision = 0,
    Color color = AppColors.textSecondary,
  }) {
    final key = WaveformKey(clipId, zoom, revision);
    final hit = _items.remove(key);
    if (hit != null && hit.size == size) {
      _items[key] = hit; // đưa lên cuối (mới dùng)
      return hit;
    }
    hit?.picture.dispose();
    final level = peakLevelFor(totalSamples: estimatedSamples, widthPx: size.width);
    final spp = peakLevelSamples[level];
    final peaks = _peaks.peaks(clipId, level, (estimatedSamples / spp).ceil() * 2 + 16);
    if (peaks == null || peaks.isEmpty) return null;
    builds++;
    final pic = WaveformPicture(_record(peaks, size, color), size, peaks.length ~/ 2 * spp);
    _items[key] = pic;
    while (_items.length > capacity) {
      _items.remove(_items.keys.first)!.picture.dispose();
    }
    return pic;
  }

  static ui.Picture _record(Float32List peaks, Size size, Color color) {
    final rec = ui.PictureRecorder();
    final canvas = Canvas(rec, Offset.zero & size);
    final paint = Paint()..color = color;
    final n = peaks.length ~/ 2;
    final mid = size.height / 2;
    final cols = size.width.ceil();
    for (var x = 0; x < cols; x++) {
      final a = (x * n / cols).floor();
      final b = math.max(a + 1, ((x + 1) * n / cols).floor());
      var lo = 0.0, hi = 0.0;
      for (var i = a; i < b && i < n; i++) {
        lo = math.min(lo, peaks[i * 2]);
        hi = math.max(hi, peaks[i * 2 + 1]);
      }
      canvas.drawRect(Rect.fromLTRB(x.toDouble(), mid - hi * mid, x + 1.0, mid - lo * mid), paint);
    }
    return rec.endRecording();
  }

  void clear() {
    for (final p in _items.values) {
      p.picture.dispose();
    }
    _items.clear();
  }
}

final waveformCacheProvider = Provider<WaveformCache>((ref) {
  final c = WaveformCache(ref.watch(peaksServiceProvider));
  ref.onDispose(c.clear);
  return c;
});

/// Vẽ lại một picture đã cache — gần như không tốn gì mỗi lần repaint.
class WaveformPicturePainter extends CustomPainter {
  WaveformPicturePainter(this.data);
  final WaveformPicture? data;
  static final _bg = Paint()..color = AppColors.background;
  static final _axis = Paint()
    ..color = AppColors.border
    ..strokeWidth = 1;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(Offset.zero & size, _bg);
    canvas.drawLine(Offset(0, size.height / 2), Offset(size.width, size.height / 2), _axis);
    final d = data;
    if (d != null) canvas.drawPicture(d.picture);
  }

  @override
  bool shouldRepaint(WaveformPicturePainter old) => old.data != data;
}
