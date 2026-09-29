import 'dart:ffi';
import 'dart:typed_data';

import 'package:engine_ffi/engine_ffi.dart';

/// Trạng thái ô clip (`LeClipState`), thứ tự khớp giá trị C 0..7.
enum ClipState {
  empty,
  stopped,
  queuedPlay,
  playing,
  queuedStop,
  queuedRecord,
  recording,
  overdubbing;

  static ClipState fromLe(int v) => v >= 0 && v < values.length ? values[v] : empty;
}

/// Bản sao Dart của `LeState`, **có thể thay đổi** và cập nhật tại chỗ mỗi frame (05 §4):
/// không tạo object mới trên đường 60Hz. Widget đọc qua `EngineStateTicker`, không giữ lâu.
final class EngineState {
  int publishCounter = 0;
  bool playing = false;
  bool anyRecording = false;
  bool linkEnabled = false;
  int linkPeers = 0;
  double beat = 0;
  double bpm = 120;
  double sampleRate = 0;
  int bufferSize = 0;
  int beatsPerBar = 4;
  int quantize = 0;
  int latencyRoundTripSamples = 0;
  double cpuLoad = 0;
  double cpuPeak = 0;
  int xrunCount = 0;
  int activeVoices = 0;
  double inputPeak = 0;

  /// [L, R], tuyến tính 0..1.
  final Float32List masterPeak = Float32List(2);

  /// `[track * 2 + kênh]`.
  final Float32List trackPeak = Float32List(LE_MAX_TRACKS * 2);

  /// -1 nếu track không phát.
  final Int8List trackPlayingSlot = Int8List(LE_MAX_TRACKS)..fillRange(0, LE_MAX_TRACKS, -1);

  /// 0..1 vị trí trong clip đang phát/thu.
  final Float32List trackClipProgress = Float32List(LE_MAX_TRACKS);

  /// Round-trip latency tính bằng ms (0 nếu chưa biết sample rate).
  double get latencyMs => sampleRate > 0 ? latencyRoundTripSamples * 1000 / sampleRate : 0;

  /// Độ dài 1 buffer tính bằng ms.
  double get bufferMs => sampleRate > 0 ? bufferSize * 1000 / sampleRate : 0;

  void copyFrom(LeState s) {
    publishCounter = s.publishCounter;
    playing = s.playing != 0;
    anyRecording = s.anyRecording != 0;
    linkEnabled = s.linkEnabled != 0;
    linkPeers = s.linkPeers;
    beat = s.beat;
    bpm = s.bpm;
    sampleRate = s.sampleRate;
    bufferSize = s.bufferSize;
    beatsPerBar = s.beatsPerBar;
    quantize = s.quantize;
    latencyRoundTripSamples = s.latencyRoundTripSamples;
    cpuLoad = s.cpuLoad;
    cpuPeak = s.cpuPeak;
    xrunCount = s.xrunCount;
    activeVoices = s.activeVoices;
    inputPeak = s.inputPeak;
    masterPeak[0] = s.masterPeak[0];
    masterPeak[1] = s.masterPeak[1];
    for (var t = 0; t < LE_MAX_TRACKS; t++) {
      final tp = s.trackPeak[t];
      trackPeak[t * 2] = tp[0];
      trackPeak[t * 2 + 1] = tp[1];
      trackPlayingSlot[t] = s.trackPlayingSlot[t];
      trackClipProgress[t] = s.trackClipProgress[t];
    }
  }
}
