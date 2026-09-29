/*
 * engine_api.h — LoopCore C API (v1)
 *
 * HỢP ĐỒNG giữa engine C++ và app Flutter (dart:ffi / ffigen).
 * Spec: docs/05-ffi-bridge.md — sửa file này thì PHẢI sửa spec cùng lúc,
 * và tăng LE_API_VERSION nếu thay đổi không tương thích ngược.
 *
 * Quy tắc thread: mọi hàm le_* gọi từ main thread, trừ le_read_state (thread nào cũng được).
 */
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
    int64_t  hostTimeNs;              /* 0 = xử lý ở block kế tiếp */
} LeCommand;

typedef enum LeCommandType {
    /* Transport */
    LE_CMD_TRANSPORT_PLAY = 1,
    LE_CMD_TRANSPORT_STOP = 2,
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
    LE_CMD_TRACK_GAIN     = 20,       /* track, f0 = dB (-120 = -inf .. +6) */
    LE_CMD_TRACK_PAN      = 21,       /* track, f0 = -1..1 */
    LE_CMD_TRACK_MUTE     = 22,       /* track, i0 = 0/1 */
    LE_CMD_TRACK_SOLO     = 23,       /* track, i0 = 0/1 */
    LE_CMD_TRACK_ARM      = 24,       /* track, i0 = 0/1 (phía main cấp phát buffer thu trước khi enqueue) */
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
    /* Chỉ dùng trong spike P0 — xoá ở P1-37 (khi đó tăng LE_API_VERSION) */
    LE_CMD_SPIKE_SINE         = 900,  /* f0 = Hz, f1 = gain 0..1 (0 = tắt) */
    LE_CMD_SPIKE_LOAD_VOICES  = 901,  /* i0 = số voice giả lập tải (0..128) */
    LE_CMD_SPIKE_RECORD       = 902,  /* i0 = ms: thu mic vào RAM (tối đa 10000) */
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

/* ───────────── Trạng thái (engine → UI, đọc mỗi frame bằng le_read_state) ───────────── */
typedef struct LeState {
    uint32_t publishCounter;          /* tăng mỗi lần audio thread publish */
    uint8_t  playing;
    uint8_t  anyRecording;
    uint8_t  linkEnabled;
    uint8_t  linkPeers;
    double   beat;                    /* vị trí transport (beat) tại đầu block gần nhất */
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

/* ───────────── Event (engine → UI, bất đồng bộ; tham số theo giá trị) ───────────── */
typedef enum LeEventType {
    LE_EVT_RECORDING_FINISHED = 1,    /* a = track, b = slot */
    LE_EVT_JOB_PROGRESS       = 2,    /* jobId, value = 0..1 */
    LE_EVT_JOB_DONE           = 3,    /* jobId */
    LE_EVT_JOB_FAILED         = 4,    /* jobId, a = LeError */
    LE_EVT_XRUN               = 5,    /* a = tổng số xrun */
    LE_EVT_AUDIO_INTERRUPTED  = 6,    /* a = 1 bắt đầu / 0 kết thúc */
    LE_EVT_ROUTE_CHANGED      = 7,    /* a = 1 nếu tai nghe có dây/interface, b = 1 nếu Bluetooth */
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
LE_EXPORT char*   le_call(const char* requestJson);             /* luôn trả JSON; giải phóng bằng le_free_string */
LE_EXPORT void    le_free_string(char* s);
LE_EXPORT void    le_read_state(LeState* out);                  /* copy bản mới nhất, không lock */
LE_EXPORT void    le_set_event_callback(LeEventCallback cb);    /* NULL để gỡ */
LE_EXPORT int32_t le_get_peaks(const char* clipId, int32_t level,
                               float* outMinMax, int32_t maxPairs); /* số cặp đã ghi, <0 = LeError */

#ifdef __cplusplus
}
#endif
