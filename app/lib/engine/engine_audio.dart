import 'package:engine_ffi/engine_ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

enum AudioStartStatus { started, micDenied, engineError }

final class AudioStartResult {
  const AudioStartResult(this.status, [this.errorCode = LeError.LE_OK]);
  final AudioStartStatus status;

  /// `LeError` khi [status] = engineError.
  final int errorCode;
  bool get ok => status == AudioStartStatus.started;
}

/// Bật/tắt audio device của engine.
///
/// P0-06: luôn xin quyền mic (Swift `AVAudioApplication.requestRecordPermission`) **trước**
/// `le_audio_start`. P2-03: xuống nền khi không phát thì tắt, lên lại thì bật.
/// Tần suất thấp → ChangeNotifier bình thường là đủ.
class EngineAudio extends ChangeNotifier {
  EngineAudio(this._engine, this._platform);

  final EngineApi _engine;
  final EnginePlatform _platform;
  bool _running = false;
  bool _suspendedByBackground = false;

  bool get isRunning => _running;

  /// Giữ audio khi app xuống nền dù transport không chạy (màn spike: sine/loop phải kêu tiếp
  /// để test P0-10 "về Home vẫn phát").
  bool keepRunningInBackground = false;

  Future<AudioStartResult> start() async {
    if (_running) return const AudioStartResult(AudioStartStatus.started);
    if (!await _ensureMicPermission()) return const AudioStartResult(AudioStartStatus.micDenied);
    final rc = _engine.audioStart();
    if (rc != LeError.LE_OK) {
      return AudioStartResult(
        rc == LeError.LE_ERR_MIC_PERMISSION ? AudioStartStatus.micDenied : AudioStartStatus.engineError,
        rc,
      );
    }
    _running = true;
    notifyListeners();
    return const AudioStartResult(AudioStartStatus.started);
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

  /// App lên lại: chỉ bật lại nếu chính [onBackground] đã tắt (quyền mic đã có từ trước).
  void onForeground() {
    if (!_suspendedByBackground) return;
    _suspendedByBackground = false;
    if (_engine.audioStart() == LeError.LE_OK) {
      _running = true;
      notifyListeners();
    }
  }

  Future<bool> _ensureMicPermission() async {
    try {
      var p = await _platform.micPermission();
      if (p == MicPermission.undetermined) {
        p = await _platform.requestMicPermission() ? MicPermission.granted : MicPermission.denied;
      }
      return p == MicPermission.granted;
    } on MissingPluginException {
      return false; // không chạy trên iOS (plugin Swift không có)
    } on PlatformException {
      return false;
    }
  }
}
