# Checklist thử tay UI trên iPad 8

> Dành cho người thử trên **iPad 8** (1080×810 pt, 60 Hz), trước các mốc M1/M2/M3.
> Gom từ các "bước người dùng" của từng đợt P2–P4, sắp theo màn hình. Tick `[x]` khi đạt. Hỏng thì ghi lại
> bước, kết quả thấy được, và ảnh chụp/ghi màn hình nếu có.
>
> Cột **Engine**: `Fake` = thử được ngay với engine giả. `Thật` = cần engine thật có op tương ứng.
> Chạy với Fake mà thấy banner/SnackBar "NOT_IMPLEMENTED" hoặc "Engine chưa hỗ trợ…" là **đúng dự kiến**.

---

## 0. Chuẩn bị

| Việc | Cách làm |
|---|---|
| Ký app | Mở `app/ios/Runner.xcworkspace` → Runner → Signing & Capabilities → chọn Team. Bundle `com.loopcore.musicLooper` |
| Bản đo hiệu năng | `cd app && flutter run --profile …` (đo 60 fps bằng Performance overlay: bấm phím **P** trong terminal) |
| Bản xem repaint | `flutter run` (debug) → DevTools → Flutter Inspector → bật **Highlight Repaints** |
| Vào thẳng Projects | Thêm `--dart-define=LOOPCORE_START=projects` (không có cờ này thì app mở màn spike, dùng tới hết M0) |
| Dùng engine giả | Thêm `--dart-define=LOOPCORE_FAKE=true` (không cần XCFramework, không có tiếng) |
| Thử lại onboarding | Xoá app khỏi iPad rồi cài lại (cờ "đã xem" nằm trong `settings.json`) |

**Luật chung khi kiểm repaint** (07 §6): để yên 5 giây thì **không viền nào đổi màu**, trừ chỗ pixel thật sự thay đổi
(meter, playhead, ô đang phát/queued, đồng hồ). **Viền bọc cả màn hình không bao giờ được đổi màu.**

---

## 1. Màn spike (mặc định tới hết M0)

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Mở app (không dart-define) | Header hiện `apiVersion = 1` và tên device, **không** có chữ FakeEngine | Thật |
| [ ] | Bấm Start audio lần đầu | Hộp thoại xin micro. Từ chối → hiện hướng dẫn "Mở Cài đặt", audio vẫn chạy ở chế độ chỉ phát | Thật |
| [ ] | Bật sine, để yên | Performance overlay 60 fps. Highlight Repaints: chỉ bảng live bên phải đổi màu | Thật |
| [ ] | Kéo slider | Chỉ cột control đổi màu | Thật |
| [ ] | Thu 4 giây → Phát loop | Nghe lại ra loa ngoài | Thật |
| [ ] | Cắm tai nghe → Passthrough | Nghe mic qua tai nghe | Thật |
| [ ] | So sánh session mode default / measurement; bấm Session info | `allowBluetoothHFP = false` | Thật |
| [ ] | Đo latency 3 lần (loa ngoài, phòng yên tĩnh) | Chép 3 dòng tóm tắt vào 08 §8 | Thật |
| [ ] | Thu 4 giây → Stretch bench (formant bật/tắt) | Ghi tổng ms; nghe file WAV trong app Files → Music Looper → spike | Thật |
| [ ] | P0-10: cắm/rút tai nghe, AirPods, gọi Siri, về Home | Dùng log event góc phải dưới để điền bảng route/ngắt; về Home vẫn phát | Thật |
| [ ] | Nút "Projects (dev)" | Lần đầu vào onboarding, các lần sau vào Projects | Fake |

---

## 2. Onboarding (lần đầu vào Projects)

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Cài mới, chạy với `LOOPCORE_START=projects` | Màn "Music Looper cần micro" với 3 lý do | Fake |
| [ ] | "Cho phép micro" → Cho phép | Sang bước "Thử ngay với project demo" | Fake |
| [ ] | "Mở project demo" | Vào thẳng Session của Demo; bấm ◀ thì về Projects (không về onboarding) | Fake |
| [ ] | Cài lại, lần này **Không cho phép** | Báo "Micro đang bị tắt…", có nút "Mở Cài đặt" và "Tiếp tục"; demo vẫn có tiếng (chỉ phát) | Thật |
| [ ] | "Bỏ qua" (góc trên phải) | Sang Projects; mở lại app không hiện onboarding nữa | Fake |
| [ ] | Cài lại, bước micro chọn **"Để sau"** → mở Demo | KHÔNG hiện hộp xin quyền của iOS; demo có tiếng (chỉ phát), banner "Chưa có quyền micro…" + nút "Cho phép micro" | Thật |
| [ ] | Sau đó arm track audio (hoặc nút LOOP trên track audio / Thu âm mới / Đo độ trễ) | Lúc này mới hiện hộp xin quyền; Cho phép → thao tác làm tiếp luôn (track được arm) | Thật |

---

## 3. Projects

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | "Tạo project demo" | Demo xuất hiện trong danh sách | Fake |
| [ ] | "Project mới" → đặt tên | Mở Session với 8 track trống | Fake |
| [ ] | Menu ⋯ của một project: Đổi tên / Nhân bản / Xoá | Danh sách cập nhật đúng; xoá có hỏi lại | Fake |
| [ ] | Nút ⚙ | Mở màn Cài đặt (MIDI learn và Link báo "Mở một project để …") | Fake |
| [ ] | Tắt hẳn app ngay lúc đang lưu (vuốt khỏi app switcher) rồi mở lại | Project vẫn mở được (dùng `.bak`), không mất | Fake |

---

## 4. Session — grid, launch, thu

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Chạm ô có clip (Perform) | Phát **ngay khi ngón chạm xuống**, không trễ cảm nhận được | Fake/Thật |
| [ ] | Chạm ô khác cùng track khi đang chạy | Ô nhấp nháy theo phách rồi chuyển sang phát đúng đầu bar | Fake |
| [ ] | Chạm số scene ▶ ở cột phải; chạm ■ Stop all | Cả hàng chuyển cùng lúc; Stop all dừng hết ở ranh giới | Fake |
| [ ] | Nhấn giữ ô ở Perform | Chỉ launch, không mở menu | Fake |
| [ ] | Arm track 5 (●) | Ô trống của track hiện vòng ○ | Fake |
| [ ] | Chạm ô trống trên track đang arm | Viền đỏ → nền đỏ + thanh tiến độ → đủ số bar thì phát; ô có tên "Bản thu …" | Fake |
| [ ] | Thoát project rồi mở lại | Clip vừa thu vẫn còn (autosave) | Fake |
| [ ] | Thu gọn panel dưới (▾) | Grid giãn ra, không giật | Fake |
| [ ] | Nhấn giữ tên track → Đổi tên track | Tên mới hiện ở header, mixer; mở lại project vẫn giữ | Fake/Thật |
| [ ] | Nhấn giữ tên track trống → Chuyển thành track nhạc cụ / audio | Đổi loại được; track còn clip thì mục này mờ "(track phải trống)" | Fake/Thật |
| [ ] | Tai nghe Bluetooth | Banner cảnh báo latency Bluetooth; rút ra thì banner mất | Thật |
| [ ] | Bấm ▶ rồi để yên quá thời gian khoá màn hình | Màn hình không tắt; bấm ■ thì khoá lại bình thường | Fake |
| [ ] | Engine thật: mở Demo → launch scene 1 | Nghe trống + bass + hợp âm | Thật |
| [ ] | Cài đặt → Độ dài thu **Tự do** → chạm ô trống trên track đang arm → đánh → chạm lại ô đó | Thu tới lúc chạm lần hai; độ dài làm tròn lên theo quantize (tối thiểu 1 bar) rồi phát | Fake/Thật |

## 4b. Nút ● LOOP và pedal mode (P2-28)

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Projects → Project mới | Top bar hiện "— BPM / chờ vòng đầu", metronome "Tắt", không có nút TAP | Fake/Thật |
| [ ] | Chạm tên Track 1 → chạm nút LOOP (ô trên cùng bên phải) | Track tự arm (lần đầu hỏi quyền micro), nút đỏ "REC", thu ngay, transport chưa chạy | Thật |
| [ ] | Đánh/hát khoảng 2–5 giây → chạm LOOP | Vòng chốt đúng lúc chạm, phát lại liền mạch; top bar hiện BPM suy ra (80–160); nút xanh "PHÁT" + vòng tiến độ | Thật |
| [ ] | Chạm LOOP lần nữa → đánh thêm → chạm LOOP | Cam "CHỒNG" (overdub) → xanh lại; lớp mới nghe chồng lên | Thật |
| [ ] | Chọn Track 2 → LOOP → đánh ~1,5 vòng → LOOP | Bắt đầu ở đầu bar; độ dài làm tròn lên bội số của vòng đầu | Thật |
| [ ] | Nút ■ (dưới LOOP) | Track đang chọn dừng ở ranh giới | Fake/Thật |
| [ ] | Nút ↶ (dưới LOOP) ngay sau khi vừa overdub | Bỏ lớp vừa chồng; không có lớp overdub → hỏi "Xoá clip …?"; track trống thì nút mờ | Fake/Thật |
| [ ] | Chạm LOOP hai lần thật nhanh | Mỗi lần chạm đều là một bước của LOOP (không còn chạm đúp = dừng) | Fake |
| [ ] | ■ dừng, xoá hết clip | Lại hiện "— BPM / chờ vòng đầu" | Fake/Thật |
| [ ] | Menu ♩ → "BPM cố định" / "Vòng đầu quyết định BPM" | Đổi chế độ; mở lại project vẫn giữ | Fake/Thật |
| [ ] | Edit → nhấn giữ ô → Màu track (có Launchpad cắm) | Đèn Launchpad của track đổi theo màu mới (màu gần nhất) | Thật |
| [ ] | Cài đặt → MIDI → Gán mới → Nút LOOP / Dừng track / Hoàn tác overdub → bấm footswitch | Footswitch làm y như nút tương ứng trên màn hình, trên track đang chọn | Thật |
| [ ] | Highlight Repaints khi LOOP đang phát | Chỉ nút LOOP (và ô đang phát) đổi màu, header không | Fake |

## 5. Session — transport bar

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Kéo BPM lên/xuống; chạm đúp | Mượt; chạm đúp về 120 | Fake |
| [ ] | TAP 4 lần theo nhịp | BPM bằng trung bình 4 lần chạm | Fake |
| [ ] | Đổi Q rồi launch clip | Ô nhấp nháy tới đúng ranh giới mới | Fake |
| [ ] | Menu metronome: Metronome + Đếm vào 1/2 bar | Chip hiện icon metronome + "Khi thu · 2"; count-in chạy trước khi thu | Fake |
| [ ] | Menu ♩: kéo thanh âm lượng metronome khi đang phát | Tiếng click to/nhỏ theo tay; mở lại project vẫn giữ | Thật |
| [ ] | Menu ♩: Nhịp 3/4 rồi 6/8 | Chip hiện "3/4"; vị trí bar.beat đếm 3 (hoặc 6) phách; quantize 1 bar theo nhịp mới | Fake/Thật |
| [ ] | Nút ● (REC jam) | Đỏ + đồng hồ chạy; bấm lại → SnackBar "Đã ghi jam_… (m:ss)" + "Chia sẻ" | Thật |
| [ ] | ⋮ → Export… / Cài đặt | Mở đúng sheet/màn | Fake |
| [ ] | Tên project rất dài | Tên co lại có "…", không tràn | Fake |

## 6. Session — chế độ Edit

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | ✎ Edit → chạm ô | Ô được chọn, tab Clip hiện nội dung | Fake |
| [ ] | Nhấn giữ ô → Nhân bản / Đổi tên / Copy → Dán / Màu track / Xoá | Mỗi thao tác đúng cả UI lẫn khi mở lại project | Fake |
| [ ] | Kéo 1 clip sang ô trống | Hỏi Di chuyển / Copy; ô đích đã có clip thì báo | Fake |
| [ ] | ✎ Edit → chạm nút scene ở cột phải | Hộp thoại đổi tên scene; **không** launch scene | Fake |
| [ ] | Overdub một clip (engine thật) → Edit → nhấn giữ ô | Có mục "Hoàn tác overdub" (chỉ bật khi vừa overdub) → bấm: bỏ lớp vừa chồng | Thật |

---

## 7. Tab Instrument

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Chạm tên track Keys ở header → tab Instrument | Hiện bàn phím 2 quãng tám; track Drums hiện pad 4×4 | Fake |
| [ ] | Chơi 10 ngón, trượt ngón qua phím, nhấc tay | Phím sáng/tắt đúng; **không còn nốt kêu** sau khi nhấc | Fake/Thật |
| [ ] | Nút −/+ quãng | Dịch quãng; nốt đang giữ được tắt trước | Fake |
| [ ] | "Thu âm mới" → nút đỏ | Meter nhảy, thời gian chạy, tự dừng ở 4 s | Fake |
| [ ] | Bước review | Waveform + "A3 +12 cent"; kéo handle trim; chọn Classic → "Tạo nhạc cụ" → tiến độ → sheet đóng | Fake |
| [ ] | Huỷ ở từng bước, rồi xem Files → Projects/<tên>.loopproj/instruments | Không còn thư mục của lần huỷ | Fake |
| [ ] | Engine thật: hát một nốt | Nốt gốc đúng (so với app tuner); bàn phím chơi được ngay | Thật |
| [ ] | Engine thật: bịt micro, thu 4 s im lặng | Báo "Không nghe thấy tiếng — kiểm tra micro rồi thu lại", quay về nút thu, không sang bước review | Thật |
| [ ] | Engine thật: vỗ tay / nói (không có cao độ rõ) | Bước review báo độ tin cậy thấp, nút Tạo khoá tới khi chọn nốt; nốt bắt đầu từ C4 | Thật |
| [ ] | Track có nhạc cụ tự thu: khối bên trái | Natural/Classic + 4 knob A/D/S/R; chỉnh R rồi chơi: nốt mới nhả dài hơn | Fake/Thật |

## 8. Tab Clip

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Cài đặt → số bar 2, quantize 1/16. Arm Keys → thu → chơi vài nốt | Chọn ô (Edit) → piano roll đúng nốt, playhead chạy | Fake |
| [ ] | Quantize / chọn nốt → Xoá / Clear | Nốt đổi đúng, mở lại vẫn giữ | Fake |
| [ ] | Edit → nhấn giữ ô trống của Keys → "Clip MIDI trống · 2 bar" | Có clip 8 beat, tab Clip mở piano roll ở chế độ ✏️ Vẽ, cột trái là phím đàn | Fake |
| [ ] | Vẽ: chạm ô trống trên lưới | Nốt mới đúng ô (lưới 1/16), nghe thử ngắn ngay; engine thật: clip đang phát nghe nốt mới ở vòng kế tiếp, không treo nốt | Fake/Thật |
| [ ] | Chạm nốt; kéo thân nốt; kéo mép phải nốt | Xoá / dời (snap, đổi cao độ) / đổi độ dài; chỉ gửi engine khi nhấc tay | Fake |
| [ ] | Đổi lưới 1/4 · 1/8 · 1/32, zoom 2×/4× rồi cuộn ngang, ▲▼ quãng tám | Nốt mới theo lưới mới; cuộn mượt; ▲▼ dịch 1 quãng tám | Fake |
| [ ] | Độ dài clip 1 bar khi có nốt ở bar 2; rồi ↶ / ↷ ở thanh công cụ | Nốt ngoài 1 bar bị bỏ; ↶ lấy lại (tối đa 50 bước, chỉ trong phiên) | Fake |
| [ ] | Chọn clip của Drums (kit) | Mỗi hàng là một pad có tên (Kick, Snare, Hat closed…); không có ▲▼ quãng tám | Fake |
| [ ] | Highlight Repaints khi kéo nốt lúc clip đang phát | Chỉ nốt đang kéo và vạch playhead đổi màu, lớp nốt và cột phím đứng yên | Fake |
| [ ] | Thu audio 1–2 bar trên track 5 → chọn ô | Waveform; kéo 2 handle loop; zoom 2×/4×, cuộn ngang mượt | Fake |
| [ ] | Kéo gain, đổi Re-Pitch | Engine thật: không có tiếng click | Thật |
| [ ] | Nút ↶ ở header tab Clip (piano roll / waveform) sau khi overdub | Bật khi có lớp undo; bấm → clip trở về trước lượt overdub, nút mờ lại | Thật |

## 9. Tab Mixer

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Kéo 2–3 fader cùng lúc bằng nhiều ngón | Mượt, nhãn dB chạy theo tay | Fake |
| [ ] | Chạm đúp fader / knob pan | Về 0 dB / giữa | Fake |
| [ ] | Giữ thêm một ngón hoặc kéo lệch ngang > 60 pt | Tinh chỉnh chậm lại ×0.1 | Fake |
| [ ] | M / S / fader Master | Đúng lệnh; nghe rõ với engine thật | Fake/Thật |
| [ ] | Chip 🎧 Off/Auto/On trên strip track audio | Đổi monitor: Auto nghe mic khi arm, On luôn nghe (dùng tai nghe!); track nhạc cụ không có chip | Fake/Thật |

## 10. Tab FX

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | "Thêm FX" → Delay | Card Delay; chỉ slot trống kế tiếp mới có nút thêm | Fake/Thật |
| [ ] | Kéo knob; chọn nhịp delay (1/16T … 1/2) | Giá trị chạy theo tay; nhịp đổi đúng | Fake/Thật |
| [ ] | Nút ⏻ (bypass) khi đang phát | Tắt/bật FX **không có tiếng click** | Thật |
| [ ] | Đổi loại FX; xoá slot 1 trong 3 | Slot sau dồn lên đúng thứ tự | Fake/Thật |
| [ ] | Card Master: EQ3 + trần limiter | Kéo được; mở lại project vẫn giữ | Fake/Thật |
| [ ] | Card Master: nút ⏻ của EQ | Tắt/bật EQ master không click; knob EQ mờ khi tắt; limiter không có nút tắt | Thật |

## 11. Tab Browser

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Chọn track 6 → gán Kit | Track đổi sang nhạc cụ; engine thật: chơi pad nghe được | Fake/Thật |
| [ ] | Gán loop Click 120 | Clip mới ở ô trống; engine thật: phát đúng nhịp | Fake/Thật |
| [ ] | Chọn clip Click 120 vừa gán → tab Clip | Warp đang **Re-Pitch** (manifest `defaultWarp`); loop Sine 120 thì **Stretch** | Fake |
| [ ] | Loop gắn tag `drums` (khi thư viện có) → đổi Warp sang Stretch | Chỗ nhãn "Warp" hiện gợi ý vàng "Loop trống nghe tự nhiên hơn khi Re-Pitch"; đổi lại Re-Pitch thì mất | Fake |
| [ ] | Như trên rồi thoát project, mở lại, chọn clip đó | Gợi ý vẫn còn (tag lưu trong project) | Fake |
| [ ] | iPad tiếng Anh → tab Browser | Tên tiếng Anh (Synth kit, Click 4 beats · 120…), dòng phụ hiện nhãn tag đã dịch (Drums · Test); gán loop → clip lấy tên tiếng Anh | Fake |

---

## 12. Export

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | ⋮ → Export… → tab "Ghi buổi jam" → ● → chơi → ■ | Share sheet mở ngay, popover neo đúng chỗ nút | Thật |
| [ ] | Tab "Export scene": scene, số bar, WAV/M4A, stems → Export | Thanh tiến độ; xong thì share sheet mở; stems tên `<tên>_t1…t8` | Thật |
| [ ] | Chọn scene chưa có clip | Nút Export tắt, báo "Scene này chưa có clip" | Fake |
| [ ] | Kéo gain master/track lên rất cao rồi Export scene | Share sheet vẫn mở; có thông báo "N mẫu bị clip: hãy giảm gain master" (không chặn) | Thật |
| [ ] | Ghi jam lâu trong lúc máy bận (vd. mở app khác nặng) | Nếu engine báo rớt khung: SnackBar "Đã ghi…" có thêm dòng "Mất X ms: bộ nhớ ghi không kịp" | Thật |
| [ ] | Lưu vào Files → "Trên iPad/Music Looper/Exports" | Thấy file; M4A mở được trong Files và Music | Thật |

## 13. Cài đặt

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Audio → Buffer 256 | Dòng "Đang chạy: 256 frame (5.3 ms)"; CPU giảm | Fake/Thật |
| [ ] | Audio → monitor mặc định, số bar, quantize MIDI, rung | Tắt hẳn app rồi mở lại: vẫn giữ | Fake |
| [ ] | Độ trễ → "Đo độ trễ" (loa ngoài hoặc cáp loopback) | Tiến độ → "Lần đo gần nhất" + mức bù tự đặt theo kết quả | Thật |
| [ ] | Độ trễ → "Đo độ trễ" khi tắt tiếng loa, không cáp loopback | "Đo lỗi: Không nghe thấy tiếng click đo — tăng âm lượng hoặc cắm cáp loopback"; mức bù cũ giữ nguyên | Thật |
| [ ] | Độ trễ → kéo "Bù thêm", thu thử | Take trễ thì tăng, sớm thì giảm; mở lại app vẫn giữ | Thật |
| [ ] | MIDI → cắm controller USB | Hiện trong "Thiết bị vào", bật/tắt được | Thật |
| [ ] | MIDI → "Ghép thiết bị Bluetooth MIDI" | Mở màn ghép của iOS (formSheet, nút Xong); ghép xong đóng lại → thiết bị mới hiện trong danh sách | Thật |
| [ ] | MIDI → Gán mới: Clip / Scene / Transport / Dừng tất cả / Gain / Mute / Tham số FX (cả Master) | Vặn/bấm nút → dòng gán hiện đúng thiết bị + kênh; bấm lại nút đó điều khiển đúng | Thật |
| [ ] | Link → bật | Số thiết bị đang nối (cần LinkKit, P4) | Thật |
| [ ] | Giấy phép → mở JUCE / Signalsmith / nội dung | Xem được toàn văn; LinkKit hiện mờ | Fake |
| [ ] | Giới thiệu | Phiên bản 0.1.0 (1), loại engine, sample rate, buffer | Fake |

## 13b. Bộ nhớ

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Simulator: Debug → Simulate Memory Warning (hoặc engine tự nhận cảnh báo RAM của iOS) | Banner nhẹ "Đã giải phóng bộ nhớ (còn dùng X MB)", đóng được, không chặn thao tác; mức nghiêm trọng thì banner vàng | Thật |

## 14. Chế độ chỉ phát (từ chối micro)

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Cài đặt iOS → Music Looper → tắt Micro, mở app, mở Demo | Vẫn có tiếng khi phát | Thật |
| [ ] | Nhìn banner trên grid | "Chưa có quyền micro: chỉ phát được, chưa thu được" + "Mở Cài đặt" (đã từ chối) / "Cho phép micro" (chưa hỏi) | Fake/Thật |
| [ ] | Đã từ chối: arm track audio; "Thu âm mới"; "Đo độ trễ" | Bị khoá (mờ, chạm không có tác dụng) | Fake/Thật |
| [ ] | Arm track instrument (thu MIDI); ● REC jam | Vẫn dùng được | Fake/Thật |
| [ ] | Bật lại Micro trong Cài đặt iOS, quay về app | Banner mất, thu audio được, không phải khởi động lại app | Thật |

---

## 15. Ngôn ngữ (English / Tiếng Việt)

App theo ngôn ngữ của iPad: tiếng Việt → tiếng Việt, **mọi ngôn ngữ khác → tiếng Anh**. Không có chỗ chọn ngôn ngữ
trong app.

| ✓ | Bước | Kết quả đúng | Engine |
|---|---|---|---|
| [ ] | Cài đặt iOS → **Music Looper → Ngôn ngữ → English** (hoặc đổi ngôn ngữ cả iPad: Cài đặt chung → Ngôn ngữ & vùng) | iOS tự khởi động lại app; mở lại thấy tiếng Anh | Fake/Thật |
| [ ] | Đi nhanh lại mục 2–14 bằng tiếng Anh: Projects, Session, 5 tab dưới, Edit, Export, Cài đặt, banner quyền micro | Không còn chữ tiếng Việt, trừ **nội dung người dùng** (tên project/track/clip/scene) và tên trong thư viện (manifest mới có tiếng Việt). Không nhãn quan trọng nào bị cắt "…" | Fake |
| [ ] | Số nhiều tiếng Anh | "1 bar" / "2 bars" (Cài đặt → Audio), "Connected: 1 peer" / "2 peers" (Link) | Fake/Thật |
| [ ] | Gây một lỗi (engine giả: Cài đặt → Link → bật) | Câu lỗi đã dịch, không hiện message kỹ thuật của engine; mã lạ thì "Engine error (MÃ)" | Fake |
| [ ] | Xoá app, cài lại, vào chỗ cần micro (Onboarding / Thu âm mới) | Hộp xin quyền micro của iOS bằng tiếng Anh | Thật |
| [ ] | Đổi sang **Français** (hoặc ngôn ngữ bất kỳ khác en/vi) | App vẫn tiếng Anh | Fake |
| [ ] | Đổi lại **Tiếng Việt**; xoá + cài lại để xem hộp xin quyền | Toàn app và hộp xin quyền micro bằng tiếng Việt | Fake/Thật |
| [ ] | Màn spike (không cờ `LOOPCORE_START`) ở cả hai ngôn ngữ | Nhãn đã dịch; lỗi engine trên màn spike vẫn hiện nguyên message engine (màn dev, cố ý) | Fake/Thật |

> Test tự động: `test/l10n/l10n_test.dart` (2 ARB cùng key + placeholder, chọn ngôn ngữ, Info.plist),
> `test/ui_review/ui_rules_test.dart` (chặn chuỗi hiển thị viết cứng), layout stress chạy cả en và vi, ảnh golden
> `test/ui_review/goldens/{session,settings}_{en,vi}.png` để xem bằng mắt (ô xám trong ảnh = chữ/ký hiệu dùng font
> hệ thống, trên iPad hiện bình thường).

---

## 16. Trước khi nộp TestFlight (P4-21)

| ✓ | Bước | Kết quả đúng |
|---|---|---|
| [ ] | Xcode → Product → Archive → Organizer → chuột phải bản archive → **Generate Privacy Report** | Có System Boot Time (35F9.1) và File Timestamp (C617.1) từ app + `engine_ffi`; không có Disk Space / User Defaults; Tracking = No |
| [ ] | Upload lên App Store Connect | Không có email cảnh báo ITMS-91053 (thiếu khai báo required-reason API) |

---

## 17. Hiệu năng và repaint (làm cuối, trước mỗi mốc)

| ✓ | Bước | Kết quả đúng |
|---|---|---|
| [ ] | Để yên Session 5 giây (transport dừng) | Không viền nào đổi màu |
| [ ] | Launch scene 8 track, để yên | Chỉ ô đang phát, meter, readout vị trí/CPU đổi màu. Overlay 60 fps |
| [ ] | Đang phát + chơi phím + kéo fader cùng lúc | 60 fps; chỉ vùng phím / fader + nhãn dB / meter đổi màu |
| [ ] | Tab FX để yên; kéo 1 knob | Để yên: không viền nào đổi màu. Kéo: chỉ knob và ô giá trị của nó |
| [ ] | Đang ghi jam | Chỉ ô đồng hồ ở thanh trên nhảy mỗi giây (không cả thanh) |
| [ ] | Mở Cài đặt, để yên | Không viền nào đổi màu; quay về Session thì meter chạy lại |
| [ ] | Dùng liên tục 1 giờ (M2) | Không crash |

> Các điểm trên đã có test tự động kiểm phần logic: `test/perf/rebuild_budget_test.dart` (8 track phát 120 frame:
> 0 rebuild; chơi 12 nốt: 0 rebuild; kéo fader 60 frame: chỉ nhãn dB), `test/ui_review/layout_stress_test.dart`
> (tràn chữ, boundary). Thử tay trên iPad để xác nhận **fps và GPU thật**.
