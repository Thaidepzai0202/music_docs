import 'dart:async';
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';

import 'engine_api.dart';
import 'engine_event.dart';
import 'loopcore_bindings.g.dart';

/// Kiểu native của `LeEventCallback` (ffigen chỉ sinh kiểu con trỏ, không sinh typedef hàm).
typedef LeEventCallbackFunction = Void Function(Int32 type, Int32 a, Int32 b, Int64 jobId, Double value);

/// Engine thật qua dart:ffi (05 §4). Một instance duy nhất cho toàn app, gọi trên main isolate.
///
/// Từ Flutter 3.29 Dart chạy trên main thread của iOS, nên gọi `le_*` thẳng ở đây là đúng
/// quy tắc "mọi hàm le_* gọi từ main thread" (05 §1).
final class EngineClient implements EngineApi {
  EngineClient._(this._b);

  /// Chỉ dùng trong test: bindings giả dựng bằng `LoopCoreBindings.fromLookup`.
  @visibleForTesting
  EngineClient.withBindings(LoopCoreBindings bindings) : this._(bindings);

  final LoopCoreBindings _b;

  // Cấp phát 1 lần, dùng lại mỗi frame/mỗi lệnh → không cấp phát trên đường nóng.
  final Pointer<LeState> _state = calloc<LeState>();
  final Pointer<LeCommand> _cmd = calloc<LeCommand>();
  NativeCallable<LeEventCallbackFunction>? _callback;
  final _events = StreamController<EngineEvent>.broadcast();
  bool _disposed = false;

  /// iOS link tĩnh LoopCore.xcframework → symbol nằm trong process.
  static EngineClient open() => EngineClient._(LoopCoreBindings(DynamicLibrary.process()));

  /// Như [open] nhưng trả `null` nếu binary không có symbol `le_*`
  /// (chưa có XCFramework, hoặc đang chạy test/simulator không có engine).
  static EngineClient? tryOpen() {
    try {
      final client = open();
      client.apiVersion; // lookup thử 1 symbol
      return client;
    } on ArgumentError {
      return null;
    }
  }

  @override
  int get apiVersion => _b.le_api_version();

  @override
  Stream<EngineEvent> get events => _events.stream;

  @override
  int create(EngineConfig config) {
    _callback?.close();
    // listener: engine gọi từ thread bất kỳ, Dart nhận bất đồng bộ trên main isolate.
    // Tham số truyền theo giá trị nên không lo con trỏ hết hạn (05 §1.4).
    _callback = NativeCallable<LeEventCallbackFunction>.listener(_onEvent);
    _b.le_set_event_callback(_callback!.nativeFunction);
    return using((arena) => _b.le_create(config.toNative(arena)));
  }

  @override
  int audioStart() => _b.le_audio_start();

  @override
  void audioStop() => _b.le_audio_stop();

  @override
  bool send(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double f1 = 0, double d0 = 0}) {
    _cmd.ref
      ..type = type
      ..track = track
      ..slot = slot
      ..i0 = i0
      ..f0 = f0
      ..f1 = f1
      ..d0 = d0
      ..hostTimeNs = 0;
    return _b.le_send(_cmd); // engine copy ngay nên dùng lại _cmd được
  }

  @override
  Map<String, dynamic> call(Map<String, dynamic> request) {
    final req = jsonEncode(request).toNativeUtf8();
    try {
      final res = _b.le_call(req.cast());
      if (res == nullptr) {
        return _errorEnvelope('INTERNAL', 'le_call trả NULL');
      }
      String text;
      try {
        text = res.cast<Utf8>().toDartString();
      } finally {
        _b.le_free_string(res); // chuỗi do engine cấp phát → engine giải phóng
      }
      try {
        return (jsonDecode(text) as Map).cast<String, dynamic>();
      } on FormatException catch (e) {
        return _errorEnvelope('INTERNAL', 'JSON engine trả về không hợp lệ: ${e.message}');
      }
    } finally {
      malloc.free(req);
    }
  }

  /// Gọi mỗi frame từ Ticker. Trả về view của struct native: đọc ngay, không giữ lâu.
  @override
  LeState readState() {
    _b.le_read_state(_state);
    return _state.ref;
  }

  @override
  Float32List? getPeaks(String clipId, int level, int maxPairs) {
    return using((arena) {
      final out = arena<Float>(maxPairs * 2);
      final n = _b.le_get_peaks(clipId.toNativeUtf8(allocator: arena).cast(), level, out, maxPairs);
      if (n < 0) return null;
      return Float32List.fromList(out.asTypedList(n * 2));
    });
  }

  void _onEvent(int type, int a, int b, int jobId, double value) {
    if (_disposed) return;
    _events.add(EngineEvent.fromNative(type, a, b, jobId, value));
  }

  @override
  void dispose() {
    if (_disposed) return;
    _disposed = true;
    _b.le_set_event_callback(nullptr);
    _callback?.close();
    _callback = null;
    _b.le_destroy();
    calloc
      ..free(_state)
      ..free(_cmd);
    _events.close();
  }

  static Map<String, dynamic> _errorEnvelope(String code, String message) => {
    'ok': false,
    'error': {'code': code, 'message': message},
  };
}
