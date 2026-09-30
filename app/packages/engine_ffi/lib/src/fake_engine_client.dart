import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'engine_api.dart';
import 'engine_event.dart';
import 'fake_session_sim.dart';
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
    'capture.analyze',
    'export.scene',
    'latency.calibrate',
  };

  /// Job của op nằm ở đây KHÔNG tự hoàn thành (dù [autoCompleteJobs]) — test tự gọi [completeJob]/[failJob].
  final Set<String> manualJobOps = {};

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

  /// Kết quả `capture.analyze` giả (07 §4.2: "A3 +12 cent"). confidence < 0.6 → nhánh PITCH_NOT_DETECTED.
  int captureRootNote = 57;
  int captureCents = 12;
  double captureConfidence = 0.91;

  /// Ghi file giả ở `capture.start {path}` khi dừng (để test app xoá file khi huỷ).
  bool writeCaptureFiles = true;

  /// Mẫu capture kế tiếp là im lặng (engine thật trong `sim.offline` không có input nên luôn im lặng):
  /// `capture.analyze` → `silent: true, rootNote: −1`; `instrument.createFromRecording` → job lỗi INVALID_ARG.
  bool captureSilent = false;
  final Set<String> _silentFiles = {};

  /// Mẫu có sẵn trên đĩa (không qua capture) — test hợp đồng tự ghi WAV cho engine thật; Fake chỉ cần biết độ dài
  /// và có cao độ rõ hay không.
  final Map<String, bool> _pitchedFiles = {};

  void addSample(String path, {double seconds = 1.0, required bool pitched}) {
    _capturedSeconds[path] = seconds;
    _pitchedFiles[path] = pitched;
    _silentFiles.remove(path);
  }

  /// File khác (SFZ…) coi như có trên đĩa — cho `preview.play` kiểm file như engine.
  void addTextFile(String path) => _textFiles.add(path);
  final _textFiles = <String>{};
  bool _fileExists(String path) => _capturedSeconds.containsKey(path) || _textFiles.contains(path);

  /// `LeConfig.libraryDir` và thư mục của `project.open` — gốc của đường dẫn tương đối (05 §3 `base`).
  String _libraryDir = '';
  String? _projectDir;

  String? _capPath;
  double _capMax = 0;
  double _capElapsed = 0;
  bool _capRunning = false;
  Map<String, dynamic>? _capResult;
  final Map<String, double> _capturedSeconds = {};

  bool get captureRunning => _capRunning;

  /// FX người dùng theo (track 0..7, index 0..2) (05 §3 `fx.set`). Test đọc để kiểm lệnh UI.
  final Map<(int, int), FakeFx> fx = {};

  /// Tham số chuỗi master cố định theo (slot, param): slot 0 = EQ3 (0/1/2 dB), slot 1 = Limiter
  /// (0 = ceiling dB, 1 = release ms). Chỉ đổi qua `FX_PARAM track −1`; `project.open` đưa về mặc định.
  final Map<(int, int), double> master = {};

  /// `FX_BYPASS track −1 slot 0`.
  bool masterEqBypass = false;

  static const _masterDefaults = {(0, 0): 0.0, (0, 1): 0.0, (0, 2): 0.0, (1, 0): -0.3};

  /// Envelope theo instrumentId (`instrument.setEnvelope`).
  final Map<String, ({double a, double d, double s, double r})> envelopes = {};

  /// Nhạc cụ tự thu đã đăng ký (ngay khi `instrument.createFromRecording` được nhận, trước khi job xong).
  final Set<String> instruments = {};

  /// `audio.setInputEnabled` (07 §4.0): false = chế độ chỉ phát khi người dùng từ chối quyền mic.
  bool inputEnabled = true;

  /// Thiết bị MIDI giả (`midi.listDevices`). Test sửa danh sách rồi `emit(MidiDevicesChanged())`.
  /// `open` = cổng đang mở (thiết bị cắm + enabled).
  final List<Map<String, dynamic>> midiInputs = [
    {'id': 'usb:launchpad-mini', 'name': 'Launchpad Mini MK3', 'enabled': true, 'open': true},
    {'id': 'ble:nanokontrol2', 'name': 'nanoKONTROL2', 'enabled': false, 'open': false},
  ];
  final List<Map<String, dynamic>> midiOutputs = [
    {'id': 'usb:launchpad-mini', 'name': 'Launchpad Mini MK3'},
  ];

  /// `midi.enableDevice` là thiết lập toàn cục, nhớ cả id chưa cắm (05 §3).
  final Map<String, bool> midiEnabledIds = {};

  /// Màu track đã gửi qua `track.configure {color}` (engine dùng cho LED Launchpad).
  final Map<int, String> trackColors = {};

  /// `memory.pressure` / engine.info.memoryMB.
  double memoryMB = 180;

  /// Nguồn đang nghe thử (`preview.play`, đã điền `base`), null = không nghe. [previewCount] = số lần gọi play.
  Map<String, Object?>? preview;
  int previewCount = 0;

  /// Target đang learn (`midi.learnStart`), null = không learn.
  Map<String, dynamic>? midiLearnTarget;

  /// `midi.setMappings` gần nhất.
  List<Object?> midiMappings = const [];

  bool linkEnabled = false;
  bool linkStartStopSync = true;

  /// `latency.setOffset` (sample).
  int latencyOffset = 0;

  /// `latency.calibrate` giả: đo được [calibrateMeasuredSamples], device báo [calibrateReportedSamples].
  /// Job xong thì engine áp dụng ngay offset = measured − reported (05 §3) và LeState báo L thực tế = measured.
  int calibrateMeasuredSamples = 492;
  int calibrateReportedSamples = 470;

  /// Đặt khác null → `latency.calibrate` thất bại: job failed AUDIO_DEVICE, message = lý do này (05 §3:
  /// NO_SIGNAL | TOO_NOISY | INCONSISTENT | DEVICE_CHANGED | TIMEOUT). Engine thật trong sim: NO_SIGNAL.
  String? calibrateFailure;

  /// Nguồn của lần learn gần nhất (`midi.learnResult`).
  Map<String, dynamic>? _learnResult;

  /// Giả lập người dùng vặn/bấm controller trong lúc learn → `MIDI_LEARNED` (a = kind, b = number);
  /// nguồn đầy đủ đọc bằng `midi.learnResult`.
  void simulateMidiLearn({
    required bool isCc,
    required int number,
    int channel = 0,
    String deviceId = 'usb:launchpad-mini',
    String deviceName = 'Launchpad Mini MK3',
  }) {
    if (midiLearnTarget == null) return;
    midiLearnTarget = null;
    _learnResult = {
      'deviceId': deviceId,
      'deviceName': deviceName,
      'kind': isCc ? 'cc' : 'note',
      'channel': channel,
      'number': number,
    };
    emit(MidiLearned(isCc: isCc, number: number));
  }

  /// Như `sim.midiIn` của engine thật: đang learn → MIDI_LEARNED từ thiết bị "virtual"; không thì bỏ qua.
  void midiIn(List<int> bytes) {
    if (bytes.length < 3) return;
    final type = bytes[0] & 0xF0;
    if (type != 0x90 && type != 0xB0) return;
    simulateMidiLearn(
      isCc: type == 0xB0,
      number: bytes[1],
      channel: bytes[0] & 0x0F,
      deviceId: 'virtual',
      deviceName: 'virtual',
    );
  }

  /// Test: kết quả giả của `export.scene.clippedSamples` / `export.jamStop.droppedFrames`.
  int exportClippedSamples = 0;
  int jamDroppedFrames = 0;

  String? _jamPath;
  double _jamElapsed = 0;
  bool get jamRunning => _jamPath != null;

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

  /// Transport + ClipScheduler giả (04 §3). Test điều khiển thời gian bằng [advanceBeats].
  late final FakeSessionSim session = FakeSessionSim(emit);

  /// true sau lệnh session đầu tiên → từ đó [readState] ghi transport/clip của [session] vào LeState.
  /// Test chỉ tự ghi `state` (không gửi lệnh session) thì không bị ghi đè.
  bool _sessionTouched = false;
  int _lastSimMicros = 0;

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

  /// jobId cấp gần nhất (0 = chưa có job) — test dùng với [completeJob]/[failJob].
  int get lastJobId => _nextJobId - 1;

  /// Hoàn thành job thủ công khi [autoCompleteJobs] = false.
  void completeJob(int jobId, {Map<String, dynamic>? result}) {
    final job = _jobs[jobId];
    if (job == null) return;
    job.status = 'done';
    if (result != null) job.result = result;
    job.onDone?.call();
    emit(JobDone(jobId: jobId));
  }

  void failJob(int jobId, {int errorCode = LeError.LE_ERR_INTERNAL, String message = ''}) {
    final job = _jobs[jobId];
    if (job == null) return;
    job
      ..status = 'failed'
      ..errorCode = errorCode
      ..errorMessage = message;
    emit(JobFailed(jobId: jobId, errorCode: errorCode));
  }

  @override
  int get apiVersion => LE_API_VERSION;

  @override
  Stream<EngineEvent> get events => _events.stream;

  @override
  int create(EngineConfig config) {
    lastConfig = config;
    _libraryDir = config.libraryDir;
    _state.ref
      ..sampleRate = config.preferredSampleRate
      ..bufferSize = config.preferredBufferSize
      ..bpm = 120
      ..beatsPerBar = 4;
    _bufferSize = config.preferredBufferSize;
    session.sampleRate = config.preferredSampleRate;
    master.addAll(_masterDefaults);
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
    }
    if (type == LeCommandType.LE_CMD_FX_BYPASS && track == -1) {
      if (slot != 0) return false; // bypass được EQ master; limiter thì không (04 §9)
      masterEqBypass = i0 != 0;
      return sendResult;
    }
    if (type == LeCommandType.LE_CMD_FX_PARAM && track == -1) {
      // Master cố định: slot 0 EQ3 (param 0..2), slot 1 Limiter (param 0..1); không bypass được.
      if (slot < 0 || slot > 1 || i0 < 0 || i0 > (slot == 0 ? 2 : 1)) return false;
      master[(slot, i0)] = f0;
      return sendResult;
    }
    if (type == LeCommandType.LE_CMD_FX_PARAM || type == LeCommandType.LE_CMD_FX_BYPASS) {
      if (track < 0 || track >= LE_MAX_TRACKS || slot < 0 || slot >= maxFxSlots) return false;
      final f = fx[(track, slot)];
      if (f == null) return false; // như engine: slot trống → false
      if (type == LeCommandType.LE_CMD_FX_BYPASS) {
        f.bypass = i0 != 0;
      } else {
        if (!fxParamIds[f.type]!.contains(i0)) return false; // paramId không có ở loại này
        f.params['$i0'] = f0;
      }
      return sendResult;
    }
    if (type < LeCommandType.LE_CMD_SPIKE_SINE) {
      _sessionTouched = true;
      if (!session.handle(type, track, slot, i0, d0, f0)) return false;
    }
    return sendResult;
  }

  @override
  Map<String, dynamic> call(Map<String, dynamic> request) {
    log.add(FakeCall(Map.unmodifiable(request)));
    final custom = onCall?.call(request);
    if (custom != null) return custom;

    // 05 §1: như engine thật, chuỗi chứa NUL (U+0000) → INVALID_ARG (JUCE không đọc được).
    if (_hasNul(request)) return _err('INVALID_ARG', 'string contains NUL');

    final op = request['op'] as String?;
    if (op != null && jobOps.contains(op)) {
      final spec = _jobSpec(op, request);
      if (spec.error != null) return spec.error!;
      final id = _nextJobId++;
      final job = _jobs[id] = _FakeJob(op, spec.result ?? Map.of(jobResults[op] ?? const {}))..onDone = spec.onDone;
      // Id đăng ký ngay khi nhận lệnh → setEnvelope gọi liền sau (lúc mở project) vẫn hợp lệ.
      if (op == 'instrument.createFromRecording' && spec.failCode == null) {
        instruments.add(request['instrumentId'] as String);
      }
      if (autoCompleteJobs && !manualJobOps.contains(op)) {
        Timer(jobDelay, () {
          if (job.status != 'running') return; // đã huỷ
          if (spec.failCode != null) {
            failJob(id, errorCode: spec.failCode!, message: spec.result?['message'] as String? ?? '');
          } else if (failingJobOps.contains(op)) {
            failJob(id);
          } else {
            for (final p in spec.progress) {
              emit(JobProgress(jobId: id, progress: p));
            }
            completeJob(id);
          }
        });
      }
      return _ok({'jobId': id});
    }

    if (op != null && _sessionOps.contains(op)) {
      _sessionTouched = true;
      final r = _sessionCall(op, request);
      if (r != null) return r;
    }

    switch (op) {
      case 'capture.start':
        final path = request['path'], max = request['maxSeconds'];
        if (path is! String || !path.startsWith('/') || max is! num || max < 0.1 || max > 120) {
          return _err('INVALID_ARG', 'path tuyệt đối + maxSeconds 0.1..120');
        }
        if (!audioRunning) return _err('AUDIO_DEVICE', 'audio chưa chạy');
        if (!inputEnabled) return _err('MIC_PERMISSION', 'input đang tắt (audio.setInputEnabled false)');
        if (_capRunning) return _err('INVALID_ARG', 'đang thu');
        _capPath = path;
        _capMax = max.toDouble();
        _capElapsed = 0;
        _capRunning = true;
        _capResult = null;
        return _ok(const {});
      case 'capture.stop':
        if (_capRunning) _finishCapture(emitEvent: false);
        final res = _capResult;
        return res == null ? _err('INVALID_ARG', 'chưa capture.start') : _ok(res); // idempotent
      case 'instrument.setMode':
        if (request['mode'] != 'natural' && request['mode'] != 'classic') return _err('INVALID_ARG', 'mode');
        return _ok(const {});
      case 'midi.listDevices':
        return _ok({
          'inputs': [for (final d in midiInputs) Map.of(d)],
          'outputs': [for (final d in midiOutputs) Map.of(d)],
        });
      case 'midi.enableDevice':
        final id = request['id'], en = request['enabled'];
        if (id is! String || en is! bool) return _err('INVALID_ARG', 'id: string, enabled: bool');
        midiEnabledIds[id] = en; // nhớ cả id chưa cắm
        final dev = midiInputs.where((d) => d['id'] == id).firstOrNull;
        if (dev != null) {
          dev['enabled'] = en;
          dev['open'] = en;
        }
        return _ok(const {});
      case 'midi.learnStart':
        final t = request['target'];
        if (t is! Map || !_validLearnTarget(t)) return _err('INVALID_ARG', 'target {kind:"clip"|"fx", …}');
        midiLearnTarget = t.cast<String, dynamic>();
        return _ok(const {});
      case 'midi.learnCancel':
        midiLearnTarget = null;
        return _ok(const {});
      case 'midi.learnResult':
        final r = _learnResult;
        return r == null ? _err('INVALID_ARG', 'chưa learn lần nào') : _ok(Map.of(r));
      case 'midi.setMappings':
        final m = request['mappings'];
        if (m is! List) return _err('INVALID_ARG', 'mappings: list');
        for (final x in m) {
          final src = x is Map ? x['src'] : null, target = x is Map ? x['target'] : null;
          final ch = src is Map ? src['channel'] : null;
          if (src is! Map ||
              (src['kind'] != 'note' && src['kind'] != 'cc') ||
              ch is! int ||
              ch < -1 ||
              ch > 15 ||
              target is! Map ||
              !_validLearnTarget(target)) {
            return _err('INVALID_ARG', 'mapping: kind note|cc, channel −1..15, target hợp lệ');
          }
        }
        midiMappings = List.unmodifiable(m);
        return _ok(const {});
      case 'memory.pressure':
        final level = request['level'];
        if (level != 'warning' && level != 'critical') return _err('INVALID_ARG', 'level: warning|critical');
        final freed = level == 'critical' ? 60.0 : 20.0;
        memoryMB -= freed;
        emit(MemoryWarning(critical: level == 'critical', megabytes: memoryMB));
        return _ok({'freedMB': freed, 'usedMB': memoryMB});
      case 'preview.play':
        // 05 §3: kênh preview riêng — không đụng track / transport; gọi lần nữa thì thay bản đang nghe.
        final src = request['source'];
        final note = request['note'] ?? 60, dur = request['durationMs'] ?? 1500;
        if (src is! Map) return _err('INVALID_ARG', 'source: {kind:"sfz",path} | {kind:"audio",file}');
        final base = src['base'] ?? 'library';
        final ref = switch (src['kind']) {
          'sfz' => src['path'],
          'audio' => src['file'],
          _ => null,
        };
        if (ref is! String || ref.isEmpty || (base != 'library' && base != 'project')) {
          return _err('INVALID_ARG', 'source: kind sfz|audio, path/file khác rỗng, base library|project');
        }
        if (note is! int || note < 0 || note > 127 || dur is! num || dur <= 0) {
          return _err('INVALID_ARG', 'note 0..127, durationMs > 0');
        }
        final root = base == 'library' ? _libraryDir : _projectDir;
        // Như engine (68): base "project" khi chưa project.open → INVALID_ARG; thiếu file → FILE_NOT_FOUND.
        if (root == null) return _err('INVALID_ARG', 'base "project" cần project.open trước');
        if (!_fileExists('$root/$ref')) return _err('FILE_NOT_FOUND', 'không có file $ref');
        preview = Map.unmodifiable({...src.cast<String, Object?>(), 'base': base});
        previewCount++;
        return _ok(const {});
      case 'preview.stop':
        preview = null;
        return _ok(const {});
      case 'link.enable':
        final en = request['enabled'], sync = request['startStopSync'];
        if (en is! bool || sync is! bool) return _err('INVALID_ARG', 'enabled/startStopSync: bool');
        linkEnabled = en;
        linkStartStopSync = sync;
        _state.ref.linkEnabled = en ? 1 : 0;
        return _ok(const {});
      case 'latency.setOffset':
        final n = request['samples'];
        if (n is! int || n.abs() > 96000) return _err('INVALID_ARG', 'samples: int, |x| ≤ 96000');
        latencyOffset = n;
        return _ok(const {});
      case 'audio.setInputEnabled':
        final en = request['enabled'];
        if (en is! bool) return _err('INVALID_ARG', 'enabled: bool');
        inputEnabled = en;
        return _ok({'inputChannels': en ? (lastConfig?.numInputChannels ?? 1) : 0});
      case 'instrument.setEnvelope':
        final id = request['instrumentId'];
        final a = request['a'], d = request['d'], sus = request['s'], r = request['r'];
        if (id is! String || !instruments.contains(id)) return _err('INVALID_ARG', 'instrumentId lạ');
        bool secs(Object? v) => v is num && v >= 0 && v <= 10;
        if (!secs(a) || !secs(d) || !secs(r) || sus is! num || sus < 0 || sus > 1) {
          return _err('INVALID_ARG', 'a/d/r 0..10 (giây), s 0..1');
        }
        double sec(Object? v) => (v as num).toDouble();
        envelopes[id] = (a: sec(a), d: sec(d), s: sus.toDouble(), r: sec(r));
        return _ok(const {});
      case 'fx.set':
        final t = request['track'], i = request['index'], type = request['type'], params = request['params'] ?? {};
        final bypass = request['bypass'] ?? false;
        if (t is! int || t < 0 || t >= LE_MAX_TRACKS || i is! int || i < 0 || i >= maxFxSlots) {
          return _err('INVALID_ARG', 'track 0..7 (master không có slot người dùng), index 0..2');
        }
        final ids = fxParamIds[type == 'compressor' ? 'comp' : type]; // "compressor" là bí danh của "comp"
        final canonical = type == 'compressor' ? 'comp' : type;
        if (ids == null) return _err('INVALID_ARG', 'type: filter|delay|reverb|eq3|comp');
        if (params is! Map || bypass is! bool) return _err('INVALID_ARG', 'params/bypass');
        final values = <String, double>{};
        for (final e in params.entries) {
          final k = e.key, v = e.value;
          if (k is! String || !ids.contains(int.tryParse(k)) || v is! num) return _err('INVALID_ARG', 'param $k');
          values[k] = v.toDouble();
        }
        fx[(t, i)] = FakeFx(canonical as String, values, bypass: bypass);
        return _ok(const {});
      case 'fx.remove':
        final t = request['track'], i = request['index'];
        if (t is! int || t < 0 || t >= LE_MAX_TRACKS || i is! int || i < 0 || i >= maxFxSlots) {
          return _err('INVALID_ARG', 'track 0..7, index 0..2');
        }
        fx.remove((t, i)); // slot cố định: không dồn slot sau; slot trống → vẫn ok
        return _ok(const {});
      case 'export.jamStart':
        final path = request['path'];
        if (path is! String || !path.startsWith('/')) return _err('INVALID_ARG', 'path tuyệt đối');
        if (jamRunning) return _err('INVALID_ARG', 'đang ghi jam');
        if (!audioRunning) return _err('AUDIO_DEVICE', 'audio chưa chạy');
        _jamPath = path;
        _jamElapsed = 0;
        return _ok(const {});
      case 'export.jamStop':
        final path = _jamPath;
        if (path == null) return _err('INVALID_ARG', 'chưa export.jamStart');
        _jamPath = null;
        _writePlaceholder(path);
        return _ok({'file': path, 'seconds': _jamElapsed, 'droppedFrames': jamDroppedFrames});
      case 'engine.info':
        return _ok({
          'apiVersion': LE_API_VERSION,
          'sampleRate': _state.ref.sampleRate,
          'bufferSize': _bufferSize,
          'inputChannels': inputEnabled ? (lastConfig?.numInputChannels ?? 1) : 0,
          'latencyRoundTripSamples': _state.ref.latencyRoundTripSamples,
          'device': 'FakeEngine',
          'stateSize': sizeOf<LeState>(),
          'commandSize': sizeOf<LeCommand>(),
          'sessionMode': _sessionMode,
          'nonFiniteSamples': 0, // mẫu NaN/Inf bị chặn ở master (debug)
          'latencyOffsetSamples': latencyOffset,
          'memoryMB': memoryMB,
          'tempoState': {
            'mode': session.tempoMode,
            'hasTempo': session.hasTempo,
            'firstLoopBeats': session.firstLoopBeats ?? 0.0, // như engine: luôn là số, 0 = chưa biết
          },
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
        if (request['jobId'] is! int) return _err('INVALID_ARG', 'jobId phải là số nguyên');
        final job = _jobs[request['jobId']];
        if (job == null) return _err('JOB_NOT_FOUND', 'jobId ${request['jobId']}');
        // Job lỗi vẫn ok:true (05 §3); chỉ jobId không tồn tại mới ok:false.
        return _ok({
          'status': job.status,
          if (job.status == 'running') 'progress': 0.0,
          if (job.status == 'done') 'result': job.result,
          if (job.status == 'failed') 'error': {'code': leErrorName(job.errorCode), 'message': job.errorMessage},
        });
      case 'job.cancel':
        if (request['jobId'] is! int) return _err('INVALID_ARG', 'jobId phải là số nguyên');
        final job = _jobs[request['jobId']];
        if (job == null) return _err('JOB_NOT_FOUND', 'jobId ${request['jobId']}');
        failJob(request['jobId'] as int, errorCode: LeError.LE_ERR_JOB_CANCELLED);
        return _ok(const {});
      default:
        return _ok(const {});
    }
  }

  /// Cho đồng hồ nhạc chạy thêm [beats] (test). Không có tác dụng khi transport dừng.
  void advanceBeats(double beats) {
    _sessionTouched = true;
    session.advanceBeats(beats);
  }

  /// Cho thời gian thực trôi [seconds]: capture (không phụ thuộc transport) + transport nếu đang chạy.
  void advanceSeconds(double seconds) {
    _tickCapture(seconds);
    if (jamRunning) _jamElapsed += seconds;
    _sessionTouched = true;
    session.advanceSeconds(seconds); // gồm vòng đầu pedal mode (đo bằng giây khi transport chưa chạy)
  }

  void advance(Duration d) => advanceSeconds(d.inMicroseconds / 1e6);

  void _tickCapture(double dt) {
    if (!_capRunning) return;
    _capElapsed += dt;
    if (_capElapsed >= _capMax) {
      _capElapsed = _capMax;
      _finishCapture(emitEvent: true);
    }
  }

  /// Dừng capture: ghi kết quả; tự dừng ở maxSeconds thì phát RECORDING_FINISHED(-2, -2, frames) (05 §3).
  void _finishCapture({required bool emitEvent}) {
    _capRunning = false;
    final path = _capPath!;
    _capResult = {'file': path, 'seconds': _capElapsed};
    _capturedSeconds[path] = _capElapsed;
    _pitchedFiles.remove(path);
    if (captureSilent) {
      _silentFiles.add(path);
    } else {
      _silentFiles.remove(path);
    }
    if (writeCaptureFiles) _writePlaceholder(path);
    if (emitEvent) emit(RecordingFinished(track: -2, slot: -2, frames: (_capElapsed * session.sampleRate).round()));
  }

  /// Target learn 05 §3 (khớp `midi::LearnAction`): clip · scene · transport · stopAll · trackGain · trackMute · fx.
  static bool _validLearnTarget(Map<dynamic, dynamic> t) {
    bool inRange(Object? v, int min, int max) => v is int && v >= min && v < max;
    bool optNum(Object? v) => v == null || v is num;
    return switch (t['kind']) {
      'clip' => inRange(t['track'], 0, LE_MAX_TRACKS) && inRange(t['slot'], 0, LE_MAX_SCENES),
      'scene' => inRange(t['slot'], 0, LE_MAX_SCENES),
      'transport' => const {'play', 'stop', 'toggle'}.contains(t['action']),
      'stopAll' || 'loopButton' || 'trackStop' || 'undoOverdub' => true,
      'trackGain' => inRange(t['track'], 0, LE_MAX_TRACKS) && optNum(t['minDb']) && optNum(t['maxDb']),
      'trackMute' => inRange(t['track'], 0, LE_MAX_TRACKS),
      'fx' =>
        (t['track'] == -1
                ? inRange(t['slot'], 0, 2) && inRange(t['param'], 0, t['slot'] == 0 ? 3 : 2)
                : inRange(t['track'], 0, LE_MAX_TRACKS) &&
                      inRange(t['slot'], 0, maxFxSlots) &&
                      inRange(t['param'], 0, 8)) &&
            optNum(t['min']) &&
            optNum(t['max']),
      _ => false,
    };
  }

  /// Ghi file giả (64 byte) để app/test thấy file tồn tại. Đường dẫn giả không ghi được → bỏ qua.
  void _writePlaceholder(String path) {
    if (!writeCaptureFiles) return;
    try {
      File(path)
        ..parent.createSync(recursive: true)
        ..writeAsBytesSync(List<int>.filled(64, 0));
    } on FileSystemException {
      // đường dẫn giả trong test — bỏ qua
    }
  }

  static bool _hasNul(Object? v) => switch (v) {
    String() => v.contains('\u0000'),
    Map() => v.entries.any((e) => _hasNul(e.key) || _hasNul(e.value)),
    List() => v.any(_hasNul),
    _ => false,
  };

  /// Mẫu thêm bằng [addSample] theo cờ `pitched`; mẫu capture theo [captureConfidence].
  double _confidenceOf(Object? file) => switch (_pitchedFiles[file]) {
    true => 0.91,
    false => 0.2,
    null => captureConfidence,
  };

  /// Kết quả / lỗi của job theo request (Fake).
  ({
    Map<String, dynamic>? result,
    int? failCode,
    Map<String, dynamic>? error,
    List<double> progress,
    void Function()? onDone,
  })
  _jobSpec(String op, Map<String, dynamic> r) {
    switch (op) {
      case 'track.setInstrument':
        final inst = r['instrument'];
        if (inst is Map && inst['kind'] == 'user' && !instruments.contains(inst['id'])) {
          return (
            result: null,
            failCode: null,
            error: _err('INVALID_ARG', 'nhạc cụ tự thu lạ: ${inst['id']}'),
            progress: const [],
            onDone: null,
          );
        }
      case 'capture.analyze':
        final secs = _capturedSeconds[r['file']];
        if (secs == null) {
          return (
            result: null,
            failCode: null,
            error: _err('FILE_NOT_FOUND', '${r['file']}'),
            progress: const [],
            onDone: null,
          );
        }
        final sr = session.sampleRate;
        final frames = (secs * sr).round();
        if (_silentFiles.contains(r['file'])) {
          return (
            result: {
              'trimStartSample': 0,
              'trimEndSample': 0,
              'rootNote': -1,
              'cents': 0,
              'confidence': 0.0,
              'peaks': List<double>.filled(1024, 0),
              'silent': true,
              'frames': frames,
              'sampleRate': sr,
            },
            failCode: null,
            error: null,
            progress: const [0.5],
            onDone: null,
          );
        }
        final conf = _confidenceOf(r['file']);
        return (
          result: {
            'trimStartSample': (0.05 * sr).round(),
            'trimEndSample': ((secs - 0.1).clamp(0.1, secs) * sr).round(),
            'rootNote': conf < 0.6 ? -1 : captureRootNote,
            'cents': conf < 0.6 ? 0 : captureCents,
            'confidence': conf,
            'silent': false,
            'frames': frames,
            'sampleRate': sr,
            'peaks': [
              for (var i = 0; i < 512; i++) ...[
                -(0.6 * math.exp(-i / 160) + 0.05) * (0.8 + 0.2 * math.sin(i * 0.9)),
                (0.6 * math.exp(-i / 160) + 0.05) * (0.8 + 0.2 * math.sin(i * 1.3)),
              ],
            ],
          },
          failCode: null,
          error: null,
          progress: const [0.5],
          onDone: null,
        );
      case 'instrument.createFromRecording':
        final mode = r['mode'];
        if (r['instrumentId'] is! String || r['file'] is! String || (mode != 'natural' && mode != 'classic')) {
          return (
            result: null,
            failCode: null,
            error: _err('INVALID_ARG', 'instrumentId/file/mode'),
            progress: const [],
            onDone: null,
          );
        }
        if (_silentFiles.contains(r['file'])) {
          return (result: {}, failCode: LeError.LE_ERR_INVALID_ARG, error: null, progress: const [], onDone: null);
        }
        final root = r['rootNote'];
        final conf = _confidenceOf(r['file']);
        if (root == null && conf < 0.6) {
          return (
            result: {},
            failCode: LeError.LE_ERR_PITCH_NOT_DETECTED,
            error: null,
            progress: const [],
            onDone: null,
          );
        }
        return (
          result: {
            'rootNote': root ?? captureRootNote,
            'cents': root == null ? captureCents : 0,
            'confidence': conf,
            'zones': 13,
            'cached': false,
          },
          failCode: null,
          error: null,
          progress: const [0.25, 0.5, 0.75],
          onDone: null,
        );
      case 'latency.calibrate':
        final busy = _jobs.values.any((j) => j.op == 'latency.calibrate' && j.status == 'running');
        final err = !inputEnabled
            ? _err('MIC_PERMISSION', 'input đang tắt')
            : busy
            ? _err('INVALID_ARG', 'đang đo')
            : null;
        if (err != null) return (result: null, failCode: null, error: err, progress: const [], onDone: null);
        if (calibrateFailure case final reason?) {
          return (
            result: {'message': reason},
            failCode: LeError.LE_ERR_AUDIO_DEVICE,
            error: null,
            progress: const [0.3],
            onDone: null,
          );
        }
        final measured = calibrateMeasuredSamples, reported = calibrateReportedSamples;
        return (
          result: {
            'measuredSamples': measured,
            'reportedSamples': reported,
            'offsetSamples': measured - reported,
            'spreadSamples': 2,
            'confidence': 0.93,
            'validRuns': 5,
          },
          failCode: null,
          error: null,
          progress: const [0.3, 0.6, 0.9],
          onDone: () {
            latencyOffset = measured - reported; // engine áp dụng ngay (không lưu qua lần mở app)
            _state.ref.latencyRoundTripSamples = measured;
          },
        );
      case 'export.scene':
        final scene = r['scene'], bars = r['bars'], path = r['path'], format = r['format'] ?? 'wav';
        final stems = r['stems'] ?? false;
        if (scene is! int || scene < 0 || scene >= LE_MAX_SCENES || bars is! int || bars < 1 || bars > 512) {
          return (
            result: null,
            failCode: null,
            error: _err('INVALID_ARG', 'scene 0..7, bars 1..512'),
            progress: const [],
            onDone: null,
          );
        }
        if (path is! String || !path.startsWith('/') || (format != 'wav' && format != 'm4a') || stems is! bool) {
          return (
            result: null,
            failCode: null,
            error: _err('INVALID_ARG', 'path tuyệt đối, format wav|m4a, stems bool'),
            progress: const [],
            onDone: null,
          );
        }
        final dot = path.lastIndexOf('.');
        final base = dot > path.lastIndexOf('/') ? path.substring(0, dot) : path;
        final stemFiles = [
          if (stems)
            for (var t = 0; t < LE_MAX_TRACKS; t++)
              if (session.hasClip(t, scene)) '${base}_t${t + 1}.$format',
        ];
        final seconds = bars * session.beatsPerBar * 60 / session.bpm;
        return (
          result: {
            'file': path,
            'seconds': seconds,
            'frames': (seconds * session.sampleRate).round(),
            'clippedSamples': exportClippedSamples,
            if (stems) 'stems': stemFiles,
          },
          failCode: null,
          error: null,
          progress: const [0.25, 0.5, 0.75],
          onDone: () => [path, ...stemFiles].forEach(_writePlaceholder),
        );
    }
    return (result: null, failCode: null, error: null, progress: const [], onDone: null);
  }

  @override
  LeState readState() {
    if (simulate) {
      // Đồng hồ nhạc chạy theo thời gian thật khi chạy app với Fake.
      final now = _clock.elapsedMicroseconds;
      if (_lastSimMicros > 0) {
        final dt = (now - _lastSimMicros) / 1e6;
        _tickCapture(dt);
        if (jamRunning) _jamElapsed += dt;
        session.advanceSeconds(dt);
      }
      _lastSimMicros = now;
      _simulate();
    }
    if (_sessionTouched || simulate) {
      session.publish(_state.ref, meters: simulate);
      if (!simulate) _state.ref.publishCounter = (_state.ref.publishCounter + 1) & 0xFFFFFFFF;
    }
    return _state.ref;
  }

  static const _sessionOps = {
    'clip.setParams',
    'midi.setRecordQuantize',
    'track.configure',
    'clip.getMidi',
    'midiClip.quantize',
    'clip.setLoopRegion',
    'project.open',
    'project.close',
    'transport.setTimeSignature',
    'transport.setTempoMode',
    'clip.setMidi',
    'clip.setAudio',
    'clip.clear',
    'clip.info',
    'clip.undoOverdub',
  };

  /// Op cấu trúc tác động lên [session]. Trả null để đi tiếp nhánh mặc định (ví dụ job của clip.setAudio).
  Map<String, dynamic>? _sessionCall(String op, Map<String, dynamic> r) {
    int? idx(String k, int max) {
      final v = r[k];
      return v is int && v >= 0 && v < max ? v : null;
    }

    switch (op) {
      case 'track.configure':
        final t = idx('track', LE_MAX_TRACKS);
        if (t == null) return _err('INVALID_ARG', 'track');
        final color = r['color'];
        if (color != null && (color is! String || !RegExp(r'^#[0-9A-Fa-f]{6}$').hasMatch(color))) {
          return _err('INVALID_ARG', 'color "#RRGGBB"'); // như engine (P4-05)
        }
        if (color is String) trackColors[t] = color;
        session.trackKind[t] = r['kind'] == 'instrument' ? 'instrument' : 'audio';
        return _ok(const {});
      case 'clip.setParams':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        final gain = r['gainDb'], warp = r['warp'];
        if (t == null || s == null) return _err('INVALID_ARG', 'track/slot');
        if (gain == null && warp == null) return _err('INVALID_ARG', 'cần gainDb hoặc warp');
        if (gain != null && (gain is! num || gain < -120 || gain > 24)) return _err('INVALID_ARG', 'gainDb −120..24');
        if (warp != null && warp != 'stretch' && warp != 'repitch') return _err('INVALID_ARG', 'warp');
        final ok = session.setParams(t, s, gainDb: (gain as num?)?.toDouble(), warp: warp as String?);
        return ok ? _ok(const {}) : _err('INVALID_ARG', 'ô không có clip audio');
      case 'midi.setRecordQuantize':
        final grid = r['grid'];
        if (grid is! num || !const [0, 0.25, 0.5].contains(grid)) return _err('INVALID_ARG', 'grid: 0|0.25|0.5');
        session.recordQuantize = grid.toDouble();
        return _ok(const {});
      case 'clip.getMidi':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        if (t == null || s == null) return _err('INVALID_ARG', 'track/slot');
        return _ok({'notes': session.midiNotes(t, s)});
      case 'midiClip.quantize':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        final grid = r['grid'];
        if (t == null || s == null || grid is! num || grid <= 0) return _err('INVALID_ARG', 'track/slot/grid');
        if (session.kindOf(t, s) != 'midi') return _err('INVALID_ARG', 'ô không có clip MIDI');
        if (grid > (session.lengthOf(t, s) ?? 0)) return _err('INVALID_ARG', 'grid > lengthBeats');
        session.quantizeNotes(t, s, grid.toDouble());
        return _ok(const {});
      case 'clip.setLoopRegion':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        if (t == null || s == null || r['startSample'] is! int || r['lengthBeats'] is! num) {
          return _err('INVALID_ARG', 'track/slot/startSample/lengthBeats');
        }
        return _ok(const {});
      case 'project.open' || 'project.close':
        _projectDir = op == 'project.open' ? r['dir'] as String? : null;
        session.reset();
        // Như engine: project mới không mang FX/nhạc cụ/master của project cũ (replay chỉ gửi giá trị khác mặc định).
        fx.clear();
        instruments.clear();
        envelopes.clear();
        midiMappings = const [];
        master
          ..clear()
          ..addAll(_masterDefaults);
        masterEqBypass = false;
        return _ok(const {});
      case 'transport.setTempoMode':
        final mode = r['mode'];
        if (mode != 'fixed' && mode != 'firstLoop') return _err('INVALID_ARG', 'mode: fixed|firstLoop');
        final flb = r['firstLoopBeats'];
        if (flb != null && (flb is! num || flb <= 0)) return _err('INVALID_ARG', 'firstLoopBeats > 0');
        final has = session.setTempoMode(mode as String, firstLoopBeats: (flb as num?)?.toDouble());
        return _ok({'hasTempo': has, 'firstLoopBeats': session.firstLoopBeats ?? 0.0}); // như engine: 0 = chưa biết
      case 'transport.setTimeSignature':
        final num = r['num'];
        if (num is! int || num < 1 || num > 16) return _err('INVALID_ARG', 'num 1..16');
        session.beatsPerBar = num;
        return _ok(const {});
      case 'clip.setMidi' || 'clip.setAudio':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        final len = r['lengthBeats'];
        if (t == null || s == null || len is! num) return _err('INVALID_ARG', 'track/slot/lengthBeats');
        session.setClip(
          t,
          s,
          clipId: '${r['clipId'] ?? ''}',
          kind: op == 'clip.setMidi' ? 'midi' : 'audio',
          lengthBeats: len.toDouble(),
          notes: [
            for (final n in (r['notes'] as List? ?? const []))
              {for (final e in (n as Map).entries) e.key as String: e.value as num},
          ],
          gainDb: (r['gainDb'] as num?)?.toDouble() ?? 0,
          warp: r['warp'] as String? ?? 'stretch',
        );
        return op == 'clip.setMidi' ? _ok(const {}) : null; // setAudio → job (decode)
      case 'clip.clear':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        if (t == null || s == null) return _err('INVALID_ARG', 'track/slot');
        session.clearClip(t, s);
        return _ok(const {});
      case 'clip.undoOverdub':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        if (t == null || s == null) return _err('INVALID_ARG', 'track/slot');
        return session.undoOverdub(t, s) ? _ok(const {}) : _err('INVALID_ARG', 'ô không có lớp overdub để hoàn tác');
      case 'clip.info':
        final t = idx('track', LE_MAX_TRACKS), s = idx('slot', LE_MAX_SCENES);
        if (t == null || s == null) return _err('INVALID_ARG', 'track/slot');
        return _ok(session.clipInfo(t, s) ?? const {'kind': 'empty'}); // như engine thật
    }
    return null;
  }

  /// Peaks giả (P2-20): 3 mức 256 / 2048 / 16384 sample/điểm như PeakBuilder (04 §5.7).
  /// Sóng "gõ theo phách" ổn định theo clipId để UI và test thấy hình giống nhau mỗi lần.
  @override
  Float32List? getPeaks(String clipId, int level, int maxPairs) {
    final c = session.findClip(clipId);
    if (c == null || c.kind != 'audio' || level < 0 || level > 2 || maxPairs <= 0) return null;
    peakRequests.add((clipId, level));
    const spp = [256, 2048, 16384];
    final sr = session.sampleRate;
    final total = (c.lengthBeats * 60 / session.bpm * sr).round();
    final n = math.min(maxPairs, (total / spp[level]).ceil());
    final seed = clipId.hashCode % 1000 / 1000.0;
    final out = Float32List(n * 2);
    for (var i = 0; i < n; i++) {
      final t = i * spp[level] / sr;
      final phase = (t * session.bpm / 60) % 1.0;
      final amp = (0.12 + 0.78 * math.exp(-phase * 5)) * (0.75 + 0.25 * math.sin(seed * 20 + i * 0.013));
      out[i * 2] = -amp * (0.85 + 0.15 * math.sin(i * 0.7));
      out[i * 2 + 1] = amp;
    }
    return out;
  }

  /// (clipId, level) của mỗi lần getPeaks — test kiểm cache waveform.
  final List<(String, int)> peakRequests = [];

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
    final mic = 0.08 + 0.06 * math.sin(t * 5.3).abs() + (_passthrough ? 0.2 : 0) + (_capRunning ? 0.45 : 0);
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
  String errorMessage = '';
  void Function()? onDone;
}

/// Số slot FX mỗi track (04 §9).
const maxFxSlots = 3;

/// Id tham số hợp lệ theo loại FX (04 §9).
const Map<String, Set<int>> fxParamIds = {
  'filter': {0, 1, 2},
  'delay': {0, 1, 2, 3},
  'reverb': {0, 1, 2, 3},
  'eq3': {0, 1, 2},
  'comp': {0, 1, 2, 3, 4},
};

/// Một slot FX trong [FakeEngineClient.fx].
final class FakeFx {
  FakeFx(this.type, this.params, {this.bypass = false});
  final String type;
  final Map<String, double> params;
  bool bypass;

  @override
  String toString() => 'FakeFx($type, $params${bypass ? ', bypass' : ''})';
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
