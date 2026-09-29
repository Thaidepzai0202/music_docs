import 'dart:async';
import 'dart:ffi';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'engine_api.dart';
import 'engine_event.dart';
import 'loopcore_bindings.g.dart';

/// Engine giả cùng interface với [EngineClient] (05 §4).
///
/// - Test: ghi lại mọi `send`/`call` theo đúng thứ tự vào [log] để so với chuỗi lệnh mong đợi.
/// - Chạy app khi chưa có `LoopCore.xcframework`: bật [simulate] để meter/CPU chuyển động.
///
/// `LeState` vẫn là struct native thật (calloc) nên code đọc state y hệt khi chạy engine thật.
final class FakeEngineClient implements EngineApi {
  FakeEngineClient({this.simulate = false, this.autoCompleteJobs = true, this.jobDelay = Duration.zero});

  /// Tự sinh số liệu meter/CPU mỗi lần [readState] (chỉ để xem UI chạy).
  final bool simulate;

  /// Op trả `jobId` tự hoàn thành sau [jobDelay] (phát `JobDone`/`JobFailed`).
  final bool autoCompleteJobs;
  final Duration jobDelay;

  /// Mọi lệnh đã gửi, theo thứ tự.
  final List<FakeEngineOp> log = [];

  /// Op nào trả `jobId` thay vì kết quả ngay (05 §3).
  final Set<String> jobOps = {
    'spike.latencyLoopback',
    'spike.stretchBench',
    'track.setInstrument',
    'clip.setAudio',
    'instrument.createFromRecording',
    'export.scene',
    'latency.calibrate',
  };

  /// Op nằm ở đây thì job của nó thất bại (`JobFailed`, a = [LeError.LE_ERR_INTERNAL]).
  final Set<String> failingJobOps = {};

  /// Kết quả `job.result` theo op.
  final Map<String, Map<String, dynamic>> jobResults = {
    'spike.latencyLoopback': {
      'ok': true,
      'measuredSamples': 492,
      'measuredMs': 10.25,
      'reportedSamples': 470,
      'reportedMs': 9.79,
      'runs': [492, 491, 493, 492, 492],
      'score': [0.91, 0.9, 0.92, 0.91, 0.9],
      'validRuns': 5,
      'spreadSamples': 2,
      'spreadMs': 0.04,
      'inputPeak': 0.42,
      'sampleRate': 48000,
    },
    'spike.stretchBench': {
      'msTotal': 1850,
      'msPerZone': [140, 142, 139, 141, 143, 140, 138, 144, 142, 141, 143, 146, 151],
      'semitones': [-18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18],
      'files': <String>[],
      'msSetup': 12,
      'msWrite': 40,
      'formant': true,
    },
  };

  /// Chặn/đổi phản hồi `call`. Trả `null` để dùng phản hồi mặc định.
  Map<String, dynamic>? Function(Map<String, dynamic> request)? onCall;

  /// Giá trị `le_send` trả về (giả lập queue đầy khi `false`).
  bool sendResult = true;

  /// Mã lỗi `le_audio_start` trả về.
  int audioStartResult = LeError.LE_OK;

  final Pointer<LeState> _state = calloc<LeState>();
  final _events = StreamController<EngineEvent>.broadcast();
  final Map<int, _FakeJob> _jobs = {};
  final _clock = Stopwatch()..start();
  int _nextJobId = 1;
  bool _disposed = false;

  // Trạng thái giả lập, cập nhật theo lệnh spike.
  int _bufferSize = 128;
  double _sineGain = 0;
  int _voices = 0;
  bool _playRecord = false;
  bool _passthrough = false;
  String _sessionMode = 'default';

  EngineConfig? lastConfig;
  bool audioRunning = false;
  int audioStartCount = 0;
  int audioStopCount = 0;

  List<FakeSend> get sent => log.whereType<FakeSend>().toList();
  List<FakeCall> get calls => log.whereType<FakeCall>().toList();

  /// Struct state để test tự ghi giá trị (ví dụ `fake.state.bpm = 120`).
  LeState get state => _state.ref;

  /// Đẩy một event như engine thật (bất đồng bộ).
  void emit(EngineEvent event) {
    if (!_disposed) _events.add(event);
  }

  /// Hoàn thành job thủ công khi [autoCompleteJobs] = false.
  void completeJob(int jobId, {Map<String, dynamic>? result}) {
    final job = _jobs[jobId];
    if (job == null) return;
    job.status = 'done';
    if (result != null) job.result = result;
    emit(JobDone(jobId: jobId));
  }

  void failJob(int jobId, {int errorCode = LeError.LE_ERR_INTERNAL}) {
    final job = _jobs[jobId];
    if (job == null) return;
    job
      ..status = 'failed'
      ..errorCode = errorCode;
    emit(JobFailed(jobId: jobId, errorCode: errorCode));
  }

  @override
  int get apiVersion => LE_API_VERSION;

  @override
  Stream<EngineEvent> get events => _events.stream;

  @override
  int create(EngineConfig config) {
    lastConfig = config;
    _state.ref
      ..sampleRate = config.preferredSampleRate
      ..bufferSize = config.preferredBufferSize
      ..bpm = 120
      ..beatsPerBar = 4;
    _bufferSize = config.preferredBufferSize;
    for (var t = 0; t < LE_MAX_TRACKS; t++) {
      _state.ref.trackPlayingSlot[t] = -1;
    }
    return LeError.LE_OK;
  }

  @override
  int audioStart() {
    audioStartCount++;
    if (audioStartResult == LeError.LE_OK) audioRunning = true;
    return audioStartResult;
  }

  @override
  void audioStop() {
    audioStopCount++;
    audioRunning = false;
  }

  @override
  bool send(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double f1 = 0, double d0 = 0}) {
    log.add(FakeSend(type: type, track: track, slot: slot, i0: i0, f0: f0, f1: f1, d0: d0));
    switch (type) {
      case LeCommandType.LE_CMD_SPIKE_SINE:
        _sineGain = f1;
      case LeCommandType.LE_CMD_SPIKE_LOAD_VOICES:
        _voices = i0;
      case LeCommandType.LE_CMD_SPIKE_PLAY_RECORD:
        _playRecord = i0 == 1;
      case LeCommandType.LE_CMD_SPIKE_PASSTHROUGH:
        _passthrough = i0 == 1;
      case LeCommandType.LE_CMD_SPIKE_RECORD:
        // Như engine P0-03: thu xong thì phát RECORDING_FINISHED(a=b=-1, value=số frame).
        Timer(Duration(milliseconds: i0), () {
          final frames = (_state.ref.sampleRate > 0 ? _state.ref.sampleRate : 48000) * i0 / 1000;
          emit(RecordingFinished(track: -1, slot: -1, frames: frames.round()));
        });
      case LeCommandType.LE_CMD_SET_BPM:
        _state.ref.bpm = d0;
    }
    return sendResult;
  }

  @override
  Map<String, dynamic> call(Map<String, dynamic> request) {
    log.add(FakeCall(Map.unmodifiable(request)));
    final custom = onCall?.call(request);
    if (custom != null) return custom;

    final op = request['op'] as String?;
    if (op != null && jobOps.contains(op)) {
      final id = _nextJobId++;
      final job = _jobs[id] = _FakeJob(op, Map.of(jobResults[op] ?? const {}));
      if (autoCompleteJobs) {
        Timer(jobDelay, () {
          if (failingJobOps.contains(op)) {
            failJob(id);
          } else if (job.status == 'running') {
            completeJob(id);
          }
        });
      }
      return _ok({'jobId': id});
    }

    switch (op) {
      case 'engine.info':
        return _ok({
          'apiVersion': LE_API_VERSION,
          'sampleRate': _state.ref.sampleRate,
          'bufferSize': _bufferSize,
          'inputChannels': lastConfig?.numInputChannels ?? 1,
          'latencyRoundTripSamples': _state.ref.latencyRoundTripSamples,
          'device': 'FakeEngine',
          'stateSize': sizeOf<LeState>(),
          'commandSize': sizeOf<LeCommand>(),
          'sessionMode': _sessionMode,
        });
      case 'spike.setBufferSize':
        final frames = request['frames'];
        if (frames is! int || !const {64, 128, 256, 512, 1024}.contains(frames)) {
          return _err('INVALID_ARG', 'frames phải là 64|128|256|512|1024');
        }
        _bufferSize = frames;
        _state.ref.bufferSize = _bufferSize;
        return _ok({'bufferSize': _bufferSize});
      case 'spike.setSessionMode':
        final mode = request['mode'];
        if (mode != 'default' && mode != 'measurement') return _err('INVALID_ARG', 'mode: default|measurement');
        _sessionMode = mode as String;
        return _ok({'mode': _sessionMode, 'applied': true});
      case 'spike.sessionInfo':
        return _ok({
          'supported': true,
          'session': {
            'category': 'AVAudioSessionCategoryPlayAndRecord',
            'mode': _sessionMode == 'measurement' ? 'AVAudioSessionModeMeasurement' : 'AVAudioSessionModeDefault',
            'options': {
              'mixWithOthers': true,
              'defaultToSpeaker': true,
              'allowBluetoothA2DP': true,
              'allowBluetoothHFP': false,
              'allowAirPlay': false,
            },
            'sampleRate': 48000,
            'ioBufferDuration': _bufferSize / 48000,
            'inputLatency': 0.004,
            'outputLatency': 0.005,
            'inputs': ['MicrophoneBuiltIn'],
            'outputs': ['Speaker'],
          },
        });
      case 'job.result':
        final job = _jobs[request['jobId']];
        if (job == null) return _err('JOB_NOT_FOUND', 'jobId ${request['jobId']}');
        // Job lỗi vẫn ok:true (05 §3); chỉ jobId không tồn tại mới ok:false.
        return _ok({
          'status': job.status,
          if (job.status == 'running') 'progress': 0.0,
          if (job.status == 'done') 'result': job.result,
          if (job.status == 'failed') 'error': {'code': leErrorName(job.errorCode), 'message': ''},
        });
      case 'job.cancel':
        final job = _jobs[request['jobId']];
        if (job == null) return _err('JOB_NOT_FOUND', 'jobId ${request['jobId']}');
        failJob(request['jobId'] as int, errorCode: LeError.LE_ERR_JOB_CANCELLED);
        return _ok(const {});
      default:
        return _ok(const {});
    }
  }

  @override
  LeState readState() {
    if (simulate) _simulate();
    return _state.ref;
  }

  @override
  Float32List? getPeaks(String clipId, int level, int maxPairs) => null;

  @override
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    calloc.free(_state);
    _events.close();
  }

  void _simulate() {
    final s = _state.ref;
    s.publishCounter = (s.publishCounter + 1) & 0xFFFFFFFF;
    if (!audioRunning) {
      s
        ..cpuLoad = 0
        ..cpuPeak = 0
        ..inputPeak = 0
        ..activeVoices = 0;
      s.masterPeak[0] = 0;
      s.masterPeak[1] = 0;
      return;
    }
    final t = _clock.elapsedMicroseconds / 1e6;
    final load = 0.04 + _voices * 0.0045 * (128 / _bufferSize);
    final mic = 0.08 + 0.06 * math.sin(t * 5.3).abs() + (_passthrough ? 0.2 : 0);
    final out = _sineGain * (0.92 + 0.08 * math.sin(t * 2)) + (_playRecord ? 0.35 : 0) + (_passthrough ? mic : 0);
    s
      ..sampleRate = 48000
      ..bufferSize = _bufferSize
      ..cpuLoad = load.clamp(0, 1).toDouble()
      ..cpuPeak = (load * 1.35 + 0.03 * math.sin(t * 3).abs()).clamp(0, 1).toDouble()
      ..inputPeak = mic.clamp(0, 1).toDouble()
      ..activeVoices = _voices;
    s.masterPeak[0] = out.clamp(0, 1).toDouble();
    s.masterPeak[1] = (out * 0.97).clamp(0, 1).toDouble();
  }

  static Map<String, dynamic> _ok(Map<String, dynamic> result) => {'ok': true, 'result': result};
  static Map<String, dynamic> _err(String code, String message) => {
    'ok': false,
    'error': {'code': code, 'message': message},
  };
}

class _FakeJob {
  _FakeJob(this.op, this.result);
  final String op;
  Map<String, dynamic> result;
  String status = 'running';
  int errorCode = 0;
}

/// Một lệnh FakeEngineClient đã nhận.
sealed class FakeEngineOp {
  const FakeEngineOp();

  /// Dạng JSON gọn, dùng so snapshot trong test.
  Map<String, dynamic> toJson();
}

final class FakeSend extends FakeEngineOp {
  const FakeSend({
    required this.type,
    required this.track,
    required this.slot,
    required this.i0,
    required this.f0,
    required this.f1,
    required this.d0,
  });
  final int type;
  final int track;
  final int slot;
  final int i0;
  final double f0;
  final double f1;
  final double d0;

  /// Tên lệnh bỏ tiền tố `LE_CMD_`, ví dụ `SET_BPM`.
  String get name => leCommandName(type);

  @override
  Map<String, dynamic> toJson() => {
    'send': name,
    if (track != -1) 'track': track,
    if (slot != -1) 'slot': slot,
    if (i0 != 0) 'i0': i0,
    if (f0 != 0) 'f0': f0,
    if (f1 != 0) 'f1': f1,
    if (d0 != 0) 'd0': d0,
  };

  @override
  String toString() => 'FakeSend(${toJson()})';
}

final class FakeCall extends FakeEngineOp {
  const FakeCall(this.request);
  final Map<String, dynamic> request;
  String get op => request['op'] as String;

  @override
  Map<String, dynamic> toJson() => {'call': request};

  @override
  String toString() => 'FakeCall($request)';
}

/// Tên `LeCommandType` (bỏ tiền tố `LE_CMD_`), để log và snapshot dễ đọc.
String leCommandName(int type) => switch (type) {
  LeCommandType.LE_CMD_TRANSPORT_PLAY => 'TRANSPORT_PLAY',
  LeCommandType.LE_CMD_TRANSPORT_STOP => 'TRANSPORT_STOP',
  LeCommandType.LE_CMD_SET_BPM => 'SET_BPM',
  LeCommandType.LE_CMD_SET_QUANTIZE => 'SET_QUANTIZE',
  LeCommandType.LE_CMD_METRONOME => 'METRONOME',
  LeCommandType.LE_CMD_SET_COUNT_IN => 'SET_COUNT_IN',
  LeCommandType.LE_CMD_CLIP_LAUNCH => 'CLIP_LAUNCH',
  LeCommandType.LE_CMD_CLIP_STOP => 'CLIP_STOP',
  LeCommandType.LE_CMD_SCENE_LAUNCH => 'SCENE_LAUNCH',
  LeCommandType.LE_CMD_STOP_ALL => 'STOP_ALL',
  LeCommandType.LE_CMD_CLIP_RECORD => 'CLIP_RECORD',
  LeCommandType.LE_CMD_RECORD_STOP => 'RECORD_STOP',
  LeCommandType.LE_CMD_OVERDUB_TOGGLE => 'OVERDUB_TOGGLE',
  LeCommandType.LE_CMD_TRACK_GAIN => 'TRACK_GAIN',
  LeCommandType.LE_CMD_TRACK_PAN => 'TRACK_PAN',
  LeCommandType.LE_CMD_TRACK_MUTE => 'TRACK_MUTE',
  LeCommandType.LE_CMD_TRACK_SOLO => 'TRACK_SOLO',
  LeCommandType.LE_CMD_TRACK_ARM => 'TRACK_ARM',
  LeCommandType.LE_CMD_TRACK_MONITOR => 'TRACK_MONITOR',
  LeCommandType.LE_CMD_SELECT_TRACK => 'SELECT_TRACK',
  LeCommandType.LE_CMD_NOTE_ON => 'NOTE_ON',
  LeCommandType.LE_CMD_NOTE_OFF => 'NOTE_OFF',
  LeCommandType.LE_CMD_ALL_NOTES_OFF => 'ALL_NOTES_OFF',
  LeCommandType.LE_CMD_FX_PARAM => 'FX_PARAM',
  LeCommandType.LE_CMD_FX_BYPASS => 'FX_BYPASS',
  LeCommandType.LE_CMD_MASTER_GAIN => 'MASTER_GAIN',
  LeCommandType.LE_CMD_SPIKE_SINE => 'SPIKE_SINE',
  LeCommandType.LE_CMD_SPIKE_LOAD_VOICES => 'SPIKE_LOAD_VOICES',
  LeCommandType.LE_CMD_SPIKE_RECORD => 'SPIKE_RECORD',
  LeCommandType.LE_CMD_SPIKE_PLAY_RECORD => 'SPIKE_PLAY_RECORD',
  LeCommandType.LE_CMD_SPIKE_PASSTHROUGH => 'SPIKE_PASSTHROUGH',
  _ => 'CMD_$type',
};
