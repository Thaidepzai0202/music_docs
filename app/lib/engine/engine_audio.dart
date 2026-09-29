import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

enum AudioStartStatus {
  /// Audio chạy, có input (thu được).
  started,

  /// Audio chạy ở chế độ CHỈ PHÁT: người dùng chưa cho quyền mic (07 §4.0, `audio.setInputEnabled false`).
  outputOnly,

  engineError,
}

final class AudioStartResult {
  const AudioStartResult(this.status, [this.errorCode = LeError.LE_OK]);
  final AudioStartStatus status;

  /// `LeError` khi [status] = engineError.
  final int errorCode;

  /// Audio đang chạy (có hoặc không có input).
  bool get ok => status != AudioStartStatus.engineError;

  /// Thu được (có quyền mic, input bật).
  bool get canRecord => status == AudioStartStatus.started;
}

/// Bật/tắt audio device của engine.
///
/// 07 §4.0 (chốt 29/09): [start] KHÔNG hỏi quyền mic. Chưa có quyền (chưa hỏi hoặc bị từ chối) thì chạy chế độ
/// chỉ phát (`audio.setInputEnabled {enabled:false}`). Chỉ hỏi (hộp thoại iOS) khi người dùng làm việc cần micro
/// lần đầu — [ensureMic]: arm track audio, nút LOOP trên track audio, Thu âm mới, đo trễ. Được quyền thì bật input
/// rồi làm tiếp thao tác dở. P2-03: xuống nền khi không phát thì tắt, lên lại thì bật.
/// Tần suất thấp → ChangeNotifier bình thường là đủ.
class EngineAudio extends ChangeNotifier {
  EngineAudio(this._engine, this._platform);

  final EngineApi _engine;
  final EnginePlatform _platform;
  bool _running = false;
  bool _suspendedByBackground = false;
  bool _inputEnabled = true;

  bool get isRunning => _running;

  /// Input (mic) đang bật. false = chế độ chỉ phát.
  bool get inputEnabled => _inputEnabled;

  /// Audio chạy nhưng không thu được vì thiếu quyền mic → hiện banner + khoá nút thu (07 §4.0).
  bool get outputOnly => _running && !_inputEnabled;

  MicPermission _permission = MicPermission.undetermined;

  /// Quyền mic lần kiểm gần nhất. `undetermined` → nút thu vẫn bấm được (bấm thì hỏi); `denied` → khoá + "Mở Cài đặt".
  MicPermission get micPermission => _permission;

  /// Thu audio được NGAY (đường nóng, không phải chờ hỏi quyền).
  bool get canRecordNow => _running && _inputEnabled;

  /// Giữ audio khi app xuống nền dù transport không chạy (màn spike: sine/loop phải kêu tiếp
  /// để test P0-10 "về Home vẫn phát").
  bool keepRunningInBackground = false;

  Future<AudioStartResult> start() async {
    if (_running) return AudioStartResult(_inputEnabled ? AudioStartStatus.started : AudioStartStatus.outputOnly);
    _permission = await _currentPermission(); // không hỏi: chưa có quyền → chỉ phát
    final mic = _permission == MicPermission.granted;
    if (mic != _inputEnabled) _setInput(mic); // tắt input TRƯỚC khi mở device → category Playback
    final rc = _engine.audioStart();
    if (rc != LeError.LE_OK) return AudioStartResult(AudioStartStatus.engineError, rc);
    _running = true;
    notifyListeners();
    return AudioStartResult(mic ? AudioStartStatus.started : AudioStartStatus.outputOnly);
  }

  /// Việc sắp làm cần micro: chưa hỏi thì hỏi (chỉ lần đầu). Được quyền → bật input (và audio nếu chưa chạy),
  /// trả true để làm tiếp. Bị từ chối → false (UI giữ khoá + banner "Mở Cài đặt").
  Future<bool> ensureMic() async {
    if (canRecordNow) return true;
    var p = await _currentPermission();
    if (p == MicPermission.undetermined) {
      try {
        p = await _platform.requestMicPermission() ? MicPermission.granted : MicPermission.denied;
      } on MissingPluginException {
        p = MicPermission.denied;
      } on PlatformException {
        p = MicPermission.denied;
      }
    }
    _permission = p;
    if (p != MicPermission.granted) {
      notifyListeners();
      return false;
    }
    if (!_inputEnabled) _setInput(true);
    if (!_running && _engine.audioStart() == LeError.LE_OK) _running = true;
    notifyListeners();
    return _running;
  }

  void stop() {
    _suspendedByBackground = false;
    if (!_running) return;
    _engine.audioStop();
    _running = false;
    notifyListeners();
  }

  Future<bool> openAppSettings() => _platform.openAppSettings();

  /// App xuống nền. [engineBusy] = transport đang chạy hoặc đang thu → giữ audio.
  void onBackground({required bool engineBusy}) {
    if (!_running || engineBusy || keepRunningInBackground) return;
    _engine.audioStop();
    _running = false;
    _suspendedByBackground = true;
    notifyListeners();
  }

  /// App lên lại: bật lại audio nếu chính [onBackground] đã tắt; đang chỉ phát mà người dùng đã cấp quyền
  /// trong Cài đặt → bật lại input (07 §4.0).
  Future<void> onForeground() async {
    if (_suspendedByBackground) {
      _suspendedByBackground = false;
      if (_engine.audioStart() == LeError.LE_OK) {
        _running = true;
        notifyListeners();
      }
    }
    await refreshInput();
  }

  /// Kiểm lại quyền mic (không hỏi lại). Đã có quyền mà input đang tắt → bật lại.
  Future<void> refreshInput() async {
    if (_inputEnabled) return;
    final p = await _currentPermission();
    final changed = p != _permission;
    _permission = p;
    if (p == MicPermission.granted) {
      _setInput(true);
      notifyListeners();
    } else if (changed) {
      notifyListeners();
    }
  }

  /// `audio.setInputEnabled`. Engine chưa có op (NOT_IMPLEMENTED) thì vẫn ghi cờ: cờ phản ánh QUYỀN mic,
  /// UI dựa vào nó để khoá nút thu.
  void _setInput(bool enabled) {
    _engine.call({'op': 'audio.setInputEnabled', 'enabled': enabled});
    _inputEnabled = enabled;
  }

  Future<MicPermission> _currentPermission() async {
    try {
      return await _platform.micPermission();
    } on MissingPluginException {
      return MicPermission.denied;
    } on PlatformException {
      return MicPermission.denied;
    }
  }
}
