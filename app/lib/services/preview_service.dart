import 'package:engine_ffi/engine_ffi.dart';

import '../data/library_repository.dart';
import '../model/project.dart';

/// Nghe thử trong Browser (07 §4.1e, 05 §3 `preview.play` / `preview.stop`): kênh preview riêng của engine, không đụng
/// track hay transport; gọi lần nữa thì thay bản đang nghe. Nghe thử là phụ: engine chưa hỗ trợ hoặc lỗi thì bỏ qua,
/// không làm phiền người dùng (trả false).
class PreviewService {
  PreviewService(this._engine);

  final EngineApi _engine;

  /// Kit (engine phát groove ngắn) / nhạc cụ (câu ngắn) / loop — đường dẫn theo `libraryDir` (`base` mặc định).
  bool playLibrary(LibraryItem it) => _call('preview.play', {
    'source': switch (it.kind) {
      LibraryKind.loop => {'kind': 'audio', 'file': it.path},
      LibraryKind.kit || LibraryKind.instrument => {'kind': 'sfz', 'path': it.path},
    },
  });

  /// Nhạc cụ tự thu ("Bản thu của tôi"): nghe file nguồn trong thư mục project.
  bool playUser(UserInstrument u) => _call('preview.play', {
    'source': {'kind': 'audio', 'file': u.source, 'base': 'project'},
  });

  bool stop() => _call('preview.stop');

  bool _call(String op, [Map<String, dynamic> params = const {}]) {
    try {
      _engine.callOk(op, params);
      return true;
    } on EngineCallException {
      return false;
    }
  }
}
