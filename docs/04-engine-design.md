# 04 — Thiết kế engine chi tiết (LoopCore)

> Hằng số MVP: `LE_MAX_TRACKS = 8`, `LE_MAX_SCENES = 8`, sample rate theo thiết bị (thường là 48 kHz), block tối đa `maxBlock = 1024` (thực tế 128–256), voice pool gồm 64 voice và 8 voice dự phòng để fade khi bị cướp.
> Mọi mục có nhãn **[RT]** đều chạy trên audio thread và phải tuân thủ 08 §4.

---

## 1. Vòng lặp xử lý một block [RT]

```cpp
void RtEngine::process(const float* const* in, float* const* out, int numFrames,
                       const CallbackContext& ctx) noexcept [[clang::nonblocking]]
{
    cpuMeter.begin();
    adoptPendingSnapshot();           // §03-4.2: nhận snapshot mới nếu có, thu hồi bản cũ
    drainRtCommands(maxPerBlock=64);  // lệnh từ UI (launch, note, gain...)
    drainMidiInput(ctx);              // nốt và CC từ controller, gắn offset theo sample
    transport.beginBlock(numFrames, ctx);   // tính beat đầu/cuối block (P4: lấy theo Link)

    // Chia block tại các "ranh giới sự kiện": quantize, loop point, nốt MIDI, bắt đầu/kết thúc thu
    for (const Segment& seg : splitter.split(transport, scheduler, numFrames)) {
        scheduler.applyDue(seg.startBeat, rtState);        // clip queued → playing/stopped/recording
        for (int t = 0; t < LE_MAX_TRACKS; ++t)
            tracks[t].render(seg, in, rtState, *snapshot); // nguồn → FX → bus của track
        metronome.render(seg);
    }
    mixer.sumToMaster(out, numFrames);   // gain, pan, mute/solo, limiter
    recorder.capture(in, numFrames);     // ghi vào buffer cấp phát sẵn
    meters.update(out, in);
    publisher.publish(rtState, transport, meters, cpuMeter.end());
}
```
- **Segment** gồm `{startFrame, numFrames, startBeat, endBeat}`. Mỗi block có tối đa 16 segment. Nếu nhiều hơn thì gộp các sự kiện gần nhau (sai lệch ≤ 1 sample).
- Mọi mảng tạm (buffer của track, segment list) được cấp phát trong `prepare(sampleRate, maxBlock)`, **không bao giờ** cấp phát trong `process`.
- **Block lớn hơn `maxBlock`** (iOS có thể gửi, xem 03 §8): `process` tự **chia thành nhiều đoạn ≤ `maxBlock`** rồi xử lý lần lượt. Không crash, không cấp phát, không bỏ sample. Có test với callback 4096 frame khi `maxBlock` = 1024.

---

## 2. Transport & Clock

### 2.1 Cơ sở thời gian
- `int64 samplePos`: số sample đã phát kể từ lúc Play. Chỉ tăng, không bao giờ lùi.
- **Đơn vị beat = nốt 1/den** của nhịp: 4/4 thì beat là nốt đen, 6/8 thì beat là nốt móc đơn. `beatsPerBar = num`, BPM đếm theo đơn vị đó (6/8 ở 120 BPM là 120 móc đơn mỗi phút). Quantize 1/16 ở 6/8 = 0.5 beat. Metronome click theo từng beat, phách mạnh ở đầu bar.
- `double beatAtBlockStart`: tính lại **mỗi block** từ `samplePos` và BPM (có neo lại khi tempo đổi), **không cộng dồn** giá trị float qua từng block để tránh trôi.
- `samplesPerBeat = 60.0 * sampleRate / bpm`.
- Khi BPM đổi, lưu neo `(anchorSample, anchorBeat)`. Từ đó `beat(s) = anchorBeat + (s - anchorSample) / samplesPerBeat`.

### 2.5 Pedal mode (`FromFirstLoop`, người dùng chọn 29/09)
- Bật bằng `transport.setTempoMode {mode:"firstLoop"}`, lưu ở `transport.tempoMode` (06 §2). Chế độ ở trạng thái **"chưa có tempo"** khi project **chưa có clip nào**.
- **Vòng đầu tiên** (khi chưa có tempo):
  - `CLIP_RECORD(i0 = 0)` thu **ngay lập tức**: không quantize, không count-in, không metronome. Transport chưa chạy.
  - `RECORD_STOP` chốt vòng: độ dài T (sample, đã bù latency như §5.3) **chính xác theo lúc bấm**, không làm tròn.
  - Engine suy ra tempo: chọn số beat `nb = beatsPerBar · 2^k` (k ≥ 0) sao cho `bpm = 60·nb·sr/T` nằm trong **[80, 160)**. Nếu T quá ngắn để có bpm < 160 ngay cả với 1 bar thì lấy `nb = beatsPerBar`, và bpm tối đa là 300.
  - Đặt `bpm`, gán `lengthBeats = nb`, rồi **khởi động transport sao cho beat 0 trùng sample đầu của take**. Clip phát tiếp ngay, liền mạch, không có khoảng trống. Phát event `TEMPO_CHANGED(value = bpm)` (dùng lại mã sẵn có) và `RECORDING_FINISHED`.
- **Các vòng sau** (đã có tempo): thu tự do như §5.2, nhưng độ dài được **làm tròn lên bội số của vòng đầu** (tính theo beat). Bắt đầu thu theo quantize (mặc định 1 bar). Metronome theo cài đặt.
- **Trở về "chưa có tempo"** khi transport dừng **và** mọi clip của project đã bị xoá. `project.open` thì đọc lại từ model.
- Đổi BPM bằng tay sau khi đã có tempo: được, đi theo warp hybrid (§10). Nó **không** đưa project về trạng thái chưa có tempo.

### 2.2 Chia block (BlockSplitter)
Đầu vào: `[beatStart, beatEnd)` của block. Tập ranh giới gồm:
- Ranh giới quantize kế tiếp, nếu có lệnh nào đang ở trạng thái Queued (§3.2)
- Điểm kết thúc loop của từng clip đang phát
- Nốt MIDI trong clip (§7) và nốt MIDI từ controller (đã có sample offset)
- Điểm bắt đầu hoặc kết thúc thu

Mỗi ranh giới được đổi ra frame offset: `offset = round((b - beatStart) * samplesPerBeat)`, rồi clamp vào `[0, numFrames)`.

### 2.3 Metronome & count-in
- Tiếng click được **tổng hợp sẵn** trong `prepare()`: sine 1.5 kHz (phách mạnh) và 1 kHz (phách thường), dài 30ms, có envelope. Lưu vào 2 buffer.
- Các tuỳ chọn: bật/tắt, chỉ kêu khi thu, âm lượng.
- Count-in (0/1/2 bar): lệnh thu đợi tới ranh giới quantize, sau đó đợi thêm N bar trong khi metronome kêu.

### 2.4 Chế độ tempo
| Chế độ | MVP | Mô tả |
|---|---|---|
| `Fixed` | ✅ | BPM do người dùng đặt (20–300), có tap tempo ở phía UI |
| `Link` | ✅ (P4) | Beat và BPM lấy theo phiên Link (§14) |
| `FromFirstLoop` | ✅ (thêm vào MVP 29/09) | Pedal mode: take đầu tiên quyết định BPM, xem §2.5 |

---

## 3. ClipScheduler

### 3.1 Máy trạng thái của clip
```
            launch(q)                 boundary                 loop...
 Stopped ──────────────► QueuedPlay ─────────────► Playing ◄──────────┐
    ▲                        │ stop trước khi tới      │ └──────────────┘
    │ boundary               ▼                         │ stop(q) / launch clip khác cùng track
 QueuedStop ◄──────────────────────────────────────────┘
 Empty ──record(q)──► QueuedRecord ──boundary(+countIn)──► Recording ──đủ độ dài / stop──► Playing
 Playing ──overdub──► Overdubbing ──overdub lần nữa──► Playing
```
- `LeClipState`: `Empty=0, Stopped=1, QueuedPlay=2, Playing=3, QueuedStop=4, QueuedRecord=5, Recording=6, Overdubbing=7`.
- **Launch vào ô trống** (không có clip, track không arm) = **dừng track** theo quantize, giống scene có ô trống (khớp với UI 07 §3.1).
- **Launch lại clip đang Playing** = **retrigger**: phát lại từ đầu tại ranh giới quantize kế tiếp (chế độ Trigger mặc định của Ableton).
- **Mỗi track chỉ có 1 clip Playing hoặc Recording tại một thời điểm.** Launch clip B trên track đang phát A: A chuyển sang QueuedStop, B chuyển sang QueuedPlay. Cả hai đổi trạng thái tại cùng một ranh giới.

### 3.2 Toán quantize
```
q = quantizeBeats  // None=0, 1/16=0.25, 1/8=0.5, 1/4=1, 1/2=2, 1 bar=beatsPerBar, 2 bar, 4 bar
boundary(now) = (q == 0) ? now : ceil((now - ε) / q) * q     // ε = 1e-9: đang đứng đúng ranh giới thì dùng luôn
```
- Lệnh được ghi `targetBeat = boundary(beatNow)` ngay lúc audio thread nhận lệnh. Khi segment chạm `targetBeat` thì áp dụng.
- **Transport đang dừng mà nhận launch:** Play bắt đầu tại beat 0, lệnh áp dụng ngay tại beat 0.
- **Clip bắt đầu phát ở beat nào:** phase của clip được tính theo `launchBeat`, tức là clip luôn **bắt đầu từ đầu** khi launch. Tuỳ chọn "legato" (giữ phase) để phase 2.

### 3.3 Scene
`SCENE_LAUNCH(s)`: với mọi track `t`, nếu ô `(t,s)` có clip thì launch clip đó. Nếu ô trống thì **dừng track** (hành vi mặc định của Ableton). Tất cả dùng cùng một `targetBeat`.

### 3.4 Stop
- `CLIP_STOP(track)` → QueuedStop theo quantize.
- `STOP_ALL` → mọi track chuyển sang QueuedStop.
- `TRANSPORT_STOP` → dừng ngay, fade 5ms, reset `samplePos`.
  - Clip đang **Recording** → bỏ take dở, ô trở về Empty (hoặc về clip cũ nếu trước đó có).
  - Clip đang **Overdubbing** → kết thúc overdub, **giữ** phần đã chồng (undo được).
  - Clip Queued* → huỷ lệnh đang chờ.

### 3.5 LaunchLog (chuẩn bị cho Arrangement)
Ring buffer 4096 sự kiện `{beat, track, slot, kind}`, audio thread ghi vào. Main thread copy ra định kỳ. UI đọc qua `launchLog.read` (05 §3). MVP chỉ lưu vào project, chưa dùng.

---

## 4. AudioClipPlayer [RT]

- Dữ liệu clip gồm `AudioData` (float, mono hoặc stereo, theo sample rate của file) và `lengthBeats`, `originalBpm`, `warpMode`, `gain`. Nếu warp đã render xong thì có thêm `stretchedData` (§10).
- **Vị trí phát được tính từ beat, không cộng dồn:**
  ```
  beatInClip = fmod(beatNow - launchBeat, lengthBeats)
  Re-Pitch:  srcPos = beatInClip * (60 * fileSR / originalBpm)       // tốc độ đổi theo BPM
  Stretched: srcPos = beatInClip * (60 * fileSR / currentBpm)        // buffer đã co giãn theo BPM hiện tại
  ```
  Rồi đọc bằng nội suy Hermite 4 điểm. Nếu tỷ lệ = 1 và sample rate trùng thì đọc thẳng, không nội suy.
- **Chống click:** fade-in 2ms khi bắt đầu, fade-out 5ms khi dừng. Tại điểm loop, crossfade 2ms nếu hai đầu clip không khớp (take thu âm luôn có).
- Mono thì phát ra 2 kênh. Pan được xử lý ở mixer.

---

## 5. Recorder

### 5.1 Arm và bộ nhớ
- `track.arm = true` trên main thread → cấp phát `RecordBuffer` với độ dài `maxTakeSeconds (mặc định 64s) + marginLatency`. Buffer là mono hoặc stereo theo input.
- Mỗi track được arm có một buffer. Tổng RAM giới hạn khoảng 8 track × 64s × 48k × 4B ≈ 98 MB (mono), chấp nhận được.

### 5.2 Độ dài take
| Kiểu | Cách hoạt động |
|---|---|
| Cố định N bar (1/2/4/8) | Thu đúng `N * beatsPerBar` beat, rồi tự chuyển sang Playing |
| Tự do | Thu cho tới khi người dùng bấm lần nữa. Độ dài được **làm tròn lên** tới bội số của quantize gần nhất (tối thiểu 1 bar) |

### 5.3 Bù latency (bắt buộc) [RT]
Âm người dùng chơi khớp với tiếng họ **nghe** thấy, nhưng tới engine muộn hơn đúng round-trip `L`:
```
L = device.inputLatency + device.outputLatency + calibrationOffset   // đơn vị sample
Bắt đầu thu tại sample B (ranh giới). Ghi thêm L sample sau điểm kết thúc.
Take cuối cùng = buffer[L ... L + lengthSamples)   → bỏ L sample đầu
```
- `calibrationOffset` lấy từ màn hiệu chỉnh (P4-12): phát click, thu lại qua mic, tìm độ lệch bằng cross-correlation.
- Khi route thay đổi thì đọc lại latency của device. Có Bluetooth thì hiện cảnh báo (latency thường > 150ms, không hợp để thu).

### 5.4 Overdub & undo
- Overdub trên clip audio đang Playing: tại vị trí phát hiện tại, `clip[i] = clip[i] + input[i]` (đã bù latency, ghi vào đúng vị trí của vòng lặp).
- Không sửa buffer đang phát tại chỗ khi snapshot khác cũng đang tham chiếu tới nó. Cách làm: khi bắt đầu overdub, main thread **đã chuẩn bị sẵn** một bản sao (`overdubTarget`) và giữ bản gốc làm `undoLayer`. Audio thread ghi vào `overdubTarget` và phát từ đó.
- Undo 1 lớp: swap về `undoLayer`, làm qua lệnh cấu trúc sinh ra snapshot mới. `project.open` và `project.close` xoá lớp undo.
- **Giới hạn MVP (P1-22):** overdub **audio** chỉ chạy khi clip đang phát **1:1** (cùng sample rate, `originalBpm` bằng BPM hiện tại). Clip đang Re-Pitch thì không vào OVERDUBBING, và phát `LE_EVT_ERROR(a = LE_ERR_OVERDUB_UNSUPPORTED, b = track, value = slot)`. **Clip MIDI thì vẫn overdub nốt** (P1-30). Overdub audio lên clip đã stretch để phase 2.
- **Giao thức "vé" (sửa R1):** mỗi lượt overdub có một vé `{session, slot, target}`. Main tạo vé và giữ target sống cho tới khi RT trả vé qua `OverdubFinished{session}`. Vé được trao bằng atomic exchange ở cả hai phía, nên mọi lúc chỉ một bên giữ vé. Bật lại khi RT chưa trả vé cũ → main **hoãn** lệnh bật. Mỗi lượt (kể cả MIDI) kết thúc đều phát `RECORDING_FINISHED(track, slot, value)`, để app đọc lại clip (audio: `clip.info` + peaks, MIDI: `clip.getMidi`).
- Vị trí ghi: `p = (t − L − launchSample) mod lenFrames` (có bù latency L như §5.3).

### 5.5 Input monitoring
Tuỳ chọn `Off / Auto / On` cho mỗi track. `Auto` = chỉ nghe khi track được arm **và** đang dùng tai nghe có dây hoặc interface. Mặc định là Off khi dùng loa trong máy (tránh hú).

### 5.6 Ghi đĩa
Thu xong → buffer được chuyển qua `rtToNrt` → main thread gắn vào clip (snapshot mới) → worker ghi file `audio/<clipId>.caf` (float32) → tính peaks (§5.7) → event `RECORDING_FINISHED(track, slot)`.
- **Hiện thực MVP (P1-21):** take nằm trong RAM cho tới khi thu xong, sau đó worker ghi cả file một lần bằng `io/CafWriter` tự viết (JUCE không ghi được CAF). Chưa dùng `ThreadedWriter`, vì chỉ cần khi cho phép take dài hơn 64 giây.
- Vòng đầu tiên sau khi thu được phát **thẳng từ buffer thu**. Khi snapshot có bản copy thì crossfade cùng pha sang, nên không mất tiếng ở vòng đầu.

### 5.7 PeakBuilder
Tính trên worker. Có 3 mức: 256, 2048, 16384 sample/điểm, mỗi điểm là cặp `(min, max)` float. Kết quả cache trong RAM và cả trên đĩa (`cache/<clipId>.peaks`). UI lấy qua `le_get_peaks`.

---

## 6. Sampler (một engine dùng cho mọi nguồn âm) [RT]

### 6.1 Dữ liệu
```cpp
struct Zone { int loKey, hiKey, loVel, hiVel; int rootKey; float tuneCents;
              const AudioData* data; float gainDb, pan;
              LoopMode loopMode;              // NoLoop | OneShot | LoopContinuous (SFZ loop_mode)
              int64 loopStart = -1, loopEnd = -1;
              Adsr::Params env;               // theo từng zone (SFZ ampeg_* của region)
              int group = 0, offBy = 0; };    // choke theo zone (SFZ group / off_by)
struct Instrument { std::vector<Zone> zones;  // sắp theo loKey; giá trị <global>/<group> đã được kế thừa xuống zone khi nạp
                    enum Mode { Natural, Classic } mode; int classicZone; };
```

### 6.2 Voice
```cpp
struct Voice { const Zone* zone; double pos, inc; float gain, velGain;
               Adsr env; int note; uint32 age; bool active, stealing; };
```
- `inc = 2^((note - rootKey + tuneCents/100) / 12) * (zoneSR / engineSR)`
- Nội suy Hermite 4 điểm. Voice hết dữ liệu (không loop) hoặc envelope về 0 thì chuyển sang inactive.
- **Voice stealing:** khi đủ 64 voice active, chọn voice ở pha release có mức thấp nhất, nếu không có thì chọn voice lâu nhất. Voice bị cướp được đánh dấu `stealing` và fade 3ms trong một **voice dự phòng** (8 voice). Nốt mới dùng slot vừa giải phóng.
- Chọn zone: `loKey ≤ note ≤ hiKey` và `loVel ≤ vel ≤ hiVel`. Nếu có nhiều zone thoả thì chọn zone đầu tiên (MVP chưa có round-robin).
- **Choke group** (hi-hat mở và đóng): một nốt mới trong group dừng nhanh (5ms) các voice khác cùng group.
- **Chế độ Classic:** chỉ dùng `classicZone` (zone gốc) cho mọi phím, bất kể khoảng cách.

### 6.3 Envelope
ADSR: attack tuyến tính, decay và release theo hàm mũ (hệ số tính sẵn khi đổi tham số, không tính `exp` mỗi sample). Drum one-shot: bỏ qua note-off, phát hết sample.

### 6.4 Nguồn instrument
| Nguồn | Nạp bằng |
|---|---|
| Drum kit, nhạc cụ có sẵn | `SfzLoader` (tập con SFZ, 06 §4) chạy trên worker |
| Tiếng người dùng tự thu | `PitchRenderer` (§8) |

---

## 7. MIDI clip [RT]

- Dữ liệu: danh sách `Note { uint8 pitch, vel; double startBeat, lengthBeats; }` sắp theo `startBeat`, nằm trong snapshot (bất biến).
- **Phát:** mỗi segment tìm nốt có `startBeat` trong `[segStart, segEnd)` theo toạ độ trong clip (có xử lý vòng loop) và gửi note-on sang sampler của track, đúng offset. Note-off được xếp vào **bảng chờ note-off cố định** (128 phần tử cho mỗi track).
- **Loop:** tại điểm loop, mọi nốt còn đang kêu vượt quá điểm cuối clip được cắt bằng note-off.
- **Stop clip:** gửi note-off cho mọi nốt đang kêu (all-notes-off của track đó).
- **Thu MIDI:** audio thread ghi `NoteEvent {beat, pitch, vel, on/off}` vào ring buffer cố định (4096). Thu xong, main thread ghép on/off thành Note, áp **quantize khi thu** (tuỳ chọn: tắt, 1/16, 1/8), dựng clip rồi phát event.
- **Overdub MIDI:** trộn nốt mới vào clip hiện có (phía main), rồi tạo snapshot mới.
- **Sửa clip đang phát (`clip.setMidi` từ piano roll):** vẫn giữ phase. Nốt đang kêu mà **không còn** trong nội dung mới thì note-off ngay, có fade release. Nốt mới có `startBeat` đã qua trong vòng hiện tại thì kêu từ vòng sau. Không nốt treo, không phát trùng.

---

## 8. Pipeline biến tiếng tự thu thành sampler (worker)

```
file thu (mono) ─► SilenceTrimmer (ngưỡng -45 dBFS, giữ 5ms pre-roll)
               ─► Yin: frame 2048, hop 512, ngưỡng 0.12, dải 50–1500 Hz
                   → median của các frame "voiced" ổn định → rootNote + cents + confidence
               ─► nếu confidence < 0.6: hỏi người dùng chọn nốt gốc (UI), mặc định C4
               ─► PitchRenderer: 13 zone, dịch -18, -15, ... , +15, +18 nửa cung so với gốc
                   mỗi zone: Signalsmith Stretch (offline: seek → process → flush)
                     setTransposeSemitones(k - cents/100)   // đồng thời chỉnh cho đúng cao độ tuyệt đối
                     giữ formant: setFormantFactor(1, compensatePitch=true) + setFormantBase(f0 / sampleRate)
                   ─► ĐO LẠI cao độ của từng zone bằng Yin → ghi độ lệch vào Zone.tuneCents (bù sai số của stretch)
                   ─► CHUẨN HOÁ âm lượng từng zone (RMS) về mức của zone gốc, kẹp ±12 dB
                   (nếu Yin không chắc cao độ của một zone thì KHÔNG bù tuneCents cho zone đó)
                   zone k áp dụng cho các phím [root+k-1, root+k+1]
               ─► Instrument(mode=Natural, classicZone = zone 0) → snapshot mới
               ─► cache: instruments/<id>/zone_<k>.caf (render lại được khi thiếu)
```
- **Kết quả P0-09 (agent 80, đo trên Mac, 29/09):**
  - **Thời gian:** 13 zone cho mẫu 4 giây mất khoảng 240ms khi tắt formant, khoảng 380ms khi bật formant.
  - **Giữ formant hoạt động:** nguyên âm dịch +12 nửa cung, trọng tâm phổ vẫn ở 1376 Hz (gốc 1437 Hz). Khi tắt formant, trọng tâm nhảy lên 2887 Hz, nghe như chipmunk.
  - **Cao độ (đã đính chính sau P3-01, đo bằng Yin):** âm có hoạ âm (nguyên âm, giọng hát) lệch **≤ 1.7 cent**, cả khi bật lẫn tắt formant.
    - Mức lệch 10–25 cent **chỉ** gặp với âm gần như sine (huýt sáo, "uuu" nhẹ) ở các zone thấp.
    - Block/interval 200/50ms giảm lệch xuống ≤ 8 cent nhưng có thể làm phụ âm bị nhoè → chọn bằng tai ở P0-12.
    - Vẫn giữ bước đo lại bằng Yin rồi ghi vào `tuneCents` (tốn khoảng 85ms cho 13 zone trên Mac).
    - Zone −18 với formant bật có dấu hiệu lệch lớn khi thử bằng giọng TTS: **phải kiểm lại bằng giọng hát thật** (fixture `voice_la_*.wav`).
  - **⚠️ Âm lượng:** bật formant thì zone cao nhỏ tiếng hơn (zone +18 hụt khoảng −12 dB), nên bắt buộc phải có bước chuẩn hoá.
  - Chi tiết: `engine/tools/docs/signalsmith-notes.md`.
- Ngân sách: mẫu 4 giây → 13 zone < 2 giây trên A12. Có event tiến độ để hiện progress bar.
- RAM: 13 × 4s × 48k × 4B ≈ **10 MB** mỗi instrument (mono).
- Phím nằm ngoài ±19 nửa cung dùng zone gần nhất rồi resample thêm (chấp nhận nghe hơi "chipmunk").

---

## 9. Processor interface & FX [RT]

```cpp
struct ProcessContext { int numFrames; double sampleRate, bpm, beat; bool playing; };

class Processor {
public:
    virtual ~Processor() = default;
    virtual void prepare(double sampleRate, int maxBlock) = 0;                 // NRT: cấp phát ở đây
    virtual void reset() noexcept = 0;                                         // xoá state (đuôi reverb…)
    virtual void process(float* const* io, int numCh, const ProcessContext&) noexcept = 0; // RT
    virtual void setParam(int id, float value) noexcept = 0;                   // RT, chỉ đặt target cho smoothing
    virtual int  latencySamples() const noexcept { return 0; }
};
```
- `FxChain` mỗi track có **3 slot cố định** (xoá slot thì engine không dồn). Master **không có slot người dùng**: slot 0 = EQ3, slot 1 = Limiter, cố định, chỉ chỉnh tham số. Thêm hoặc bớt FX là **lệnh cấu trúc**: main thread tạo `Processor` mới, gọi `prepare()`, đưa vào snapshot. Xoá FX thì bản cũ được thu hồi qua ReleasePool.
- Tham số từ UI đi qua `LE_CMD_FX_PARAM(track, slot, paramId, value)` → `setParam` → `juce::SmoothedValue` (20ms).
- **Hành vi FxChain (P3-12, đã hiện thực):**
  - `fx.set` cùng loại với FX đang có ở slot → **giữ instance**, không cắt đuôi reverb/delay. Khác loại hoặc slot trống → tạo Processor mới trên main (create → prepare → setParam → reset), đưa vào snapshot, RT crossfade 20ms.
  - Main tự gắn `instanceId` vào `d0` của `FX_PARAM`/`FX_BYPASS`. RT chỉ áp lệnh cho đúng instance, lệnh tới trước snapshot thì được giữ tạm. `le_send` trả `false` nếu slot trống hoặc `paramId` không có ở loại FX đó.
  - **Bypass:** crossfade dry/wet 10ms. Khi đã bypass hẳn, processor vẫn chạy nhưng nhận im lặng, nên đuôi tắt tự nhiên. **Bypass không giảm CPU**: tải tệ nhất không phụ thuộc bypass.
  - `TRANSPORT_STOP` không reset FX, đuôi vẫn ngân. Sample rate đổi thì prepare lại mọi Processor, giữ nguyên tham số. Processor được gọi theo khúc ≤ 256 frame.
  - **Master:** EQ3 phẳng thì hoàn toàn không xử lý (output giống hệt từng bit, golden cũ vẫn khớp). **Được bypass EQ master** (`FX_BYPASS track -1 slot 0`), nhưng **Limiter master không bao giờ bypass được**, vì đây là lớp bảo vệ cuối cùng.

| FX | Tham số (id: dải) | Hiện thực |
|---|---|---|
| Filter | 0 mode (LP/HP/BP), 1 cutoff 20–20k (log), 2 resonance 0.5–10 | `juce::dsp::StateVariableTPTFilter` |
| Delay | 0 time (sync: 1/16…1/2, có chấm, triplet), 1 feedback 0–0.95, 2 mix, 3 ping-pong | `juce::dsp::DelayLine` (prepare tối đa 4s) + LP trong vòng feedback |
| Reverb | 0 size, 1 damping, 2 width, 3 mix | `juce::dsp::Reverb` |
| EQ3 | 0 low gain (shelf 200Hz), 1 mid gain (peak 1k, Q 0.7), 2 high gain (shelf 5k) ±15 dB | Biquad **tự tính hệ số** (xem cảnh báo) |
| Compressor | 0 threshold, 1 ratio, 2 attack, 3 release, 4 makeup | `juce::dsp::Compressor` |
| Limiter (master) | 0 ceiling -0.3 dBFS, 1 release | `juce::dsp::Limiter` |

> ⚠️ **Cảnh báo:** `juce::dsp::IIR::Coefficients::make*()` **cấp phát bộ nhớ**, vì chúng trả về `ReferenceCountedObjectPtr`. Không được gọi trên audio thread. Với EQ, tự tính hệ số biquad theo RBJ Audio EQ Cookbook vào mảng `float[5]`. Việc này chỉ là toán, an toàn trên RT.
> Delay đồng bộ tempo: thời gian delay tính lại khi BPM đổi, có smoothing để không nghe tiếng "zipper".

---

## 10. Warp hybrid khi đổi BPM

```
BPM đổi (UI hoặc Link)
 ├─[RT] ngay lập tức: mọi clip audio dùng Re-Pitch (§4). Tiếng liên tục, cao độ lệch tạm thời
 └─[NRT] debounce 300ms → với mỗi clip có warp = stretch và originalBpm ≠ bpm:
        WarpRenderer (worker): Signalsmith, ratio = originalBpm / newBpm, giữ cao độ
        → stretchedData(bpm = newBpm) → snapshot mới
[RT] khi snapshot có stretchedData khớp BPM hiện tại:
        đợi tới ranh giới 1 bar kế tiếp → crossfade equal-power 10ms từ Re-Pitch sang Stretched
```
- Nếu BPM lại đổi trong lúc đang render: huỷ job cũ (`job.cancel`) rồi xếp job mới sau debounce.
- Mỗi clip giữ tối đa 1 `stretchedData` (theo BPM gần nhất). Có thêm bản đệm tạm thời trong lúc chuyển đổi. RAM tối đa khoảng 2× dữ liệu clip.
- Loop có sẵn trong thư viện (đã biết `originalBpm`) đi theo đúng đường này ngay khi được gán vào clip.
- **Hiện thực (P3-08/10/11):**
  - Cache `<project>/cache/stretched/<clipId>@<bpm>.caf` được kiểm theo độ dài + sample rate. Mỗi clip giữ **1 bản stretched gần nhất**, nên quay lại BPM cũ thì dùng ngay, không render lại.
  - Khi audio đổi (overdub, undo, setAudio) thì bản stretched bị bỏ.
  - `clip.setAudio` hoặc `setParams warp = stretch` render **ngay** khi gán, không debounce.
  - Offline: debounce tính theo số frame đã render, nên kết quả tất định.
- **Giới hạn đã biết:** phase vocoder làm nhoè đầu tiếng của âm có transient mạnh (loop trống bị pre-echo khoảng −27 dB trong 10ms trước click). Xử lý:
  - (a) Loop trống trong thư viện khai `defaultWarp: "repitch"` (06 §4), UI gợi ý Re-Pitch cho clip trống.
  - (b) WarpRenderer đo mật độ onset rồi chọn preset block/interval ngắn (khoảng 40/10ms) cho clip nhiều transient.
  - (c) Chế độ "Beats" (warp theo lát cắt) để **phase 2**.

---

## 11. Mixer & master [RT]

- Mỗi track: `gain` (dB, -inf…+6), `pan` (luật -3 dB), `mute`, `solo`. Có solo thì các track không solo bị mute.
- Mute và solo dùng ramp 5ms. Gain và pan đi qua SmoothedValue.
- Master: gain → EQ3 → Limiter → output. Meter peak có ballistics: giữ đỉnh 1 giây, hạ 20 dB/giây (tính ở phía UI từ giá trị peak thô).

---

## 12. Export (worker)

| Kiểu | Cách làm |
|---|---|
| **Ghi lại buổi jam** (real-time) | Ghi master bus trong lúc chơi, dùng cùng cơ chế Recorder cộng `ThreadedWriter` → `exports/jam_<time>.wav` |
| **Export scene** (offline) | Tạo một `RtEngine` offline với **cùng snapshot**, launch scene tại beat 0, render N bar với `maxBlock` 1024, nhanh hơn real-time và không ảnh hưởng tới phần đang phát live. Tuỳ chọn xuất stems theo từng track |
| Định dạng | WAV 24-bit (JUCE `WavAudioFormat`). M4A/AAC 256k: chuyển từ WAV bằng `AVAssetExportSession` hoặc `ExtAudioFile` (Obj-C++) |

---

## 13. MIDI input & MIDI learn

- `MidiInputRouter`: mở mọi `juce::MidiInput` được bật. Callback (thread CoreMIDI) đẩy `{timestampNs, bytes[3]}` vào SPSC `midiToRt`. **Không dùng `MidiMessageCollector`** vì bên trong có lock.
- [RT] Mỗi block, message được đổi timestamp sang frame offset (so với host time của block) rồi định tuyến:
  - Nốt → sampler của track **đang được chọn** (track chọn là tham số trong RtState)
  - Mapping learn → hành động (launch `(t,s)`, scene, play/stop, gain track, tham số FX)
- **MIDI learn** (NRT): UI bật learn cho một đích → message kế tiếp được báo qua event → lưu mapping vào `MidiLearnMap` trong snapshot.
- **Launchpad:** preset mapping (layout Programmer, lưới 8×8 map sang grid). Đèn LED phản hồi được gửi bằng `MidiOutput` từ **main thread** (Timer 30Hz) dựa trên `LeState.clipState`.
- **BLE MIDI:** màn ghép nối `CABTMIDICentralViewController` được mở từ Swift (plugin Flutter), không qua JUCE.

---

## 14. Ableton Link (LinkKit, P4)

- `LinkSync` giữ `ABLLinkRef`. Main thread gọi bật/tắt và mở màn settings (`ABLLinkSettingsViewController`, mở từ Swift).
- [RT] Mỗi block:
  ```
  state = ABLLinkCaptureAudioSessionState(link)
  hostTimeAtOutput = ctx.hostTime + outputLatencyTicks
  beatAtBlockStart = ABLLinkBeatAtTime(state, hostTimeAtOutput, quantum = beatsPerBar)
  bpm = ABLLinkGetTempo(state)
  nếu có lệnh SET_BPM từ UI:  ABLLinkSetTempo(state, bpm, hostTime); ABLLinkCommitAudioSessionState(link, state)
  nếu bpm khác block trước:     rtToNrt.push(TempoChanged{bpm})   → §10 warp
  ```
- Host time lấy từ `AudioIODeviceCallbackContext::hostTimeNs` của JUCE, đổi sang mach ticks. Plan B lấy từ `AudioTimeStamp.mHostTime`.
- Start/stop sync: bật theo tuỳ chọn. Play hoặc Stop được đồng bộ ở ranh giới quantum.
- Bắt buộc: entitlement `com.apple.developer.networking.multicast` và `NSLocalNetworkUsageDescription`, và phải qua **toàn bộ** test plan của LinkKit.

---

## 15. JobSystem (NRT)

- `JobSystem` bọc `juce::ThreadPool(2)`. Mỗi job có `jobId` (int64 tăng dần), `progress` (atomic float), `cancelFlag` (atomic bool).
- Job gửi tiến độ bằng event `JOB_PROGRESS` (tối đa 10 lần/giây), rồi `JOB_DONE` hoặc `JOB_FAILED`. Kết quả JSON lấy qua `le_call({"op":"job.result","jobId":…})`.
- Job **không bao giờ** chạm vào RtState. Kết quả được đưa vào engine bằng snapshot mới, tạo trên main thread (job gửi `MessageManager::callAsync`).

---

## 16. Ngân sách bộ nhớ (iPad 8, 3 GB RAM)

| Hạng mục | Ước tính |
|---|---|
| Drum kit + nhạc cụ đang nạp | ≤ 200 MB (float32 trong RAM, trên đĩa lưu FLAC) |
| Clip audio (8×8, trung bình 8 giây, mono) | 64 × 8s × 48k × 4B ≈ 98 MB (giới hạn thực tế thấp hơn nhiều) |
| Buffer thu cho track đã arm | ≤ 98 MB |
| Instrument từ tiếng thu | 10 MB mỗi cái, tối đa 8 |
| Bản stretch khi đổi BPM | ≤ 1× clip audio |
| **Tổng mục tiêu** | **< 600 MB**, có cảnh báo khi vượt 500 MB |
