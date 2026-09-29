import 'package:engine_ffi/engine_ffi.dart';

/// Thông tin device + buffer (Settings → Audio / Giới thiệu). Quyền mic và bật/tắt audio ở `EngineAudio`.
class AudioDeviceService {
  AudioDeviceService(this._engine);

  final EngineApi _engine;

  int get apiVersion => _engine.apiVersion;

  /// `engine.info`; lỗi → map rỗng.
  Map<String, dynamic> info() {
    final r = _engine.call({'op': 'engine.info'});
    return r['ok'] == true ? (r['result'] as Map).cast<String, dynamic>() : const {};
  }

  /// Đổi buffer (tạm dùng `spike.setBufferSize`, đổi tên chính thức sau M0). Lỗi → [EngineCallException].
  void setBufferSize(int frames) => _engine.callOk('spike.setBufferSize', {'frames': frames});
}
