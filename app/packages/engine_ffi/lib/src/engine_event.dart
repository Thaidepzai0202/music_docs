import 'loopcore_bindings.g.dart';

/// Event bất đồng bộ engine → UI (05 §2 `LeEventType`).
///
/// Engine gọi callback C với tham số theo giá trị; `EngineClient` nhận qua
/// `NativeCallable.listener` rồi map sang các lớp con dưới đây.
sealed class EngineEvent {
  const EngineEvent();

  factory EngineEvent.fromNative(int type, int a, int b, int jobId, double value) => switch (type) {
    LeEventType.LE_EVT_RECORDING_FINISHED => RecordingFinished(track: a, slot: b, frames: value.round()),
    LeEventType.LE_EVT_JOB_PROGRESS => JobProgress(jobId: jobId, progress: value),
    LeEventType.LE_EVT_JOB_DONE => JobDone(jobId: jobId),
    LeEventType.LE_EVT_JOB_FAILED => JobFailed(jobId: jobId, errorCode: a),
    LeEventType.LE_EVT_XRUN => XrunOccurred(totalXruns: a),
    LeEventType.LE_EVT_AUDIO_INTERRUPTED => AudioInterrupted(began: a == 1),
    LeEventType.LE_EVT_ROUTE_CHANGED => RouteChanged(wiredOrInterface: a == 1, bluetooth: b == 1),
    LeEventType.LE_EVT_TEMPO_CHANGED => TempoChanged(bpm: value),
    LeEventType.LE_EVT_LINK_PEERS => LinkPeersChanged(peers: a),
    LeEventType.LE_EVT_MIDI_DEVICES => const MidiDevicesChanged(),
    LeEventType.LE_EVT_MIDI_LEARNED => MidiLearned(isCc: a == 1, number: b),
    LeEventType.LE_EVT_ERROR => EngineErrorEvent(errorCode: a, track: b, slot: value.round()),
    LeEventType.LE_EVT_MEMORY_WARNING => MemoryWarning(megabytes: value, critical: a == 1),
    _ => UnknownEngineEvent(type: type, a: a, b: b, jobId: jobId, value: value),
  };
}

final class RecordingFinished extends EngineEvent {
  const RecordingFinished({required this.track, required this.slot, this.frames = 0});

  /// -1 ở spike P0 (SPIKE_RECORD không gắn với ô nào).
  final int track;
  final int slot;

  /// Số frame đã thu (`value`). Engine P0 điền giá trị này cho SPIKE_RECORD.
  final int frames;
  @override
  String toString() => 'RecordingFinished(track: $track, slot: $slot, frames: $frames)';
}

final class JobProgress extends EngineEvent {
  const JobProgress({required this.jobId, required this.progress});
  final int jobId;

  /// 0..1
  final double progress;
  @override
  String toString() => 'JobProgress(jobId: $jobId, progress: $progress)';
}

final class JobDone extends EngineEvent {
  const JobDone({required this.jobId});
  final int jobId;
  @override
  String toString() => 'JobDone(jobId: $jobId)';
}

final class JobFailed extends EngineEvent {
  const JobFailed({required this.jobId, required this.errorCode});
  final int jobId;

  /// `LeError`
  final int errorCode;
  @override
  String toString() => 'JobFailed(jobId: $jobId, errorCode: $errorCode)';
}

final class XrunOccurred extends EngineEvent {
  const XrunOccurred({required this.totalXruns});
  final int totalXruns;
  @override
  String toString() => 'XrunOccurred(totalXruns: $totalXruns)';
}

final class AudioInterrupted extends EngineEvent {
  const AudioInterrupted({required this.began});

  /// true = bắt đầu bị ngắt (Siri, cuộc gọi…), false = hết ngắt.
  final bool began;
  @override
  String toString() => 'AudioInterrupted(began: $began)';
}

final class RouteChanged extends EngineEvent {
  const RouteChanged({required this.wiredOrInterface, required this.bluetooth});
  final bool wiredOrInterface;
  final bool bluetooth;
  @override
  String toString() => 'RouteChanged(wiredOrInterface: $wiredOrInterface, bluetooth: $bluetooth)';
}

final class TempoChanged extends EngineEvent {
  const TempoChanged({required this.bpm});
  final double bpm;
  @override
  String toString() => 'TempoChanged(bpm: $bpm)';
}

final class LinkPeersChanged extends EngineEvent {
  const LinkPeersChanged({required this.peers});
  final int peers;
  @override
  String toString() => 'LinkPeersChanged(peers: $peers)';
}

final class MidiDevicesChanged extends EngineEvent {
  const MidiDevicesChanged();
  @override
  String toString() => 'MidiDevicesChanged()';
}

final class MidiLearned extends EngineEvent {
  const MidiLearned({required this.isCc, required this.number});

  /// false = note, true = CC
  final bool isCc;
  final int number;
  @override
  String toString() => 'MidiLearned(isCc: $isCc, number: $number)';
}

final class EngineErrorEvent extends EngineEvent {
  const EngineErrorEvent({required this.errorCode, this.track = -1, this.slot = -1});

  /// `LeError`
  final int errorCode;

  /// Ô liên quan (vd `OVERDUB_UNSUPPORTED`: b = track, value = slot); −1 nếu lỗi không gắn với ô nào.
  final int track;
  final int slot;
  @override
  String toString() => 'EngineErrorEvent(errorCode: $errorCode, track: $track, slot: $slot)';
}

final class MemoryWarning extends EngineEvent {
  const MemoryWarning({required this.megabytes, this.critical = false});

  /// Bộ nhớ engine đang dùng sau khi đã giải phóng (MB).
  final double megabytes;

  /// a = 1: mức critical (iOS sắp đóng app); a = 0: warning.
  final bool critical;
  @override
  String toString() => 'MemoryWarning(megabytes: $megabytes, critical: $critical)';
}

/// Loại event chưa biết (header mới hơn wrapper). Giữ nguyên số liệu thô.
final class UnknownEngineEvent extends EngineEvent {
  const UnknownEngineEvent({
    required this.type,
    required this.a,
    required this.b,
    required this.jobId,
    required this.value,
  });
  final int type;
  final int a;
  final int b;
  final int jobId;
  final double value;
  @override
  String toString() => 'UnknownEngineEvent(type: $type, a: $a, b: $b, jobId: $jobId, value: $value)';
}
