# P1 — Engine core headless (W3–W14: 19/10/2026 – 08/01/2027)

**Mục tiêu:** engine hoàn chỉnh phần lõi, chạy được trên Mac qua harness và trên iPad qua shell spike. **Chưa có UI Flutter thật.**
**Đầu ra:** grid 8×8 (audio và MIDI clip), quantize, scene, thu âm có bù latency, overdub, sampler 64 voice, SFZ, MIDI clip, bộ golden test, **M1**.
**Spec chính:** 03, 04, 05, 08. Task **[RT]** bắt buộc chạy RTSan.

---

## Tổng quan task

| TT | Mã | Việc | Tuần | Ước lượng | Phụ thuộc |
|---|---|---|---|---|---|
| [x] | P1-01 | Khung engine: `Engine`, `DeviceIO`, `RtEngine::prepare/process` | W3 | 1.5d | M0 |
| [x] | P1-02 | Harness CLI: `render` / `play` | W3 | 1.5d | P1-01 |
| [x] | P1-03 | Render offline + scenario runner + hạ tầng golden | W3 | 2d | P1-02 |
| [x] | P1-04 | RtQueues + `le_send` [RT] | W4 | 1d | P1-01 |
| [x] | P1-05 | `CommandProcessor` + dispatch `le_call` JSON + JobSystem | W4 | 1.5d | P1-01 |
| [x] | P1-06 | GraphSnapshot + SnapshotBuilder + swap + ReleasePool [RT] | W4 | 1.5d | P1-04 |
| [x] | P1-07 | StatePublisher đầy đủ + event dispatch | W4 | 1d | P1-04 |
| [x] | P1-08 | Transport: BPM, neo, play/stop, toán beat [RT] | W5 | 1.5d | P1-06 |
| [x] | P1-09 | BlockSplitter [RT] | W5 | 1.5d | P1-08 |
| [x] | P1-10 | Metronome + count-in [RT] | W5 | 1d | P1-09 |
| [x] | P1-11 | AudioFileIO + job `clip.setAudio` | W6 | 1d | P1-05 |
| [x] | P1-12 | AudioClipPlayer [RT] | W6 | 2d | P1-09, P1-11 |
| [x] | P1-13 | Track + Mixer [RT] | W6 | 1d | P1-12 |
| [x] | P1-14 | Limiter master + meter [RT] | W6 | 0.5d | P1-13 |
| [x] | P1-15 | ClipScheduler: máy trạng thái + quantize [RT] | W7 | 2d | P1-09 |
| [x] | P1-16 | Scene launch, stop all, transport stop [RT] | W7 | 1d | P1-15 |
| [x] | P1-17 | LaunchLog | W7 | 0.5d | P1-15 |
| [x] | P1-18 | Arm + cấp phát RecordBuffer (main) | W8 | 0.5d | P1-06 |
| [x] | P1-19 | Recorder capture: độ dài cố định/tự do, count-in [RT] | W8 | 2d | P1-15, P1-18 |
| [x] | P1-20 | Bù latency [RT] | W8 | 1d | P1-19 |
| [x] | P1-21 | Ghi đĩa + `RECORDING_FINISHED` + `clip.info` | W8 | 1d | P1-19 |
| [x] | P1-22 | Overdub audio + undo 1 lớp [RT] | W9 | 2d | P1-21 |
| [x] | P1-23 | Input monitoring [RT] | W9 | 1d | P1-19 |
| [x] | P1-24 | PeakBuilder + `le_get_peaks` | W9 | 1d | P1-21 |
| [x] | P1-25 | Sampler: voice pool, zone, Hermite, stealing [RT] | W10 | 2d | P1-09 |
| [x] | P1-26 | ADSR + choke group + one-shot [RT] | W10 | 1d | P1-25 |
| [x] | P1-27 | SfzLoader (tập con) + job `track.setInstrument` | W10 | 1.5d | P1-11 |
| [~] | P1-28 | Fixture drum kit và nhạc cụ + golden | W10 | 0.5d | P1-27 |
| [x] | P1-29 | MidiClipPlayer: phát, bảng note-off, cắt ở loop [RT] | W11 | 1.5d | P1-25, P1-15 |
| [x] | P1-30 | Thu MIDI + quantize khi thu + overdub MIDI | W11 | 1.5d | P1-29 |
| [ ] | P1-31 | Đường NOTE_ON/OFF + map phím trong harness | W11 | 0.5d | P1-25 |
| [ ] | P1-32 | `project.open` + kịch bản khôi phục project | W11 | 1d | P1-27, P1-11 |
| [ ] | P1-33 | Build iOS cho full engine, thay engine trong shell spike | W12 | 1.5d | tất cả ở trên |
| [ ] | P1-34 | Kịch bản tải chuẩn trên iPad 8 + đo | W12 | 1d | P1-33 |
| [~] | P1-35 | Prototype hiệu chỉnh latency trên máy | W12 | 1d | P1-33, P0-07 |
| [ ] | P1-36 | Soak 30 phút (Mac + iPad) + sửa lỗi | W13 | 1.5d | P1-34 |
| [~] | P1-37 | Quét TSan/ASan, xoá code spike | W13 | 1d | – |
| [x] | P1-39 | **Pedal mode `FromFirstLoop`** (04 §2.5) + `transport.setTempoMode` + sửa clip MIDI đang phát (04 §7), người dùng yêu cầu 29/09 | W13 | 2d | P1-19, P1-29 |
| [ ] | P1-38 | **Review M1** + cập nhật docs + retro | W14 | 2d + buffer | tất cả |

---

## W3 — Khung

### P1-01 · Khung engine · 1.5d
- `le::core::Engine` (facade mà C API gọi vào), `le::io::DeviceIO` (**interface**: `start(callback)`, `stop()`, `sampleRate()`, `bufferSize()`, `latencies()`, `hostTimeNs`). Hiện thực bằng `JuceDeviceIO` (Plan B chỉ cần thêm `CoreAudioIO`).
- `RtEngine` không phụ thuộc thiết bị: `prepare(sr, maxBlock)`, `process(in, out, n, ctx)` theo 04 §1 (bản rỗng).
- **DoD:** harness phát im lặng qua `JuceDeviceIO`. `RtEngine` được tạo trong test **mà không cần thiết bị**.

### P1-02 · Harness CLI · 1.5d
- `le-harness render <scenario.json> -o out.wav [--block N]`
- `le-harness play [scenario.json]`: chạy real-time, đọc bàn phím terminal (raw mode):
  - `1–8`: launch ô `(t, scene đang chọn)`
  - `Q–I`: chọn scene
  - `Space`: play/stop
  - `[` `]`: chọn track · `Shift+R`: record ở track đang chọn (vì `R` trùng với hàng Q–I dùng để chọn scene)
  - `O`: overdub
  - `Z–M`: nốt (P1-31)
  - Mỗi giây in dòng trạng thái (beat, CPU, xrun, trạng thái clip)
- **DoD:** `--help` rõ ràng. `play` dừng sạch bằng Ctrl-C.

### P1-03 · Render offline + scenario runner + golden · 2d
- Scenario JSON theo 08 §3.2: `setup` (call/send), `timeline` (`atBeat`), `renderBeats`, `expect` (`golden`, `nullTestMaxDb`, `firstNonSilentSample`, `maxSampleJumpDb`).
- Test Catch2 tự quét `tests/scenarios/*.json` và chạy mỗi file với block 64/128/256/1024.
- `scripts/golden_update.sh`
- **DoD:** 1 scenario (`spike_sine_ramp`) xanh ở cả 4 kích thước block. Golden sai thì báo **sample đầu tiên bị lệch**. Scenario metronome chuyển sang DoD của P1-10, vì phải có Transport.

## W4 — Các kênh giữa các thread

### P1-04 · RtQueues [RT] · 1d
- `rtCommands` (UI → RT, 1024 phần tử), `rtToNrt` (RT → NRT: Retire/Event, 1024), `midiToRt` (512). Chỉ dùng `try_push`/`front`/`pop`, không bao giờ block.
- `le_send`: validate ở phía main (track/slot trong dải cho phép), chạy side effect NRT nếu có, rồi mới push. Queue đầy thì trả `false` và tăng bộ đếm.
- **DoD:** stress test 2 thread dưới TSan. RTSan sạch.

### P1-05 · CommandProcessor + JSON + JobSystem · 1.5d
- Bảng dispatch `op → handler`, envelope `ok/result/error` (05 §3), mã lỗi (05 §2).
- `JobSystem` (04 §15), `job.result`, `job.cancel`.
- **DoD:** test hợp đồng: mọi op chưa làm trả `NOT_IMPLEMENTED`, JSON hỏng trả `INVALID_ARG`, job giả hoàn thành và phát đúng thứ tự event.

### P1-06 · Snapshot + ReleasePool [RT] · 1.5d
- `GraphSnapshot` bất biến. `SnapshotBuilder` dựng từ `EngineModel`. `pending.exchange` theo 03 §4.2. `RtState::remap()`.
- `ReleasePool` chạy trên Timer 30Hz của main thread.
- **DoD:** test: 10⁴ lần swap liên tiếp trong lúc render, không leak (ASan), không huỷ object trên RT (RTSan).

### P1-07 · StatePublisher đầy đủ + events · 1d
- Điền đủ `LeState` (05 §2). Có event dispatcher (Timer) gọi `LeEventCallback`.
- **DoD:** test `publishCounter` tăng đơn điệu, không đọc phải dữ liệu rách (TSan).

## W5 — Thời gian

### P1-08 · Transport [RT] · 1.5d
- 04 §2.1: `samplePos`, neo `(anchorSample, anchorBeat)`, `SET_BPM`, play/stop, `beatsPerBar`.
- **DoD:** unit test 08 §3.1 (không trôi sau 10⁸ sample, đổi BPM giữ đúng vị trí beat).

### P1-09 · BlockSplitter [RT] · 1.5d
- 04 §2.2: tối đa 16 segment, gộp các sự kiện cách nhau ≤ 1 sample.
- **DoD:** unit test ranh giới ở frame 0, frame cuối, nhiều ranh giới trong một block, block 1 frame.

### P1-10 · Metronome + count-in [RT] · 1d
- 04 §2.3. Có các chế độ tắt / bật / chỉ khi thu.
- **DoD:** scenario `metronome_4bars` xanh. Click rơi đúng sample `k * samplesPerBeat`.

## W6 — Phát audio

### P1-11 · AudioFileIO + `clip.setAudio` · 1d
- Decode WAV/AIFF/CAF/FLAC thành `AudioData` float trên worker. Có kiểm tra định dạng và báo lỗi đúng mã.
- **DoD:** file hỏng hoặc thiếu trả `FILE_FORMAT` / `FILE_NOT_FOUND`. Job phát đúng event.

### P1-12 · AudioClipPlayer [RT] · 2d
- 04 §4: vị trí tính từ beat, Re-Pitch, Hermite, fade vào 2ms / ra 5ms, crossfade 2ms ở điểm loop.
- **DoD:** scenario `clip_loop_120` (null test), `clip_repitch_100_to_120` (độ dài vòng đúng), `maxSampleJumpDb` không có bước nhảy lớn tại điểm loop.

### P1-13 · Track + Mixer [RT] · 1d
- 04 §11: gain, pan (luật -3 dB), mute, solo (ramp 5ms), SmoothedValue.
- **DoD:** unit test luật pan. Scenario solo 1 track.

### P1-14 · Limiter + meter [RT] · 0.5d
- **DoD:** tín hiệu +6 dBFS vào thì ra ≤ -0.3 dBFS. Meter peak trong `LeState` đúng.

## W7 — Grid

### P1-15 · ClipScheduler [RT] · 2d
- 04 §3.1–3.2: mọi trạng thái, `targetBeat`, launch khi transport đang dừng, hai clip cùng track.
- **DoD:** unit test cho **mọi** mũi tên trên sơ đồ trạng thái. Scenario `launch_quantized_1bar` với `firstNonSilentSample = 96000`.

### P1-16 · Scene, stop all, transport stop [RT] · 1d
- 04 §3.3–3.4.
- **DoD:** scenario scene có ô trống (track đó dừng), stop all theo quantize, transport stop có fade.

### P1-17 · LaunchLog · 0.5d
- 04 §3.5
- **DoD:** 100 lần launch → log đúng thứ tự, đúng beat.

## W8 — Thu âm

### P1-18 · Arm + RecordBuffer · 0.5d
- `TRACK_ARM` ở phía main cấp phát trước, rồi mới push lệnh (04 §5.1).
- **DoD:** RTSan sạch khi arm hoặc disarm trong lúc đang phát.

### P1-19 · Recorder capture [RT] · 2d
- Độ dài cố định N bar hoặc tự do (làm tròn lên), count-in, `CLIP_RECORD`, `RECORD_STOP`, chuyển sang Playing.
- Trong scenario, input được **giả lập** bằng file (`"input": "fixtures/…wav"`).
- **DoD:** scenario thu 2 bar → clip đúng `2 * beatsPerBar` beat. Take tự do dừng ở beat 5.3 → 8 beat (quantize 1 bar).

### P1-20 · Bù latency [RT] · 1d
- 04 §5.3. Scenario giả lập độ trễ input L sample.
- **DoD:** take sau khi bù bắt đầu **đúng sample** tương ứng (±0) với L = 0, 256, 1000.

### P1-21 · Ghi đĩa + event · 1d
- `ThreadedWriter`, file `audio/<clipId>.caf`, event `RECORDING_FINISHED` **sau khi** đóng file, `clip.info`.
- **DoD:** đọc lại file giống buffer từng bit.

## W9 — Hoàn thiện thu âm

### P1-22 · Overdub + undo [RT] · 2d
- 04 §5.4. `OVERDUB_TOGGLE`, `clip.undoOverdub`.
- **DoD:** scenario overdub 1 vòng → golden. Undo → giống bản trước overdub từng bit.

### P1-23 · Input monitoring [RT] · 1d
- Off / Auto / On (04 §5.5). Trạng thái tai nghe lấy từ route (main thread cập nhật cờ atomic).
- **DoD:** Auto khi không có tai nghe → không nghe thấy input.

### P1-24 · PeakBuilder + `le_get_peaks` · 1d
- 3 mức, cache RAM và đĩa (04 §5.7).
- **DoD:** min/max đúng với file sine. `maxPairs` nhỏ hơn dữ liệu → trả về đúng số cặp.

## W10 — Sampler

### P1-25 · Voice pool & zone [RT] · 2d
- 04 §6.1–6.2: 64 + 8 voice, chọn zone, `inc`, Hermite, stealing có fade.
- **DoD:** unit test stealing: 100 note-on → không bao giờ vượt 64 voice active, không click (`maxSampleJumpDb`).

### P1-26 · ADSR + choke + one-shot [RT] · 1d
- **DoD:** unit test từng pha ADSR. Hi-hat mở bị hi-hat đóng chặn trong 5ms.

### P1-27 · SfzLoader + `track.setInstrument` · 1.5d
- Tập con opcode ở 06 §4. Opcode lạ → cảnh báo. Đường dẫn tương đối theo `default_path`.
- **DoD:** parse đúng 3 file SFZ mẫu. File thiếu sample → lỗi rõ ràng, không crash.

### P1-28 · Fixture & golden · 0.5d
- 1 drum kit nhỏ (4 sample) và 1 nhạc cụ (3 zone), dùng CC0 hoặc tự tổng hợp, lưu trong `tests/fixtures/`.
- **DoD:** scenario beat 1 bar → golden (sau khi **bạn nghe duyệt**).

## W11 — MIDI & khôi phục project

### P1-29 · MidiClipPlayer [RT] · 1.5d
- 04 §7: nốt chính xác từng sample, bảng note-off, cắt ở điểm loop, all-notes-off khi dừng.
- **DoD:** scenario nốt ở beat 0.5 → voice bắt đầu đúng sample 12000 (@120 BPM, 48 kHz). Không có nốt treo sau khi dừng.

### P1-30 · Thu MIDI · 1.5d
- Ring buffer `NoteEvent`, ghép on/off ở main, quantize khi thu (tắt / 1/16 / 1/8), overdub MIDI.
- **DoD:** scenario thu 1 bar nốt lệch 10ms → quantize 1/16 ra đúng lưới.

### P1-31 · NOTE_ON/OFF + phím harness · 0.5d
- **DoD:** chơi được sampler bằng bàn phím trong `le-harness play`.

### P1-32 · `project.open` + kịch bản khôi phục · 1d
- Chuỗi lệnh ở 06 §6, viết thành scenario `restore_project.json` (4 track: drum MIDI, audio, instrument, trống).
- **DoD:** render sau khi khôi phục giống golden. Mọi job xong trước khi Play.

## W12–W14 — Lên máy & M1

### P1-33 · Full engine trên iOS · 1.5d
- Build XCFramework đầy đủ, thay engine spike trong app spike. Thêm nút "Nạp kịch bản tải chuẩn" (đọc JSON đóng gói, phát qua `le_call`/`le_send`).
- **DoD:** chạy được trên iPad 8. Tính năng spike vẫn còn (latency, stretch) để so sánh.

### P1-34 · Đo tải chuẩn trên iPad 8 · 1d
- Kịch bản tải chuẩn ở 08 §1, bản P1 (chưa có FX và stretch: 8 track + 64 voice + 1 track thu).
- **DoD:** CPU trung bình và đỉnh ghi vào 08 §8. Đỉnh < 40% (còn chừa chỗ cho FX ở P3).

### P1-35 · Prototype hiệu chỉnh latency · 1d
- Dùng lại LatencyProbe → `latency.calibrate` → `calibrationOffset`, rồi dùng trong P1-20.
- **DoD:** bạn thu một take gõ theo metronome → nghe lại khớp.

### P1-36 · Soak 30 phút · 1.5d
- Harness Mac chạy `mac-rtsan` 30 phút với kịch bản ngẫu nhiên (launch, record, note liên tục). iPad chạy 30 phút tải chuẩn.
- **DoD:** 0 xrun, 0 vi phạm RTSan, RAM phẳng.

### P1-37 · Quét sanitizer + dọn dẹp · 1d
- **Kèm hardening từ rà soát RT (`engine/tools/docs/rt-review-2026-09-29.md`):** R5 (luôn prepare probe), R7 (buffer thu theo SR lớn nhất), R8 (`liveFade_` tính từ `kSwapFadeSec`), R9 (dọn `jobs_`), R10 (chỉ một producer MIDI).
- **DoD:** TSan và ASan sạch. Xoá `src/spike/` (giữ `LatencyProbe` vì chuyển sang `io/`), xoá `LE_CMD_SPIKE_*` khỏi header (**tăng** `LE_API_VERSION` lên 2, cập nhật 05).

### P1-38 · Review M1 · 2d + buffer
- Checklist M1 (02 §3), cập nhật 01/04/05/08 theo code thật, retro, lên kế hoạch P2.

---

## Có thể chạy song song (khi dùng nhiều agent)

| Luồng A (core/time) | Luồng B (DSP) | Luồng C (IO/tools) |
|---|---|---|
| P1-04 → 07 → 08 → 09 → 15 → 16 | P1-12 → 13 → 14 (sau P1-09) · P1-25 → 26 | P1-02 → 03 · P1-11 · P1-27 → 28 · P1-24 |

Mỗi luồng chỉ sở hữu thư mục của mình (`core/`, `dsp/`, `io/`+`render/`+`harness/`). Interface giữa các luồng được chốt trong header nội bộ **trước** khi làm.
