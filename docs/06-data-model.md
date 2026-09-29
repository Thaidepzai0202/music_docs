# 06 — Data model & định dạng file

> **Nguồn sự thật:** project model nằm ở **Dart**. Engine chỉ giữ bản cần để phát (EngineModel và snapshot).
> Khi mở project, Dart **phát lại** một chuỗi lệnh cấu trúc để dựng lại engine (§6).

---

## 1. Cấu trúc thư mục

```
<App Documents>/                          ← hiện trong app Files (UIFileSharingEnabled + LSSupportsOpeningDocumentsInPlace)
├── Projects/
│   └── My Jam.loopproj/                  ← "package": app Files hiển thị như 1 file
│       ├── project.json                  ← model, xem §2
│       ├── project.json.bak              ← bản lưu thành công trước đó
│       ├── audio/
│       │   └── c_<uuid>.caf              ← take thu âm (float32, sample rate gốc)
│       ├── instruments/
│       │   └── i_<uuid>/
│       │       ├── source.caf            ← tiếng thu gốc (đã trim)
│       │       └── zones/zone_+03.caf    ← CACHE: render lại được
│       └── cache/                        ← xoá được bất cứ lúc nào
│           ├── c_<uuid>.peaks
│           └── stretched/c_<uuid>@120.00.caf
├── UserSamples/                          ← tiếng thu dùng lại được giữa các project (phase 2)
└── Exports/
    └── My Jam – Scene 1 – 2027-03-20.wav

<App Bundle>/…/flutter_assets/assets/library/   ← chỉ đọc, đóng gói sẵn trong app dưới dạng Flutter asset
                                          (đường dẫn thật lấy bằng FlutterDartProject.lookupKey, rồi truyền vào LeConfig.libraryDir)
├── manifest.json
├── kits/<kitId>/<kitId>.sfz + samples/*.flac
├── instruments/<instId>/<instId>.sfz + samples/*.flac
└── loops/<loopId>.flac (+ metadata trong manifest)
```
- Khai báo UTI `com.<you>.loopproj`, kiểu `com.apple.package`, trong `UTExportedTypeDeclarations`.
- ID dùng UUID v4 có tiền tố: `t_` (track), `c_` (clip), `i_` (instrument), `p_` (project).

---

## 2. `project.json`, schema v1

```json
{
  "schemaVersion": 1,
  "id": "p_7d0c…",
  "name": "My Jam",
  "createdAt": "2027-03-20T10:15:00Z",
  "modifiedAt": "2027-03-20T11:02:13Z",
  "appVersion": "0.1.0+12",
  "transport": {
    "bpm": 120.0,
    "timeSignature": [4, 4],
    "quantize": "1bar",
    "metronome": { "mode": "recordOnly", "volume": 0.7 },
    "countInBars": 1,
    "tempoMode": "fixed",
    "firstLoopBeats": null
  },
  "scenes": [
    { "index": 0, "name": "Verse" }, { "index": 1, "name": "Chorus" }
  ],
  "tracks": [
    {
      "id": "t_a1…", "index": 0, "name": "Drums", "color": "#FF7A59",
      "kind": "instrument",
      "instrument": { "kind": "sfz", "path": "kits/808/808.sfz" },
      "mixer": { "gainDb": 0.0, "pan": 0.0, "mute": false, "solo": false },
      "monitor": "off",
      "fx": [
        { "type": "filter", "bypass": false, "params": { "0": 0, "1": 18000, "2": 0.7 } }
      ],
      "clips": [
        {
          "slot": 0, "id": "c_91…", "kind": "midi", "name": "Beat A",
          "lengthBeats": 8.0,
          "notes": [ { "p": 36, "v": 110, "s": 0.0, "d": 0.25 }, { "p": 38, "v": 100, "s": 1.0, "d": 0.25 } ]
        }
      ]
    },
    {
      "id": "t_b2…", "index": 1, "name": "Vocal", "color": "#59C3FF",
      "kind": "audio",
      "mixer": { "gainDb": -3.0, "pan": 0.1, "mute": false, "solo": false },
      "monitor": "auto",
      "fx": [ { "type": "reverb", "bypass": false, "params": { "0": 0.6, "1": 0.4, "2": 1.0, "3": 0.25 } } ],
      "clips": [
        {
          "slot": 0, "id": "c_3f…", "kind": "audio", "name": "Hát 1",
          "file": "audio/c_3f….caf",
          "lengthBeats": 16.0, "originalBpm": 120.0,
          "warp": "stretch", "gainDb": 0.0, "tags": [],
          "loop": { "startSample": 0 }
        }
      ]
    },
    {
      "id": "t_c3…", "index": 2, "name": "La Synth", "color": "#B98CFF",
      "kind": "instrument",
      "instrument": { "kind": "user", "id": "i_55…" },
      "mixer": { "gainDb": -6.0, "pan": 0.0, "mute": false, "solo": false },
      "monitor": "off", "fx": [], "clips": []
    }
  ],
  "userInstruments": [
    {
      "id": "i_55…", "name": "Tiếng la",
      "source": "instruments/i_55…/source.caf",
      "rootNote": 57, "cents": 12, "confidence": 0.91,
      "mode": "natural",
      "envelope": { "a": 0.005, "d": 0.2, "s": 0.8, "r": 0.3 }
    }
  ],
  "master": { "gainDb": 0.0, "eq3": [0, 0, 0], "eq3Bypass": false, "limiterCeilingDb": -0.3 },
  "midiMappings": [
    { "src": { "device": "Launchpad X", "kind": "note", "channel": 0, "number": 81 },
      "target": { "kind": "clip", "track": 0, "slot": 0 } }
  ],
  "link": { "enabled": false, "startStopSync": true },
  "launchLog": []
}
```

### Quy ước
- `transport.tempoMode`: `"fixed"` | `"firstLoop"` (pedal mode, 04 §2.5). **Độ dài thu** (1/2/4/8 bar | Tự do) là **thiết lập chung của app** (`settings.json`), không lưu trong project. Pedal mode lúc chờ vòng đầu thì luôn thu kiểu tự do.
- `tracks[].index` từ 0 đến 7, `clips[].slot` từ 0 đến 7. Ô không có clip thì **không xuất hiện** trong mảng.
- Tham số FX có key là `paramId` dạng chuỗi (04 §9).
- `notes`: `p` = pitch, `v` = velocity 1–127, `s` = start (beat), `d` = duration (beat).
- Đường dẫn file luôn **tương đối so với thư mục project**, riêng nhạc cụ trong Library thì tương đối so với `libraryDir`.
- **Loop lấy từ Library** được **chép vào** `<project>/audio/<clipId>.wav` khi gán vào clip, để project tự đủ file (chia sẻ được, không vỡ khi Library đổi). **Kit và nhạc cụ SFZ thì không chép**, chỉ tham chiếu theo đường dẫn trong Library. Vì vậy `id` và đường dẫn của mục trong Library phải **giữ ổn định** giữa các phiên bản app.

---

## 3. Phiên bản & migration

- `schemaVersion` là số nguyên. App mới **đọc được mọi version cũ** qua chuỗi migrator phía Dart: `v1 → v2 → …`.
- Gặp version **mới hơn** app đang chạy: mở ở chế độ chỉ đọc và cảnh báo người dùng.
- Mỗi migrator có test round-trip với file mẫu trong `app/test/fixtures/projects/`.

---

## 4. Thư viện âm thanh: tập con SFZ

Dùng SFZ vì đây là định dạng text mở và có sẵn nhiều nhạc cụ miễn phí. Chỉ hỗ trợ các opcode dưới đây, **opcode khác bị bỏ qua kèm cảnh báo trong log**.

| Header | Opcode hỗ trợ |
|---|---|
| `<control>` | `default_path` |
| `<global>` / `<group>` / `<region>` | `sample`, `lokey`, `hikey`, `key`, `pitch_keycenter`, `lovel`, `hivel`, `tune`, `volume`, `pan`, `loop_mode` (`no_loop` / `one_shot` / `loop_continuous`), `loop_start`, `loop_end`, `ampeg_attack`, `ampeg_decay`, `ampeg_sustain`, `ampeg_release`, `group`, `off_by` |

Ví dụ drum kit:
```
<control> default_path=samples/
<global> loop_mode=one_shot ampeg_release=0.05
<region> key=36 sample=kick.flac
<region> key=38 sample=snare.flac
<region> key=42 sample=hat_closed.flac group=1 off_by=2
<region> key=46 sample=hat_open.flac   group=2 off_by=1
```

### `Library/manifest.json`
```json
{
  "version": 1,
  "kits":        [ { "id": "808", "name": { "en": "808 Classic", "vi": "808 Cổ điển" }, "path": "kits/808/808.sfz", "tags": ["hiphop"], "license": "CC0" } ],
  "instruments": [ { "id": "epiano", "name": "E-Piano", "path": "instruments/epiano/epiano.sfz", "range": [36, 96], "license": "CC0" } ],
  "loops":       [ { "id": "funk_01", "name": "Funk Drums 1", "file": "loops/funk_01.flac",
                     "bpm": 100, "beats": 8, "key": null, "tags": ["drums"], "defaultWarp": "repitch", "license": "CC0" } ]
}
```
- `name` là object theo ngôn ngữ `{en, vi}`. Thiếu ngôn ngữ nào thì dùng `en`. `tags` là **id tiếng Anh cố định** (`drums`, `bass`…), UI map sang nhãn đã dịch qua ARB, dùng một khoá select `libraryTag` (id lạ thì hiện nguyên id).
- Clip gán từ Library **chép `tags` vào `clips[].tags`** (06 §2), để gợi ý "loop trống → Re-Pitch" vẫn còn sau khi mở lại project. Take tự thu thì `tags: []`.
- Sample trong bundle: **FLAC 24-bit / 48 kHz**. Dùng mono nếu nguồn là mono. Decode sang float32 lúc nạp (trên worker).
- Mỗi mục có trường `license`. Toàn bộ file license được lưu ở `content/LICENSES/` trong repo và hiện ở màn "Giấy phép" trong app.

### Nguồn nội dung gợi ý (phải tự kiểm tra lại license từng bộ trước khi dùng)
| Nguồn | License | Dùng cho |
|---|---|---|
| VSCO-2 Community Edition | CC0 | Nhạc cụ dàn nhạc, piano |
| Salamander Grand Piano | CC-BY 3.0 (phải ghi công) | Piano |
| Freesound (lọc chỉ CC0) | CC0 | Drum hit, FX |
| Tự thu hoặc tự tổng hợp | Của bạn | Bass, pad, loop |

**Ngân sách dung lượng MVP:** 4 drum kit, 6–8 nhạc cụ, khoảng 40 loop, tổng ≤ 250 MB FLAC.

---

## 5. Lưu an toàn

1. Ghi vào `project.json.tmp` → `fsync` → đổi tên `project.json` hiện tại thành `.bak` → đổi tên `.tmp` thành `project.json`.
2. **Autosave:** 2 giây sau thay đổi cuối cùng (debounce), mỗi lần thu âm xong, và khi app chuyển xuống nền (`AppLifecycleState.paused`).
3. File audio được engine ghi xong **trước** khi model trỏ tới nó (event `RECORDING_FINISHED` chỉ phát sau khi đóng file).
4. Khi mở project: `project.json` hỏng thì thử `.bak`. File audio thiếu thì đánh dấu clip "missing" (UI hiện màu xám), không crash.
5. Dọn rác: khi lưu, xoá các `audio/*.caf` không còn clip nào tham chiếu, nhưng chỉ khi file đó cũ hơn 24 giờ (tránh xoá nhầm take vừa thu).

---

## 6. Dựng lại engine khi mở project

```
Dart: ProjectRepository.load(dir) → Project (sau migrate)
Dart: engine.call(project.open {dir})
      engine.send(SET_BPM), call(transport.setTimeSignature), send(SET_QUANTIZE), send(METRONOME),
      send(SET_COUNT_IN), send(MASTER_GAIN)
      send(FX_PARAM track -1 slot 0 p0..2) nếu master.eq3 có band ≠ 0 · send(FX_PARAM track -1 slot 1 p0) nếu limiterCeilingDb ≠ -0.3
      với mỗi userInstrument: call(instrument.createFromRecording {… mode, rootNote}) → jobId (dùng cache nếu có)
                              + call(instrument.setEnvelope {…}) ngay sau đó, không chờ job (engine đã đăng ký id)
                              ← chạy TRƯỚC vòng track, vì track.setInstrument {kind:"user"} cần nhạc cụ đã tồn tại
      với mỗi track:
        call(track.configure) → [instrument] call(track.setInstrument)  → jobId
        send(TRACK_GAIN/PAN/MUTE/SOLO/MONITOR)
        với mỗi fx: call(fx.set {…, bypass})
        với mỗi clip: audio → call(clip.setAudio) → jobId | midi → call(clip.setMidi)
      call(midi.setMappings), call(link.enable)
Dart: đợi mọi jobId (hiện progress "Đang mở project… 7/12") → sẵn sàng
```
Chuỗi lệnh này cũng là **định dạng kịch bản** của harness (08 §3.2), nên cùng một đường đi được test ở cả hai phía.
