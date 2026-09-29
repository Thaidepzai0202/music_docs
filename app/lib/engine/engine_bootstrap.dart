import 'package:engine_ffi/engine_ffi.dart';
import 'package:path_provider/path_provider.dart';

/// Kết quả khởi tạo engine lúc mở app.
final class EngineBootstrap {
  const EngineBootstrap({required this.engine, required this.isFake, required this.createResult, this.dataDir = ''});

  final EngineApi engine;

  /// Documents/ của app (đã truyền vào `LeConfig.dataDir`).
  final String dataDir;

  /// true = chưa có LoopCore.xcframework trong binary → đang dùng [FakeEngineClient] giả lập.
  final bool isFake;

  /// `LeError` của `le_create` (0 = OK).
  final int createResult;

  bool get ok => createResult == LeError.LE_OK;
}

/// Chọn engine thật nếu binary có symbol `le_*`, không thì dùng Fake (simulate) để UI vẫn chạy.
/// Gọi `le_create` với Documents/ và `<bundle>/Library` (06 §1). Chưa start audio: phải xin quyền mic trước.
Future<EngineBootstrap> bootstrapEngine({EnginePlatform platform = const EnginePlatform()}) async {
  final real = EngineClient.tryOpen();
  final EngineApi engine = real ?? FakeEngineClient(simulate: true);

  final docs = await getApplicationDocumentsDirectory();
  String? resources;
  try {
    resources = await platform.bundleResourcePath();
  } on Exception {
    resources = null;
  }
  final rc = engine.create(EngineConfig(dataDir: docs.path, libraryDir: resources == null ? '' : '$resources/Library'));
  return EngineBootstrap(engine: engine, isFake: real == null, createResult: rc, dataDir: docs.path);
}
