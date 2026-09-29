# le-fuzz-api và le-soak — cách chạy

Hai công cụ tìm lỗi mà unit test và scenario khó chạm tới. Agent 80 làm, 29/09/2026.

| Tool | Để làm gì | Đi vào engine qua | Build nên dùng |
|---|---|---|---|
| `le-fuzz-api` | Chuỗi lệnh ngẫu nhiên, có cả lệnh hỏng: engine không được crash và luôn trả lời đúng hợp đồng | **C API** (`le_call`, `le_send`, `le_get_peaks`, `le_audio_*`, `le_read_state`), giống app Dart | `mac-asan` (ASan + UBSan); `mac-tsan` với `--mode device` |
| `le-soak` | Một phiên chơi nhạc dài như người dùng thật: không NaN, không rò bộ nhớ, take nào thu xong cũng có file | `Engine::send` / `Engine::call` (tương đương `le_send` / `le_call`) + `OfflineDeviceIO`. Tool tự render từng block | `mac-rtsan` (bắt vi phạm RT); `mac-release` khi chỉ đo bộ nhớ |

Cả hai build trong thư mục riêng, đúng luật 11 §6. Ví dụ:

```bash
cmake --preset mac-asan  -B <scratch>/cmake-mac-asan  && ninja -C <scratch>/cmake-mac-asan  le-fuzz-api
cmake --preset mac-tsan  -B <scratch>/cmake-mac-tsan  && ninja -C <scratch>/cmake-mac-tsan  le-fuzz-api
cmake --preset mac-rtsan -B <scratch>/cmake-mac-rtsan && ninja -C <scratch>/cmake-mac-rtsan le-soak
```

Mã thoát chung: `0` là sạch, `1` là có vi phạm (có in chi tiết kèm cách tái hiện), `2` là sai tham số. Khi sanitizer hoặc signal làm tiến trình chết, tool vẫn in dòng `💥 … tái hiện: <lệnh>` trước khi thoát.

---

## le-fuzz-api

```bash
<scratch>/cmake-mac-asan/tools/le-fuzz-api --selftest                       # bộ bắt JUCE assertion còn chạy?
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ASAN_OPTIONS=abort_on_error=1 \
  <scratch>/cmake-mac-asan/tools/le-fuzz-api --seeds 1-100 --ops 300          # sim, deterministic
TSAN_OPTIONS=halt_on_error=0 \
  <scratch>/cmake-mac-tsan/tools/le-fuzz-api --mode device --seeds 1-5 --ops 200   # audio thread thật
<build>/tools/le-fuzz-api --seed 17 --ops 124 --verbose                        # tái hiện 1 seed, in từng bước
```

**Hai chế độ chạy:**
- `--mode sim` (mặc định): gọi `sim.offline` với sample rate và block size ngẫu nhiên, rồi `sim.advance` để render. Mọi thứ chạy trên một thread.
- `--mode device`: CoreAudio thật (chỉ mở output, không xin quyền mic), pump bằng Timer 30 Hz của JUCE qua `CFRunLoop`. Đây là chế độ để TSan thấy được audio thread thật. Máy không có loa thì seed đó bị bỏ qua.

**Mỗi bước là một trong các việc sau:**
- `le_call` hợp lệ cho mọi op của 05 §3 (thỉnh thoảng một tham số bị thay bằng giá trị biên hoặc sai kiểu, bị xoá, hoặc có thêm field rác);
- JSON hỏng: cắt cụt, chèn ký tự, lồng sâu 10–400 cấp, UTF-8 sai, op lạ;
- `le_send` với LeCommand ngẫu nhiên (75 % nằm trong dải, còn lại có NaN, Inf, type lạ);
- render / advance; `le_get_peaks` (buffer có mẫu canh để phát hiện ghi tràn);
- `job.result` / `job.cancel`; `le_audio_stop` / `start`;
- hiếm hơn: `le_destroy` rồi `le_create` lại giữa chừng (như app bị huỷ).

**Bất biến (vi phạm thì mã thoát là 1):**
1. Không crash: ASan, UBSan và TSan báo lỗi thì death callback in dòng tái hiện.
2. `le_call` luôn trả JSON đúng envelope: `{"ok":true,"result":{…}}`, hoặc `{"ok":false,"error":{"code":UPPER_SNAKE,"message":…}}`.
3. LeState hợp lý sau mỗi bước: `clipState ≤ 7`, `trackPlayingSlot ∈ [−1, 7]`, mọi số thực hữu hạn, peak ≥ 0, master ≤ 0 dBFS, `trackClipProgress ∈ [0, 1]`. Sau block đầu tiên còn phải có `bpm ∈ [20, 300]`, quantize và beatsPerBar trong dải.
4. Event có type hợp lệ và value hữu hạn; event job có jobId > 0.
5. Mọi jobId đã trả cho client phải tra được bằng `job.result` và **kết thúc** (`done` hoặc `failed`) trong 60 s khi engine tiếp tục chạy.
6. `le_get_peaks` không ghi quá `maxPairs` và không trả peak NaN.
7. **Bản debug (kể cả asan / tsan / rtsan): không có "JUCE Assertion failure" nào.**
   - JUCE in assertion qua `DBG` → `fputs(stderr)`, không qua `juce::Logger`. Vì vậy tool chặn fd 2 bằng `StderrWatch` trong `api_client.h`: fd 2 được nối vào một pipe, một thread đọc chuyển nguyên văn ra stderr thật và quét từng dòng.
   - Sau mỗi bước, main ghi một dòng đánh dấu rồi chờ thread quét tới nó. Nhờ vậy assertion do main gây ra được gán **đúng bước / seed**; assertion từ worker được gán cho lần kiểm kế tiếp.
   - Khi tiến trình chết, pipe được đổ ra ngay, nên báo cáo sanitizer không bị mất.
   - `le-fuzz-api --selftest` tự gây một assertion; nếu không bắt được thì mã thoát là 1. Nên chạy lệnh này trước mỗi đợt fuzz.
   - Tắt bằng `LE_NO_STDERR_WATCH=1`.

**Chuỗi UTF-8** (05 §1: mọi chuỗi qua FFI là UTF-8):
- **Hợp lệ** (dựng bằng `juce::String::fromUTF8`): tiếng Việt NFC và NFD, emoji 4 byte, chuỗi ZWJ, CJK / RTL, BOM, U+10FFFF, ký tự điều khiển. Chúng đi vào `name`, `clipId` (thành tên file `audio/<clipId>.caf`) và thư mục project (`Dự án 🎵`).
- **Không hợp lệ** (byte thô chèn thẳng vào JSON): byte lẻ, chuỗi cụt, overlong, surrogate mã hoá UTF-8, > U+10FFFF, FE/FF, escape `\ud800` lẻ, `\u0000`.
- Cả JSON hợp lệ có key rỗng `{"": …}`.
- ⚠️ **Không bao giờ** dựng `juce::String` / `juce::var` từ `const char*` chứa byte > 127: đó là constructor **ASCII** (juce_String.cpp:327) và làm hỏng chuỗi trước khi tới engine. Đây là lỗi cũ của `randomString`, eb bắt được, đã sửa.

**Tổng kết in ở cuối (thông tin, không tính là vi phạm):**
- Số lần gặp `error.code == "INTERNAL"` (mã lỗi mơ hồ), kèm vài ví dụ.
- Các `le_call` chậm nhất trên main (05 §1 yêu cầu < 1 ms, trừ `sim.*`).

**Tái hiện:** chuỗi *lệnh* chỉ phụ thuộc seed, không phụ thuộc response, nên `--seed N --ops K` phát lại đúng các lệnh tới bước K. Chỉ có thời điểm job chạy trên worker xong là có thể lệch vài bước giữa các lần chạy. Nếu một lỗi chỉ thỉnh thoảng mới hiện ra, hãy chạy lại vài lần hoặc dùng `--verbose` để xem lệnh gây lỗi.

---

## le-soak

```bash
<scratch>/cmake-mac-rtsan/tools/le-soak --seed 1                  # 30 phút nhạc (mặc định), block 128
<build>/tools/le-soak --seed 3 --minutes 60 --block 256 --warmup 5 --max-leak-mb 8
<build>/tools/le-soak --seed 1 --minutes 5 --hostile              # thêm bật/tắt overdub dồn dập (đường race R1)
```

**Dựng phiên:**
- Mở project trong thư mục tạm.
- Track 0–3 là audio: slot 0–1 có clip từ `tests/fixtures`, gồm cả clip 100 BPM (Re-Pitch).
- Track 4–7 là nhạc cụ `inst_synth` / `kit_synth` với clip MIDI.
- Arm rồi disarm mọi track một lần, để buffer thu 66 s được cấp phát ngay từ đầu. Nhờ vậy lượng bộ nhớ tăng sau đó chỉ đến từ rò rỉ thật.

**Khi chạy:**
- Tool render từng block, input là sine 220 Hz −12 dB cộng một chút nhiễu.
- Pump khoảng 30 Hz, tức mỗi 12 block ở 128 frame. Đây là nhịp của app thật. Scenario / `sim.advance` pump sau **mỗi** block nên che mất các race giữa main và RT.
- Cứ 50 ms – 1,5 s làm một hành động ngẫu nhiên: launch, stop, scene, stop all, thu audio (có độ dài hoặc tự do, kèm RECORD_STOP), thu MIDI, overdub, nốt nhạc cụ, BPM, quantize, metronome, mixer, `fx.set` / `fx.remove` / FX_PARAM, `clip.clear`, `clip.undoOverdub`, `clip.setMidi`, transport stop rồi play.

**Kiểm (vi phạm thì mã thoát là 1):**
1. Mọi mẫu output hữu hạn và |x| ≤ 1 (sau limiter). **Phải kiểm trên output**, vì meter peak không nhìn thấy NaN: `std::max(running, NaN)` giữ nguyên `running`.
2. LeState hợp lý, envelope `le_call` đúng, job kết thúc.
3. Mỗi `RECORDING_FINISHED` của track audio: `clip.info` trỏ tới `audio/<clipId>.caf` và file đó có trên đĩa ngay lúc event tới. Theo 04 §5.6, event chỉ được phát sau khi file đã đóng.
4. **Bộ nhớ phẳng**, đo bằng *heap đang cấp* (`malloc_zone_statistics`: số byte còn được cấp phát thật, không tính trang allocator giữ lại để dùng lại).
   - **Mức nền:** setup, rồi `project.close`, rồi đo; sau đó setup lại để chơi.
   - **Cuối phiên:** `project.close`, render và pump thêm để snapshot cũ được thu hồi, rồi đo lại.
   - **Hai lần đo cùng ở trạng thái "project đã đóng"**, nên phần chênh chính là thứ phiên chơi để lại mà không ai giải phóng. Nó phải ≤ `--max-leak-mb` (mặc định 8 MB: sau khi JobSystem thôi giữ capture của job, phiên 30 phút còn để lại khoảng 3 MB vì `jobs_` không bao giờ xoá — R9).
   - Trong lúc chơi, footprint tăng là hợp lệ, vì take, bản copy overdub và lớp undo lấp dần các ô. Tool vẫn in `phys_footprint` / RSS mỗi 5 phút để tham khảo.
   - Muốn biết ai giữ vùng nhớ: `MallocStackLogging=1 le-soak … --hold 120`, rồi trong lúc tool đang giữ, chạy `malloc_history <pid> -allBySize` (xem các khối lớn nhất và stack đã cấp phát chúng).
5. Build `mac-rtsan`: vi phạm RT trên audio thread làm RTSan dừng tiến trình, và tool in seed cùng thời điểm (giây nhạc).

---

## Gợi ý đưa vào CI / quy trình

- **Mỗi PR đụng `src/`:** `le-fuzz-api --seeds 1-50 --ops 300` trên `mac-asan` (khoảng vài phút).
- **Hằng đêm:**
  - `le-fuzz-api --seeds <ngày>*1000 … +200` (seed mới mỗi đêm);
  - `le-soak --minutes 30` trên `mac-rtsan` với 2 seed;
  - `le-fuzz-api --mode device --seeds 1-5` trên `mac-tsan` (cần máy có loa).
- Lỗi tìm ra được gửi cho chủ code (68: `core/`, `io/`, `api/`; 80: `dsp/`, `render/`, `midi/`, `tools/`), kèm dòng tái hiện.

## Giới hạn đã biết

- Fuzz dùng fixture thật trong `tests/fixtures`, không tự sinh file âm thanh hỏng. Việc fuzz decoder và SFZ parser (file hỏng) để cho P4-16.
- `--mode device` không mở mic, nên đường input thật (monitoring, thu âm) chỉ được TSan kiểm qua `le-soak` offline hoặc test của 68.
- Phần lồng JSON cực sâu (hàng chục nghìn cấp) chưa được thử vì app không bao giờ gửi. Parser JSON của JUCE là đệ quy.

## Kết quả đợt chạy đầu (29/09/2026, agent 80)

| Chạy | Kết quả |
|---|---|
| `le-fuzz-api` mac-release, seed 101–400 × 300 bước | 52 376 `le_call` · 28 627 `le_send` · 923 lần restart — **0 vi phạm** |
| `le-fuzz-api` mac-asan (ASan + UBSan), seed 1–100 × 300 | 0 vi phạm, 0 lỗi ASan. UBSan báo 1 chỗ: **L2** (`job.result` / `job.cancel` ép `jobId` ngoài dải sang int64 — EngineOps.cpp:91/97, việc của 68) |
| `le-fuzz-api --mode device` mac-tsan, seed 1–5 × 200 | 0 vi phạm, 0 cảnh báo TSan |
| `le-soak` mac-rtsan, seed 1, 30 phút | Không vi phạm RT. 79 / 79 take có file. **Vi phạm bộ nhớ +52 MB → L1** |
| `le-soak` mac-release, seed 2, 30 phút | 68 / 68 take có file. **+51 MB → L1** |

- **L1 (S2, việc của 68):** `JobSystem` giữ `Job::fn` của mọi job đã chạy xong, và `jobs_` không bao giờ xoá. Lambda `writeTake` capture `shared_ptr<AudioData>` của take và của clip sau overdub, nên mỗi take để lại toàn bộ audio của nó.
  - Xác nhận bằng `malloc_history`: 14 take + 11 target overdub = 25 job persist.
  - Thử bản vá `job_->fn = nullptr` sau khi chạy (link JobSystem.o đã vá, chỉ trong scratchpad): phiên 30 phút chỉ còn để lại +3,0 và +3,6 MB (là phần `jobs_` không được xoá).
- **Lỗi của chính tool, đã sửa trong đợt này:**
  - RNG từng seed bằng `seed·γ`: luồng của seed N+1 trùng luồng của seed N lệch 1 lần rút, nên các seed gần như giống nhau. Nay trộn seed qua hàm băm trước.
  - Fuzz ở chế độ sim từng báo nhầm "job treo" khi audio đã stop: không có gì pump. Nay tool chạy run loop như app.
  - Cách đo bộ nhớ ban đầu (footprint sau setup so với cuối phiên) báo nhầm, vì take lấp dần các ô. Nay so heap đang cấp ở hai trạng thái "project đã đóng".
