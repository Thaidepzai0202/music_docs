# 05 — FFI bridge: hợp đồng giữa Flutter và engine

> Đây là **hợp đồng**. Engine (C++) và app (Dart) được phát triển song song dựa trên file này.
> File header thật nằm ở `engine/include/le/engine_api.h`. Khi đổi header thì **phải cập nhật file này** và tăng `LE_API_VERSION` nếu thay đổi làm vỡ tương thích.

---

## 1. Quy tắc

1. Mọi hàm `le_*` được gọi từ **main thread**. Từ Flutter 3.29, Dart chạy trên main thread của iOS, nên FFI gọi thẳng là đúng. Ngoại lệ duy nhất là `le_read_state`, gọi được từ thread bất kỳ.
2. Mỗi hàm `le_*` phải trả về trong **< 1ms**. Việc gì lâu hơn phải thành **job** (trả `jobId` ngay).
3. Không truyền con trỏ tới object Dart sang C. Chuỗi là UTF-8. Chuỗi do engine trả về phải được giải phóng bằng `le_free_string`.
4. Callback event dùng **tham số truyền theo giá trị** (không truyền con trỏ), vì `NativeCallable.listener` là bất đồng bộ và con trỏ sẽ không còn hợp lệ khi Dart xử lý.
5. Symbol được export bằng `LE_EXPORT` (visibility default + `used`). Ở phía iOS link bằng `-force_load` để linker không strip (09 §5).

---

## 2. Header `engine_api.h` (v1)

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LE_API_VERSION 1
#define LE_MAX_TRACKS  8
#define LE_MAX_SCENES  8

#if defined(__GNUC__) || defined(__clang__)
#  define LE_EXPORT __attribute__((visibility("default"))) __attribute__((used))
#else
#  define LE_EXPORT
#endif

/* ───────────── Cấu hình ───────────── */
typedef struct LeConfig {
    int32_t     apiVersion;           /* = LE_API_VERSION, engine từ chối nếu lệch */
    int32_t     preferredBufferSize;  /* 128 */
    double      preferredSampleRate;  /* 48000 */
    int32_t     numInputChannels;     /* 1 (mic) hoặc 2 (interface) */
    int32_t     _reserved;
    const char* dataDir;              /* Documents/ của app, UTF-8 */
    const char* libraryDir;           /* thư mục thư viện âm thanh đóng gói */
} LeConfig;

/* ───────────── Lệnh RT (đường nhanh) ───────────── */
typedef struct LeCommand {            /* 32 byte */
    uint16_t type;                    /* LeCommandType */
    int8_t   track;                   /* 0..7, -1 = không áp dụng / master */
    int8_t   slot;                    /* 0..7 (scene hoặc FX slot), -1 = không áp dụng */
    int32_t  i0;
    float    f0;
    float    f1;
    double   d0;
    int64_t  hostTimeNs;              /* 0 = xử lý ngay ở block kế tiếp */
} LeCommand;

typedef enum LeCommandType {
    /* Transport */
    LE_CMD_TRANSPORT_PLAY = 1,        /* */
    LE_CMD_TRANSPORT_STOP = 2,        /* */
    LE_CMD_SET_BPM        = 3,        /* d0 = bpm (20..300) */
    LE_CMD_SET_QUANTIZE   = 4,        /* i0 = LeQuantize */
    LE_CMD_METRONOME      = 5,        /* i0 = 0 tắt / 1 bật / 2 chỉ khi thu; f0 = volume 0..1 */
    LE_CMD_SET_COUNT_IN   = 6,        /* i0 = số bar 0..2 */
    /* Clip & scene */
    LE_CMD_CLIP_LAUNCH    = 10,       /* track, slot */
    LE_CMD_CLIP_STOP      = 11,       /* track */
    LE_CMD_SCENE_LAUNCH   = 12,       /* slot = scene */
    LE_CMD_STOP_ALL       = 13,
    LE_CMD_CLIP_RECORD    = 14,       /* track, slot, i0 = số bar (0 = tự do) */
    LE_CMD_RECORD_STOP    = 15,       /* track: kết thúc take tự do tại ranh giới quantize */
    LE_CMD_OVERDUB_TOGGLE = 16,       /* track */
    /* Track */
    LE_CMD_TRACK_GAIN     = 20,       /* track, f0 = dB (-inf..+6), dùng -120 cho -inf */
    LE_CMD_TRACK_PAN      = 21,       /* track, f0 = -1..1 */
    LE_CMD_TRACK_MUTE     = 22,       /* track, i0 = 0/1 */
    LE_CMD_TRACK_SOLO     = 23,       /* track, i0 = 0/1 */
    LE_CMD_TRACK_ARM      = 24,       /* track, i0 = 0/1  (main side: cấp phát buffer thu trước khi enqueue) */
    LE_CMD_TRACK_MONITOR  = 25,       /* track, i0 = 0 off / 1 auto / 2 on */
    LE_CMD_SELECT_TRACK   = 26,       /* track: nhận nốt từ MIDI controller */
    /* Nốt (bàn phím / pad trên màn hình) */
    LE_CMD_NOTE_ON        = 30,       /* track, i0 = note 0..127, f0 = velocity 0..1 */
    LE_CMD_NOTE_OFF       = 31,       /* track, i0 = note */
    LE_CMD_ALL_NOTES_OFF  = 32,       /* track (-1 = mọi track) */
    /* FX & master */
    LE_CMD_FX_PARAM       = 40,       /* track (-1 = master), slot = fx index, i0 = paramId, f0 = value */
    LE_CMD_FX_BYPASS      = 41,       /* track, slot, i0 = 0/1 */
    LE_CMD_MASTER_GAIN    = 42,       /* f0 = dB */
    /* Chỉ dùng trong spike P0 — xoá sau M0 */
    LE_CMD_SPIKE_SINE         = 900,  /* f0 = Hz, f1 = gain 0..1 (gain 0 = tắt) */
    LE_CMD_SPIKE_LOAD_VOICES  = 901,  /* i0 = số voice sine giả lập tải (0..128) */
    LE_CMD_SPIKE_RECORD       = 902,  /* i0 = ms: thu mic vào RAM */
    LE_CMD_SPIKE_PLAY_RECORD  = 903,  /* i0 = 1 phát loop / 0 dừng */
    LE_CMD_SPIKE_PASSTHROUGH  = 904   /* i0 = 0/1: mic → loa (dùng tai nghe!) */
} LeCommandType;

typedef enum LeQuantize {
    LE_Q_NONE = 0, LE_Q_1_16 = 1, LE_Q_1_8 = 2, LE_Q_1_4 = 3, LE_Q_1_2 = 4,
    LE_Q_1_BAR = 5, LE_Q_2_BAR = 6, LE_Q_4_BAR = 7
} LeQuantize;

typedef enum LeClipState {
    LE_CLIP_EMPTY = 0, LE_CLIP_STOPPED = 1, LE_CLIP_QUEUED_PLAY = 2, LE_CLIP_PLAYING = 3,
    LE_CLIP_QUEUED_STOP = 4, LE_CLIP_QUEUED_RECORD = 5, LE_CLIP_RECORDING = 6, LE_CLIP_OVERDUBBING = 7
} LeClipState;

/* ───────────── Trạng thái (engine → UI, đọc mỗi frame) ───────────── */
typedef struct LeState {
    uint32_t publishCounter;          /* tăng mỗi lần audio thread publish */
    uint8_t  playing;
    uint8_t  anyRecording;
    uint8_t  linkEnabled;
    uint8_t  linkPeers;
    double   beat;                    /* vị trí transport (beat), tại đầu block cuối cùng */
    double   bpm;
    double   sampleRate;
    int32_t  bufferSize;
    int32_t  beatsPerBar;
    int32_t  quantize;                /* LeQuantize */
    int32_t  latencyRoundTripSamples;
    float    cpuLoad;                 /* 0..1, trung bình 1 giây */
    float    cpuPeak;                 /* 0..1, max trong 1 giây */
    uint32_t xrunCount;
    int32_t  activeVoices;
    float    inputPeak;               /* 0..1 tuyến tính */
    float    masterPeak[2];
    float    trackPeak[LE_MAX_TRACKS][2];
    uint8_t  clipState[LE_MAX_TRACKS][LE_MAX_SCENES];  /* LeClipState */
    int8_t   trackPlayingSlot[LE_MAX_TRACKS];          /* -1 nếu không phát */
    float    trackClipProgress[LE_MAX_TRACKS];         /* 0..1 vị trí trong clip đang phát/thu */
} LeState;

/* ───────────── Event (engine → UI, bất đồng bộ) ───────────── */
typedef enum LeEventType {
    LE_EVT_RECORDING_FINISHED = 1,    /* a = track, b = slot */
    LE_EVT_JOB_PROGRESS       = 2,    /* jobId, value = 0..1 */
    LE_EVT_JOB_DONE           = 3,    /* jobId */
    LE_EVT_JOB_FAILED         = 4,    /* jobId, a = LeError */
    LE_EVT_XRUN               = 5,    /* a = tổng số xrun */
    LE_EVT_AUDIO_INTERRUPTED  = 6,    /* a = 1 bắt đầu / 0 kết thúc */
    LE_EVT_ROUTE_CHANGED      = 7,    /* a = 1 nếu có tai nghe có dây/interface, b = 1 nếu Bluetooth */
    LE_EVT_TEMPO_CHANGED      = 8,    /* value = bpm (từ Link) */
    LE_EVT_LINK_PEERS         = 9,    /* a = số peer */
    LE_EVT_MIDI_DEVICES       = 10,   /* danh sách đổi → gọi midi.listDevices */
    LE_EVT_MIDI_LEARNED       = 11,   /* a = kind (0 note, 1 cc), b = number */
    LE_EVT_ERROR              = 12,   /* a = LeError */
    LE_EVT_MEMORY_WARNING     = 13    /* value = MB đang dùng */
} LeEventType;

typedef void (*LeEventCallback)(int32_t type, int32_t a, int32_t b, int64_t jobId, double value);

/* ───────────── Mã lỗi ───────────── */
typedef enum LeError {
    LE_OK = 0,
    LE_ERR_INVALID_ARG = -1, LE_ERR_NOT_CREATED = -2, LE_ERR_ALREADY_CREATED = -3, LE_ERR_API_VERSION = -4,
    LE_ERR_NOT_IMPLEMENTED = -5,
    LE_ERR_AUDIO_DEVICE = -10, LE_ERR_MIC_PERMISSION = -11,
    LE_ERR_FILE_NOT_FOUND = -20, LE_ERR_FILE_FORMAT = -21, LE_ERR_DISK_FULL = -22,
    LE_ERR_OUT_OF_MEMORY = -30, LE_ERR_QUEUE_FULL = -31,
    LE_ERR_JOB_CANCELLED = -40, LE_ERR_JOB_NOT_FOUND = -41,
    LE_ERR_PITCH_NOT_DETECTED = -50,
    LE_ERR_INTERNAL = -99
} LeError;

/* ───────────── Hàm ───────────── */
LE_EXPORT int32_t le_api_version(void);
LE_EXPORT int32_t le_create(const LeConfig* config);            /* LeError */
LE_EXPORT void    le_destroy(void);
LE_EXPORT int32_t le_audio_start(void);                         /* LeError */
LE_EXPORT void    le_audio_stop(void);
LE_EXPORT bool    le_send(const LeCommand* cmd);                /* false = queue đầy hoặc lệnh không hợp lệ */
LE_EXPORT char*   le_call(const char* requestJson);             /* luôn trả JSON, giải phóng bằng le_free_string */
LE_EXPORT void    le_free_string(char* s);
LE_EXPORT void    le_read_state(LeState* out);                  /* copy bản mới nhất, không lock, thread nào cũng được */
LE_EXPORT void    le_set_event_callback(LeEventCallback cb);    /* NULL để gỡ */
LE_EXPORT int32_t le_get_peaks(const char* clipId, int32_t level,
                               float* outMinMax, int32_t maxPairs); /* trả số cặp đã ghi, <0 = LeError */

#ifdef __cplusplus
}
#endif
```

---

## 3. Lệnh cấu trúc (`le_call`)

**Envelope:**
- Request: `{"op":"<tên>", ...tham số}`
- Response thành công: `{"ok":true,"result":{...}}`
- Response lỗi: `{"ok":false,"error":{"code":"FILE_NOT_FOUND","message":"..."}}`
- Lệnh trả `jobId` là lệnh bất đồng bộ: theo dõi bằng event `JOB_*`, lấy kết quả bằng `job.result`.
- **Định dạng job (đã chốt 2026-09-29):**
  - Lệnh tạo job trả `{"ok":true,"result":{"jobId":42}}`.
  - `job.result {jobId}` trả `{"ok":true,"result":{"status":"running","progress":0.4}}`, hoặc `{"ok":true,"result":{"status":"done","result":{...}}}`, hoặc `{"ok":true,"result":{"status":"failed","error":{"code":"...","message":"..."}}}`.
  - Job **thất bại** vẫn trả `ok:true`, vì bản thân lời gọi `job.result` thành công. `jobId` không tồn tại thì mới trả `ok:false` với mã `JOB_NOT_FOUND`.
- **`engine.info` (P0-03)** trả thêm `running, inputLatencySamples, outputLatencySamples, deviceXruns, configSize, rejectedCommands, droppedEvents, sessionMode`.
- **Spike:** `LE_EVT_RECORDING_FINISHED` do `SPIKE_RECORD` sinh ra có `a = -1, b = -1, value = số frame đã thu`.
- **Khoá layout:** kích thước và offset đã được kiểm tra bằng compiler C: `LeCommand` = 32, `LeState` = 248, `LeConfig` = 40 byte. Offset của `LeState`: `beat@8, bpm@16, sampleRate@24, bufferSize@32, quantize@40, cpuLoad@48, xrunCount@56, inputPeak@64, masterPeak@68, trackPeak@76, clipState@140, trackPlayingSlot@204, trackClipProgress@212`. Cả test C++ (`static_assert(offsetof…)`) lẫn test Dart đều khoá các giá trị này.

| op | Tham số | Kết quả | Phase |
|---|---|---|---|
| `engine.info` | – | `{apiVersion, sampleRate, bufferSize, inputChannels, latencyRoundTripSamples, device, stateSize, commandSize}` | P0 |
| `spike.setBufferSize` | `{frames: 128\|256}` | `{bufferSize}`: khởi động lại device | P0 |
| `spike.setSessionMode` | `{mode:"default"\|"measurement"}` | `{mode, applied}`: áp dụng ngay | P0 |
| `spike.sessionInfo` | – | `{supported, session:{category, mode, options:{mixWithOthers, defaultToSpeaker, allowBluetoothA2DP, allowBluetoothHFP, allowAirPlay}, sampleRate, ioBufferDuration, inputLatency, outputLatency, inputs:[…], outputs:[…]}}` | P0 |
| `spike.latencyLoopback` | `{}` | `jobId` → `{measuredSamples, reportedSamples, runs:[...]}` | P0 |
| `spike.stretchBench` | `{semitones:[-18,-15,-12,-9,-6,-3,0,3,6,9,12,15,18], formant:true, saveDir}`. `semitones` là **danh sách các giá trị cụ thể** (không phải cặp min/max). `saveDir` = `<Documents>/spike` | `jobId` → `{msTotal, msPerZone:[...], files:[...]}` | P0 |
| `project.open` | `{dir}` | `{}`: reset model, `dir` là nơi lưu audio thu âm | P1 |
| `project.close` | – | `{}` | P1 |
| `transport.setTimeSignature` | `{num, den}` | `{}` | P1 |
| `track.configure` | `{track, kind:"audio"\|"instrument", name}` | `{}` | P1 |
| `track.setInstrument` | `{track, instrument:{kind:"sfz", path} \| {kind:"user", id}}` | `jobId` | P1 |
| `clip.setAudio` | `{track, slot, clipId, file, lengthBeats, originalBpm, warp:"stretch"\|"repitch", gain}` | `jobId` (decode) | P1 |
| `clip.setMidi` | `{track, slot, clipId, lengthBeats, notes:[{p,v,s,d}]}` | `{}` | P1 |
| `clip.getMidi` | `{track, slot}` | `{notes:[...]}` | P1 |
| `clip.info` | `{track, slot}` | `{clipId, kind, file?, lengthBeats, originalBpm?, warp?, gain}` | P1 |
| `clip.clear` | `{track, slot}` | `{}` | P1 |
| `clip.undoOverdub` | `{track, slot}` | `{}` | P1 |
| `clip.setLoopRegion` | `{track, slot, startSample, lengthBeats}` | `{}` | P2 |
| `midiClip.quantize` | `{track, slot, grid:0.25}` | `{}` | P2 |
| `capture.start` / `capture.stop` | `{path, maxSeconds}` / – | `{}` / `{file, seconds}`: thu một mẫu cho sampler (không tạo clip) | P3 |
| `instrument.createFromRecording` | `{instrumentId, file, rootNote?, mode:"natural"\|"classic"}` | `jobId` → `{rootNote, cents, confidence, zones}` | P3 |
| `instrument.setMode` | `{instrumentId, mode}` | `{}` | P3 |
| `fx.set` | `{track (-1=master), index, type:"filter"\|"delay"\|"reverb"\|"eq3"\|"comp", params:{id:value}}` | `{}` | P3 |
| `fx.remove` | `{track, index}` | `{}` | P3 |
| `export.scene` | `{scene, bars, path, format:"wav"\|"m4a", stems:false}` | `jobId` | P3 |
| `export.jamStart` / `export.jamStop` | `{path}` / – | `{}` / `{file, seconds}` | P3 |
| `latency.calibrate` | – | `jobId` → `{roundTripSamples}` | P4 |
| `latency.setOffset` | `{samples}` | `{}` | P4 |
| `midi.listDevices` | – | `{inputs:[{id,name,enabled}], outputs:[...]}` | P4 |
| `midi.enableDevice` | `{id, enabled}` | `{}` | P4 |
| `midi.learnStart` / `midi.learnCancel` | `{target:{kind:"clip",track,slot} \| {kind:"fx",track,slot,param} \| ...}` | `{}` | P4 |
| `midi.setMappings` | `{mappings:[...]}` | `{}` | P4 |
| `link.enable` | `{enabled, startStopSync}` | `{}` | P4 |
| `job.result` | `{jobId}` | `{status:"running"\|"done"\|"failed", result?, error?}` | P1 |
| `job.cancel` | `{jobId}` | `{}` | P1 |

---

## 4. Wrapper phía Dart (`packages/engine_ffi`)

```dart
/// Một instance duy nhất cho toàn app. Mọi method gọi trên main isolate.
final class EngineClient {
  EngineClient._(this._b);
  final LoopCoreBindings _b;                         // sinh bằng ffigen từ engine_api.h
  final Pointer<LeState> _state = calloc<LeState>(); // cấp phát 1 lần, dùng lại mỗi frame
  final Pointer<LeCommand> _cmd = calloc<LeCommand>();
  NativeCallable<LeEventCallbackFunction>? _callback;
  final _events = StreamController<EngineEvent>.broadcast();

  static EngineClient open() =>
      EngineClient._(LoopCoreBindings(DynamicLibrary.process())); // static link trên iOS

  Stream<EngineEvent> get events => _events.stream;

  int create(EngineConfig c) {
    _callback = NativeCallable<LeEventCallbackFunction>.listener(_onEvent);
    _b.le_set_event_callback(_callback!.nativeFunction);
    return using((arena) => _b.le_create(c.toNative(arena)));
  }

  bool send(int type, {int track = -1, int slot = -1, int i0 = 0,
                       double f0 = 0, double f1 = 0, double d0 = 0}) {
    _cmd.ref
      ..type = type ..track = track ..slot = slot
      ..i0 = i0 ..f0 = f0 ..f1 = f1 ..d0 = d0 ..hostTimeNs = 0;
    return _b.le_send(_cmd);   // engine copy ngay nên dùng lại _cmd được
  }

  Map<String, dynamic> call(Map<String, dynamic> request) {
    final req = jsonEncode(request).toNativeUtf8();
    try {
      final res = _b.le_call(req.cast());
      final text = res.cast<Utf8>().toDartString();
      _b.le_free_string(res);
      return jsonDecode(text) as Map<String, dynamic>;
    } finally {
      malloc.free(req);
    }
  }

  /// Gọi mỗi frame từ Ticker. Trả về view của struct native: đọc ngay, không giữ lâu.
  LeState readState() { _b.le_read_state(_state); return _state.ref; }

  void _onEvent(int type, int a, int b, int jobId, double value) =>
      _events.add(EngineEvent(type, a, b, jobId, value));

  void dispose() {
    _b.le_set_event_callback(nullptr);
    _callback?.close();
    _b.le_destroy();
    calloc..free(_state)..free(_cmd);
  }
}
```

### Kiểu dữ liệu tầng trên
- `EngineState`: class Dart **có thể thay đổi**, được cập nhật tại chỗ mỗi frame từ `LeState` để tránh cấp phát. Widget đọc qua `EngineStateTicker` (07 §5).
- `EngineEvent` → map sang sealed class: `RecordingFinished`, `JobProgress`, `JobDone`…
- `JobHandle`: `Future<Map>` hoàn thành khi có `JOB_DONE` (tự gọi `job.result`) hoặc báo lỗi khi `JOB_FAILED`.

---

## 5. Kiểm tra hợp đồng

- Test C++ (`tests/api_contract_test.cpp`): `static_assert(sizeof(LeCommand) == 32)`, gọi thử mọi op JSON với tham số sai → phải nhận `ok:false` đúng mã lỗi.
- Test Dart (`engine_ffi/test/`): kiểm tra `sizeOf<LeCommand>() == 32` và `sizeOf<LeState>()` bằng giá trị C++ in ra (qua `engine.info` trả thêm `stateSize`).
- Đổi header → chạy `scripts/gen_bindings.sh` (ffigen) → build lại cả hai bên → chạy test hợp đồng.
