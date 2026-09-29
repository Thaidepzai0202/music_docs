import AVFAudio
import Flutter
import UIKit

/// Việc hiếm cần API iOS (07 §7): quyền mic, mở Settings, đường dẫn bundle.
/// KHÔNG đụng AVAudioSession — JUCE sở hữu session (P0-06).
/// [main] FlutterMethodChannel gọi handle() trên main thread.
public class EngineFfiPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "loopcore/engine_ffi", binaryMessenger: registrar.messenger())
    registrar.addMethodCallDelegate(EngineFfiPlugin(), channel: channel)
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "micPermission":
      result(Self.name(of: AVAudioApplication.shared.recordPermission))

    case "requestMicPermission":
      // Chỉ hiện hộp thoại ở lần đầu; đã từ chối thì trả false ngay → Dart hướng dẫn mở Settings.
      AVAudioApplication.requestRecordPermission { granted in
        DispatchQueue.main.async { result(granted) }
      }

    case "openAppSettings":
      guard let url = URL(string: UIApplication.openSettingsURLString) else {
        result(false)
        return
      }
      UIApplication.shared.open(url, options: [:]) { ok in result(ok) }

    case "bundleResourcePath":
      result(Bundle.main.resourcePath)

    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private static func name(of p: AVAudioApplication.recordPermission) -> String {
    switch p {
    case .granted: return "granted"
    case .denied: return "denied"
    default: return "undetermined"
    }
  }
}
