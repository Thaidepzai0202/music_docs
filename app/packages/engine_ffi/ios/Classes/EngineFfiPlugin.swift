import AVFAudio
import CoreAudioKit
import Flutter
import UIKit

/// Việc hiếm cần API iOS (07 §7): quyền mic, mở Settings, giữ màn hình sáng, đường dẫn bundle,
/// màn ghép Bluetooth MIDI (P4-03).
/// KHÔNG đụng AVAudioSession — JUCE sở hữu session (P0-06).
/// [main] FlutterMethodChannel gọi handle() trên main thread.
public class EngineFfiPlugin: NSObject, FlutterPlugin {
  /// Giữ presenter sống tới khi màn ghép Bluetooth MIDI đóng.
  private var bluetoothMidi: BluetoothMidiPresenter?

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

    case "setKeepScreenOn":
      // Màn hình luôn sáng khi transport chạy (07 §3.4). Không cần thêm package wakelock.
      UIApplication.shared.isIdleTimerDisabled = (call.arguments as? Bool) ?? false
      result(nil)

    case "assetPath":
      // Đường dẫn thật của một Flutter asset (thư mục thư viện âm thanh) để engine C++ đọc file.
      guard let asset = call.arguments as? String else {
        result(nil)
        return
      }
      let path = Bundle.main.bundlePath + "/" + FlutterDartProject.lookupKey(forAsset: asset)
      result(FileManager.default.fileExists(atPath: path) ? path : nil)

    case "bundleResourcePath":
      result(Bundle.main.resourcePath)

    case "showBluetoothMidi":
      // CABTMIDICentralViewController: quét + ghép thiết bị BLE MIDI. Ghép xong CoreMIDI báo thiết bị mới →
      // engine phát MIDI_DEVICES. result() khi người dùng đóng màn (Xong hoặc vuốt xuống).
      guard bluetoothMidi == nil, let top = Self.topViewController() else {
        result(false)
        return
      }
      let presenter = BluetoothMidiPresenter { [weak self] in
        self?.bluetoothMidi = nil
        result(true)
      }
      bluetoothMidi = presenter
      presenter.present(from: top)

    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private static func topViewController() -> UIViewController? {
    let window = UIApplication.shared.connectedScenes
      .compactMap { $0 as? UIWindowScene }
      .flatMap { $0.windows }
      .first { $0.isKeyWindow }
    var top = window?.rootViewController
    while let presented = top?.presentedViewController { top = presented }
    return top
  }

  private static func name(of p: AVAudioApplication.recordPermission) -> String {
    switch p {
    case .granted: return "granted"
    case .denied: return "denied"
    default: return "undetermined"
    }
  }
}

/// Trình bày `CABTMIDICentralViewController` trong navigation controller (formSheet trên iPad) có nút Xong.
private final class BluetoothMidiPresenter: NSObject, UIAdaptivePresentationControllerDelegate {
  init(onClose: @escaping () -> Void) {
    self.onClose = onClose
  }

  private let onClose: () -> Void
  private weak var nav: UINavigationController?

  func present(from host: UIViewController) {
    let central = CABTMIDICentralViewController()
    central.navigationItem.rightBarButtonItem = UIBarButtonItem(
      barButtonSystemItem: .done, target: self, action: #selector(done))
    let nav = UINavigationController(rootViewController: central)
    nav.modalPresentationStyle = .formSheet
    nav.presentationController?.delegate = self
    self.nav = nav
    host.present(nav, animated: true)
  }

  @objc private func done() {
    nav?.dismiss(animated: true) { [onClose] in onClose() }
  }

  // Vuốt xuống để đóng.
  func presentationControllerDidDismiss(_ presentationController: UIPresentationController) {
    onClose()
  }
}
