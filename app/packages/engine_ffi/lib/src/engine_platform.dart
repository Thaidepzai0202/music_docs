import 'package:flutter/services.dart';

/// Trạng thái quyền micro (`AVAudioApplication.recordPermission`).
enum MicPermission { undetermined, denied, granted }

/// Việc hiếm, không nằm trên đường nóng → đi qua MethodChannel tới Swift trong plugin (07 §7).
///
/// Chỉ xin quyền, KHÔNG đụng AVAudioSession: session do JUCE quản lý (P0-06).
class EnginePlatform {
  const EnginePlatform();

  static const channel = MethodChannel('loopcore/engine_ffi');

  Future<MicPermission> micPermission() async {
    final s = await channel.invokeMethod<String>('micPermission');
    return switch (s) {
      'granted' => MicPermission.granted,
      'denied' => MicPermission.denied,
      _ => MicPermission.undetermined,
    };
  }

  /// Hiện hộp thoại hệ thống (chỉ lần đầu). `true` nếu được cấp.
  Future<bool> requestMicPermission() async => await channel.invokeMethod<bool>('requestMicPermission') ?? false;

  /// Mở trang Settings của app (khi người dùng đã từ chối quyền mic).
  Future<bool> openAppSettings() async => await channel.invokeMethod<bool>('openAppSettings') ?? false;

  /// Giữ màn hình sáng (tắt idle timer) khi đang phát — 07 §3.4.
  Future<void> setKeepScreenOn(bool on) async {
    try {
      await channel.invokeMethod<void>('setKeepScreenOn', on);
    } on MissingPluginException {
      // Không chạy trên iOS (test) — bỏ qua.
    }
  }

  /// Đường dẫn tuyệt đối của Flutter asset [asset] (ví dụ `assets/library`), null nếu không có.
  Future<String?> assetPath(String asset) async {
    try {
      return await channel.invokeMethod<String>('assetPath', asset);
    } on MissingPluginException {
      return null;
    }
  }

  /// Mở màn ghép Bluetooth MIDI của iOS (`CABTMIDICentralViewController`, P4-03); xong khi người dùng đóng màn.
  /// Không chạy trên iOS (test) → bỏ qua.
  Future<void> showBluetoothMidi() async {
    try {
      await channel.invokeMethod<bool>('showBluetoothMidi');
    } on MissingPluginException {
      // test / không phải iOS
    }
  }

  /// `Bundle.main.resourcePath` — gốc của thư viện âm thanh đóng gói (06 §1).
  Future<String?> bundleResourcePath() => channel.invokeMethod<String>('bundleResourcePath');
}
