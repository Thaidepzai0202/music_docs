import 'package:engine_ffi/engine_ffi.dart';

/// Trạng thái Ableton Link đọc từ engine (bật/tắt lưu trong project → `ProjectController.setLink`).
class LinkService {
  LinkService(this._engine);

  final EngineApi _engine;

  int get peers => _engine.readState().linkPeers;

  Stream<int> get peersChanged => _engine.events.whereType<LinkPeersChanged>().map((e) => e.peers);
}

extension on Stream<EngineEvent> {
  Stream<T> whereType<T>() => where((e) => e is T).cast<T>();
}
