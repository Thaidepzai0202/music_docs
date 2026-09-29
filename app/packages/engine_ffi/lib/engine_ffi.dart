/// Cầu nối Flutter ↔ LoopCore (05). Dùng [EngineApi]; chọn [EngineClient] (thật) hoặc
/// [FakeEngineClient] (test, hoặc khi chưa có LoopCore.xcframework).
library;

export 'src/engine_api.dart';
export 'src/engine_client.dart';
export 'src/engine_event.dart';
export 'src/engine_platform.dart';
export 'src/fake_engine_client.dart';
export 'src/loopcore_bindings.g.dart'
    show
        LE_API_VERSION,
        LE_MAX_SCENES,
        LE_MAX_TRACKS,
        LeClipState,
        LeCommand,
        LeCommandType,
        LeError,
        LeEventType,
        LeQuantize,
        LeState;
