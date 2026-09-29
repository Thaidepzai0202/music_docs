import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'engine_event.dart';
import 'loopcore_bindings.g.dart';

/// Giao diện chung của engine phía Dart (05 §4).
///
/// `EngineClient` gọi C thật; `FakeEngineClient` ghi lại lệnh để test và để chạy UI khi
/// chưa có `LoopCore.xcframework`. Mọi method gọi trên main isolate.
abstract interface class EngineApi {
  /// `le_api_version()` của engine đang link.
  int get apiVersion;

  /// Event bất đồng bộ từ engine (broadcast).
  Stream<EngineEvent> get events;

  /// `le_create`. Trả `LeError` (0 = OK).
  int create(EngineConfig config);

  /// `le_audio_start`. Gọi SAU khi đã có quyền mic. Trả `LeError`.
  int audioStart();

  /// `le_audio_stop`.
  void audioStop();

  /// Lệnh RT (đường nhanh) qua `le_send`. `false` = queue đầy hoặc lệnh không hợp lệ.
  bool send(int type, {int track = -1, int slot = -1, int i0 = 0, double f0 = 0, double f1 = 0, double d0 = 0});

  /// Lệnh cấu trúc qua `le_call`. Luôn trả envelope `{ok, result | error}` (05 §3).
  Map<String, dynamic> call(Map<String, dynamic> request);

  /// Bản state mới nhất. Là view của struct native dùng lại mỗi frame:
  /// đọc ngay trong frame đó, KHÔNG giữ tham chiếu qua frame sau.
  LeState readState();

  /// `le_get_peaks`: tối đa [maxPairs] cặp min/max. `null` nếu engine báo lỗi.
  Float32List? getPeaks(String clipId, int level, int maxPairs);

  void dispose();
}

/// Tham số khởi tạo engine (`LeConfig`).
final class EngineConfig {
  const EngineConfig({
    required this.dataDir,
    required this.libraryDir,
    this.preferredBufferSize = 128,
    this.preferredSampleRate = 48000,
    this.numInputChannels = 1,
  });

  final String dataDir;
  final String libraryDir;
  final int preferredBufferSize;
  final double preferredSampleRate;
  final int numInputChannels;

  /// Cấp phát `LeConfig` + chuỗi UTF-8 trong [arena]; hết scope arena thì tự giải phóng.
  /// Arena mặc định dùng calloc nên `_reserved` = 0.
  Pointer<LeConfig> toNative(Arena arena) {
    final p = arena<LeConfig>();
    p.ref
      ..apiVersion = LE_API_VERSION
      ..preferredBufferSize = preferredBufferSize
      ..preferredSampleRate = preferredSampleRate
      ..numInputChannels = numInputChannels
      ..dataDir = dataDir.toNativeUtf8(allocator: arena).cast()
      ..libraryDir = libraryDir.toNativeUtf8(allocator: arena).cast();
    return p;
  }
}

/// Lỗi trả về trong envelope `{ok:false, error:{code, message}}`.
final class EngineCallException implements Exception {
  const EngineCallException(this.op, this.code, this.message);
  final String op;

  /// Mã chuỗi, ví dụ `FILE_NOT_FOUND`, `NOT_IMPLEMENTED`.
  final String code;
  final String message;
  @override
  String toString() => 'EngineCallException($op: $code${message.isEmpty ? '' : ' — $message'})';
}

extension EngineCallX on EngineApi {
  /// Gọi [op] và trả `result`. Envelope lỗi → ném [EngineCallException].
  Map<String, dynamic> callOk(String op, [Map<String, dynamic> params = const {}]) {
    final res = call({'op': op, ...params});
    if (res['ok'] == true) {
      return (res['result'] as Map?)?.cast<String, dynamic>() ?? const {};
    }
    final err = (res['error'] as Map?)?.cast<String, dynamic>() ?? const {};
    throw EngineCallException(op, '${err['code'] ?? 'INTERNAL'}', '${err['message'] ?? ''}');
  }

  /// Gọi op bất đồng bộ, trả `jobId` (theo dõi bằng `JobTracker`).
  int callJob(String op, [Map<String, dynamic> params = const {}]) {
    final result = callOk(op, params);
    final jobId = result['jobId'];
    if (jobId is! int) {
      throw EngineCallException(op, 'INTERNAL', 'thiếu jobId trong result: $result');
    }
    return jobId;
  }
}

/// Tên đọc được của `LeError` (log, banner, test).
String leErrorName(int code) => switch (code) {
  LeError.LE_OK => 'OK',
  LeError.LE_ERR_INVALID_ARG => 'INVALID_ARG',
  LeError.LE_ERR_NOT_CREATED => 'NOT_CREATED',
  LeError.LE_ERR_ALREADY_CREATED => 'ALREADY_CREATED',
  LeError.LE_ERR_API_VERSION => 'API_VERSION',
  LeError.LE_ERR_NOT_IMPLEMENTED => 'NOT_IMPLEMENTED',
  LeError.LE_ERR_AUDIO_DEVICE => 'AUDIO_DEVICE',
  LeError.LE_ERR_MIC_PERMISSION => 'MIC_PERMISSION',
  LeError.LE_ERR_FILE_NOT_FOUND => 'FILE_NOT_FOUND',
  LeError.LE_ERR_FILE_FORMAT => 'FILE_FORMAT',
  LeError.LE_ERR_DISK_FULL => 'DISK_FULL',
  LeError.LE_ERR_OUT_OF_MEMORY => 'OUT_OF_MEMORY',
  LeError.LE_ERR_QUEUE_FULL => 'QUEUE_FULL',
  LeError.LE_ERR_JOB_CANCELLED => 'JOB_CANCELLED',
  LeError.LE_ERR_JOB_NOT_FOUND => 'JOB_NOT_FOUND',
  LeError.LE_ERR_PITCH_NOT_DETECTED => 'PITCH_NOT_DETECTED',
  LeError.LE_ERR_OVERDUB_UNSUPPORTED => 'OVERDUB_UNSUPPORTED',
  LeError.LE_ERR_INTERNAL => 'INTERNAL',
  _ => 'ERROR($code)',
};
