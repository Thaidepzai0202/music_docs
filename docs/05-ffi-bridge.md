# 05 — FFI bridge: hợp đồng giữa Flutter và engine

> Đây là **hợp đồng**. Engine (C++) và app (Dart) được phát triển song song dựa trên file này.
> File header thật nằm ở `engine/include/le/engine_api.h`. Khi đổi header thì **phải cập nhật file này** và tăng `LE_API_VERSION` nếu thay đổi làm vỡ tương thích.

---

## 1. Quy tắc

1. Mọi hàm `le_*` được gọi từ **main thread**. Từ Flutter 3.29, Dart chạy trên main thread của iOS, nên FFI gọi thẳng là đúng. Ngoại lệ duy nhất là `le_read_state`, gọi được từ thread bất kỳ.
2. Mỗi hàm `le_*` phải trả về trong **< 1ms**. Việc gì lâu hơn phải thành **job** (trả `jobId` ngay).
3. Không truyền con trỏ tới object Dart sang C. Chuỗi là UTF-8. Chuỗi do engine trả về phải được giải phóng bằng `le_free_string`.
   - Engine **kiểm UTF-8 hợp lệ** ở biên (`le_call`, `le_get_peaks`) bằng `CharPointer_UTF8::isValidString`. Không hợp lệ thì trả `INVALID_ARG`.
   - Sau đó engine **kiểm cú pháp JSON** bằng `util::JsonValidator`, **trước khi** gọi `juce::JSON::parse`. Key rỗng, JSON bị cắt, số sai dạng, độ sâu lồng > 32 hoặc request > 1 MB đều trả `INVALID_ARG`. Lý do: `juce::JSON::parse` assert với các trường hợp này (fuzz tìm ra 29/09).
   - **Giới hạn của JSON do JUCE đọc:**
     - Số nguyên có **tối đa 18 chữ số** (JUCE tràn int64 **không báo** khi dài hơn). Mọi id và số trong API đều nằm trong ±2^53, nên không ảnh hưởng.
     - **Chuỗi không được chứa ký tự NUL** (`\u0000`), kể cả trong giá trị, vì JUCE không đọc được. UI lọc NUL ở ô nhập tên.
   - Mọi chỗ đổi `const char*` sang `juce::String` phải dùng `juce::String::fromUTF8` (constructor `juce::String(const char*)` của JUCE hiểu chuỗi là **ASCII**, nên sẽ làm hỏng tiếng Việt và emoji).
   - Tên project, đường dẫn và tên clip có dấu tiếng Việt hoặc emoji phải chạy đúng từ đầu đến cuối (có test).
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
    LE_CMD_LOOP_BUTTON    = 17,       /* track, slot (-1 = tự chọn): một lần chạm nút LOOP/footswitch → engine xoay vòng thu → chốt → overdub → phát (thêm 29/09, không phá ABI) */
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
    LE_EVT_TEMPO_CHANGED      = 8,    /* value = bpm (Link, hoặc pedal mode vừa suy ra); value = 0 → pedal mode trở về "chưa có tempo" */
    LE_EVT_LINK_PEERS         = 9,    /* a = số peer */
    LE_EVT_MIDI_DEVICES       = 10,   /* danh sách đổi → gọi midi.listDevices */
    LE_EVT_MIDI_LEARNED       = 11,   /* a = kind (0 note, 1 cc), b = number */
    LE_EVT_ERROR              = 12,   /* a = LeError */
    LE_EVT_MEMORY_WARNING     = 13,   /* value = MB đang dùng */
    LE_EVT_CLIP_CHANGED       = 14    /* a = track, b = slot: nội dung clip MIDI vừa đổi (overdub trộn nốt), tối đa 10 lần/s; thêm 30/09, không phá ABI */
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
    LE_ERR_OVERDUB_UNSUPPORTED = -60,   /* thêm 29/09 (không phá ABI): clip Re-Pitch/MIDI hoặc chưa có target → không vào OVERDUBBING */
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
- **Thứ tự bảo đảm của job:** khi nhận `JOB_DONE`, model và snapshot **đã được áp dụng**. Các `le_call` gọi ngay sau đó (`clip.info`, `clip.getMidi`, `job.result`) thấy kết quả mới. Riêng **`LeState` có thể trễ tối đa 1 block** (khoảng 2.7ms ở 128 frame), vì audio thread nhận snapshot ở đầu block kế tiếp. Test đọc `LeState` sau `JOB_DONE` phải render thêm 1 block.
- **`engine.info` (P0-03)** trả thêm `running, inputLatencySamples, outputLatencySamples, deviceXruns, configSize, rejectedCommands, droppedEvents, sessionMode`.
- **Spike:** `LE_EVT_RECORDING_FINISHED` do `SPIKE_RECORD` sinh ra có `a = -1, b = -1, value = số frame đã thu`.
- **`LE_CMD_LOOP_BUTTON` (máy trạng thái trong engine, dùng chung cho nút LOOP trên UI và footswitch):**
  - Chọn ô đích: `slot ≥ 0` thì dùng ô đó. `slot = -1` thì ưu tiên ô đang Recording / Overdubbing / Playing của track, không có thì lấy ô trống đầu tiên.
  - Chuyển trạng thái của ô: Empty → thu tự do (`CLIP_RECORD i0 = 0`) · Recording → chốt (`RECORD_STOP`) · Playing → bật overdub · Overdubbing → tắt overdub · Stopped/Queued → launch.
  - **Không** có chạm đúp hay nhấn giữ trên LOOP. Dừng track và hoàn tác dùng nút riêng trên UI (07 §3.1b), hoặc target MIDI `trackStop` / `undoOverdub`.
- **Khoá layout:** kích thước và offset đã được kiểm tra bằng compiler C: `LeCommand` = 32, `LeState` = 248, `LeConfig` = 40 byte. Offset của `LeState`: `beat@8, bpm@16, sampleRate@24, bufferSize@32, quantize@40, cpuLoad@48, xrunCount@56, inputPeak@64, masterPeak@68, trackPeak@76, clipState@140, trackPlayingSlot@204, trackClipProgress@212`. Cả test C++ (`static_assert(offsetof…)`) lẫn test Dart đều khoá các giá trị này.

| op | Tham số | Kết quả | Phase |
|---|---|---|---|
| `engine.info` | – | `{apiVersion, sampleRate, bufferSize, inputChannels, latencyRoundTripSamples, device, stateSize, commandSize}` | P0 |
| `spike.setBufferSize` | `{frames: 128\|256}` | `{bufferSize}`: khởi động lại device | P0 |
| `spike.setSessionMode` | `{mode:"default"\|"measurement"}` | `{mode, applied}`: áp dụng ngay | P0 |
| `spike.sessionInfo` | – | `{supported, session:{category, mode, options:{mixWithOthers, defaultToSpeaker, allowBluetoothA2DP, allowBluetoothHFP, allowAirPlay}, sampleRate, ioBufferDuration, inputLatency, outputLatency, inputs:[…], outputs:[…]}}` | P0 |
| `spike.latencyLoopback` | `{}`. Audio phải đang chạy, nếu không trả `ok:false AUDIO_DEVICE` | `jobId` → `{ok, measuredSamples, measuredMs, reportedSamples, reportedMs, runs, validRuns, spreadSamples, spreadMs, score, inputPeak, sampleRate}` | P0 |
| `spike.stretchBench` | `{semitones:[-18,-15,-12,-9,-6,-3,0,3,6,9,12,15,18], formant:true, saveDir}`. `semitones` là **danh sách các giá trị cụ thể** (không phải cặp min/max). `saveDir` = `<Documents>/spike`. Tuỳ chọn: `baseHz, blockMs, intervalMs, tonalityLimitHz, cheaper` | `jobId` → `{msTotal, msPerZone:[...], files:[...], …}` | P0 |
| `project.open` | `{dir}` | `{}`. Reset **toàn bộ state của project** về mặc định: track trống, không FX, không nhạc cụ, master EQ phẳng, trần limiter −0.3, BPM 120, 4/4, quantize 1 bar, metronome tắt, count-in 0, mapping MIDI trống, lớp undo trống. **Không reset thiết lập toàn cục**: `midi.setRecordQuantize`, `latency.setOffset`, `audio.setInputEnabled`, buffer size, thiết bị MIDI đã bật. `dir` là nơi lưu audio thu âm | P1 |
| `project.close` | – | `{}` | P1 |
| `transport.setTimeSignature` | `{num, den}` | `{}` | P1 |
| `transport.setTempoMode` | `{mode:"fixed"\|"firstLoop", firstLoopBeats?}` | `{hasTempo, firstLoopBeats}`. `firstLoopBeats` = độ dài vòng đầu (beat), dùng để làm tròn các vòng sau, kể cả sau khi mở lại project. Khi trở về "chưa có tempo", engine phát `TEMPO_CHANGED(value = 0)`. Pedal mode, 04 §2.5. `engine.info.tempoState = {mode, hasTempo, firstLoopBeats}` (`firstLoopBeats = 0` nghĩa là chưa có vòng đầu). Với `firstLoop`: `hasTempo = true` khi model **đã có ≥ 1 clip**, BPM lấy từ model. UI đọc lại sau `TEMPO_CHANGED`, `RECORDING_FINISHED` và `project.open`. Khi mở project, gửi op này **ngay sau** `SET_BPM` / `transport.setTimeSignature` | P2 |
| `track.configure` | `{track, kind:"audio"\|"instrument", name, color?:"#RRGGBB"}` | `{}`. `color` dùng để engine chọn màu LED Launchpad gần nhất (P4-05) | P1 |
| `track.setInstrument` | `{track, instrument:{kind:"sfz", path} \| {kind:"user", id}}` | `jobId` | P1 |
| `clip.setAudio` | `{track, slot, clipId, file, lengthBeats, originalBpm, warp:"stretch"\|"repitch", gainDb}` (**dB**, cùng đơn vị với model) | `jobId` (decode) | P1 |
| `clip.setMidi` | `{track, slot, clipId, lengthBeats, notes:[{p,v,s,d}]}` | `{}` | P1 |
| `clip.getMidi` | `{track, slot}` | `{notes:[...]}` | P1 |
| `clip.setParams` | `{track, slot, gainDb?, warp?}` | `{}`. Chỉ đổi tham số, **không decode lại**: dựng snapshot mới, audio thread dùng ramp để không click | P2 |
| `clip.info` | `{track, slot}` | `{clipId, kind, file?, lengthBeats, originalBpm?, warp?, gainDb, hasUndo}`. Ô trống trả `{kind:"empty"}`. `hasUndo` = true khi ô có lớp `clip.undoOverdub`. **Chỉ clip audio có lớp undo**, clip MIDI luôn `false`. `stretchedBpm` = BPM của bản stretch đang dùng (0 nếu đang Re-Pitch) | P1 |
| `clip.clear` | `{track, slot}` | `{}` | P1 |
| `clip.undoOverdub` | `{track, slot}` | `{}` | P1 |
| `clip.setLoopRegion` | `{track, slot, startSample, lengthBeats}` | `{}` | P2 |
| `midi.setRecordQuantize` | `{grid: 0 \| 0.25 \| 0.5}` (beat, 0 = tắt) | `{}`. Áp dụng khi ghép nốt lúc **thu xong** (P1-30). Đây là **thiết lập toàn cục của engine: `project.open` KHÔNG reset.** UI gọi lệnh này khi mở app và khi đổi Settings | P1 |
| `midiClip.quantize` | `{track, slot, grid:0.25}` | `{}` | P2 |
| `capture.start` / `capture.stop` | `{path, maxSeconds}` / – | `{}` / `{file, seconds}`: thu một mẫu cho sampler (không tạo clip). Tới `maxSeconds` thì tự dừng và phát `RECORDING_FINISHED(a = -2, b = -2, value = frames)`. Gọi `capture.stop` sau đó vẫn trả đúng kết quả (idempotent). Meter lấy từ `LeState.inputPeak` | P3 |
| `capture.analyze` | `{file}` | `jobId` → `{trimStartSample, trimEndSample, rootNote, cents, confidence, peaks:[min,max,…] (512 cặp)}`. Chạy SilenceTrimmer + Yin. UI dùng kết quả để vẽ waveform, hiện handle trim và nốt gốc **trước** khi tạo nhạc cụ | P3 |
| `audio.setInputEnabled` | `{enabled}` | `{inputChannels}`. Khởi động lại device có hoặc không có input (category PlayAndRecord ↔ Playback). Dùng khi người dùng **từ chối quyền mic**: app vẫn phát được, chỉ tắt thu. Được quyền lại thì bật input | P2 |
| `instrument.createFromRecording` | `{instrumentId, file, trimStartSample?, trimEndSample?, rootNote?, mode:"natural"\|"classic"}`. **Engine đăng ký `instrumentId` ngay khi nhận lệnh**, nên `instrument.setEnvelope` / `setMode` / `track.setInstrument {kind:"user"}` gửi trong lúc job chưa xong vẫn hợp lệ, và được áp dụng khi render xong. Không truyền trim thì tự trim. Không truyền `rootNote` mà confidence < 0.6 thì job failed `PITCH_NOT_DETECTED` | `jobId` → `{rootNote, cents, confidence, zones}` | P3 |
| `instrument.setMode` | `{instrumentId, mode}` | `{}` | P3 |
| `instrument.setEnvelope` | `{instrumentId, a, d, s, r}` (giây, s là 0..1) | `{}`. Áp dụng cho mọi zone của nhạc cụ tự thu. Voice đang kêu giữ envelope cũ, nốt mới dùng envelope mới | P3 |
| `fx.set` | `{track (0..7), index (0..2), type:"filter"\|"delay"\|"reverb"\|"eq3"\|"comp", params:{id:value}, bypass:false}`. **`"comp"` là tên chuẩn** (engine chấp nhận thêm `"compressor"` làm bí danh). **Master không có slot người dùng**: `track = -1` trả `INVALID_ARG`. Master cố định slot 0 = EQ3 (param 0/1/2 = low/mid/high dB), slot 1 = Limiter (param 0 = ceiling dB, 1 = release ms), chỉ chỉnh qua `LE_CMD_FX_PARAM track -1`. Khi mở project thì `bypass` đi kèm luôn trong lệnh này. `LE_CMD_FX_BYPASS` chỉ dùng để bật/tắt nhanh lúc đang chơi | `{}` | P3 |
| `fx.remove` | `{track, index}` | `{}`. Slot **cố định**, engine không dồn slot. Xoá slot trống vẫn `ok`. UI tự dồn bằng cách gửi lại `fx.set` | P3 |
| `export.scene` | `{scene, bars, path, format:"wav"\|"m4a", stems:false}` | `jobId` → `{file, seconds, stems?:[…]}`. Tên file stem: `<base>_t<n>.<ext>`, với n = **1..8** (số track đếm từ 1, dễ đọc với người dùng) | P3 |
| `export.jamStart` / `export.jamStop` | `{path}` / – | `{}` / `{file, seconds}`. Gọi `jamStop` khi chưa start thì trả `INVALID_ARG` | P3 |
| `latency.calibrate` | – | `jobId` → `{measuredSamples, reportedSamples, offsetSamples = measured − reported, spreadSamples, confidence}`. Thành công thì engine **áp dụng ngay** `offsetSamples`. Engine **không lưu** qua các lần mở app: app tự lưu offset và gửi lại bằng `latency.setOffset` khi khởi động | P4 |
| `latency.setOffset` | `{samples}` | `{}`. **Dấu:** số dương = tăng độ trễ cần bù, tức là take bị dời **sớm lên** thêm. L thực tế = inputLatency + outputLatency (theo số device báo) + `samples` (04 §5.3) | P4 |
| `midi.listDevices` | – | `{inputs:[{id,name,enabled,open}], outputs:[{id,name}]}` | P4 |
| `midi.enableDevice` | `{id, enabled}` | `{}`. Thiết lập **toàn cục**. Engine nhớ cả thiết bị chưa cắm, cắm vào là tự mở. Cắm hoặc rút thì phát `LE_EVT_MIDI_DEVICES` | P4 |
| `memory.pressure` | `{level:"warning"\|"critical"}` | `{freedMB, usedMB}`. Nhả bản stretched của clip không phát, nhạc cụ tự thu không track nào dùng, peaks thừa; mức critical nhả thêm lớp undo của ô không phát. Engine **tự** nghe `UIApplicationDidReceiveMemoryWarning` (xử lý như critical). Sau đó phát `LE_EVT_MEMORY_WARNING(a = 1 nếu critical, value = MB)`. `engine.info.memoryMB` | P4 |
| `sim.midiIn` | `{bytes:[s,d1,d2]}` | `{}`. **Chỉ có ở bản build test** (`LE_ENABLE_SIM`): đưa message MIDI tổng hợp vào nguồn "virtual" | P4 (test) |
| `midi.learnStart` / `midi.learnCancel` | `{target}` | `{}`. Các kind của `target`, khớp `midi::LearnAction`: `{kind:"clip",track,slot}` · `{kind:"scene",slot}` · `{kind:"transport",action:"play"\|"stop"\|"toggle"}` · `{kind:"stopAll"}` · `{kind:"trackGain",track,minDb?:-60,maxDb?:6}` · `{kind:"trackMute",track}` · `{kind:"fx",track(-1=master),slot,param,min?,max?}` · `{kind:"loopButton"}` (tác động lên track đang chọn, giống `LE_CMD_LOOP_BUTTON slot = -1`) · `{kind:"trackStop"}` (dừng track đang chọn) · `{kind:"undoOverdub"}` (hoàn tác overdub của ô đang phát trên track đang chọn). Ba target này để nối footswitch | P4 |
| `midi.learnResult` | – | `{deviceId, deviceName, kind:"note"\|"cc", channel, number}`: nguồn của lần learn gần nhất. Gọi sau `LE_EVT_MIDI_LEARNED`, vì event chỉ có kind + number (header không đổi) | P4 |
| `midi.setMappings` | `{mappings:[{src:{device, kind, channel, number}, target}]}` | `{}`. `device: ""` = mọi thiết bị, `channel: -1` = mọi kênh. `target` giống `midi.learnStart` | P4 |
| `link.enable` | `{enabled, startStopSync}` | `{}` | P4 |
| `launchLog.read` | `{sinceIndex}` | `{events:[{index, beat, track, slot, kind:"launch"\|"stop"\|"record"\|"scene"}], nextIndex}`. Giữ tối đa 4096 sự kiện gần nhất | P1 (P1-17) |
| `sim.offline` | `{enabled, sampleRate:48000, blockSize:128}` | `{}`. **Chỉ có ở bản build test** (Mac, `LE_ENABLE_SIM`). Thay device thật bằng `OfflineDeviceIO`; phải gọi khi audio chưa chạy. Bản iOS release trả `NOT_IMPLEMENTED` | P1 (test) |
| `sim.advance` | `{frames}` hoặc `{beats}` | `{beat, frames}`. Render **đồng bộ** trên main, output bị bỏ đi. Đây là ngoại lệ duy nhất của luật "< 1ms", vì chỉ dùng trong test. Để test hợp đồng Dart (vd. `clip_scheduler_contract.dart`) chạy được với engine thật | P1 (test) |
| `preview.play` | `{source:{kind:"sfz",path,base?} \| {kind:"audio",file,base?}, note?:60, durationMs?:1500}` | `{}`. **`base`: `"library"` (mặc định, đường dẫn tương đối theo `libraryDir`) \| `"project"` (tương đối theo thư mục của `project.open`, dùng cho Bản thu của tôi).** Nghe thử trong Browser qua **kênh preview riêng** (một sampler ngoài các track, trộn vào master trước limiter). Không ảnh hưởng track hay transport. Gọi lần nữa thì dừng bản đang nghe. Kit thì phát một groove ngắn. Nạp chạy trên worker; lần đầu có thể trễ khoảng 100ms. **Giới hạn đã biết:** (1) tiếng preview có trong bản ghi jam (`export.jamStart` ghi sau limiter) — P4-26 tách đường cue; (2) nạp lỗi (file hỏng, SFZ sai) không được báo: reply `{}` trả trước khi nạp, chỉ `FILE_NOT_FOUND` / `INVALID_ARG` báo đồng bộ | P2 |
| `preview.stop` | – | `{}` | P2 |
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
