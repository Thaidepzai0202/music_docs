# AVAudioSession trên iOS: JUCE làm gì và engine đặt lại gì (P0-06)

Nguồn đọc: `engine/third_party/JUCE/modules/juce_audio_devices/native/juce_Audio_ios.cpp` (JUCE **9.0.3**;
từ JUCE 9 file đổi tên từ `juce_ios_Audio.cpp`). Code engine: `src/io/ios/AudioSession_ios.mm`, `src/io/JuceDeviceIO.cpp`.

## 1. JUCE 9.0.3 tự cấu hình session thế nào

| Lúc nào | JUCE làm gì | Dòng |
|---|---|---|
| Tạo device (`Pimpl` ctor) | `setCategory(PlayAndRecord, options)` → `setActive(true)` để đọc SR / buffer khả dụng → `setActive(false)` | ~347 |
| `open()` (mỗi lần `AudioDeviceManager` mở hoặc mở lại device, kể cả khi đổi buffer) | `setActive(true)` → `setCategory(inputs > 0 ? PlayAndRecord : Playback, options)` → `setPreferredSampleRate` → `setPreferredIOBufferDuration` → `fixAudioRouteIfSetToReceiver` → start RemoteIO | ~681–727 |
| Interruption bắt đầu | `handleStatusChange(false)`: `setActive(false)`, `AudioOutputUnitStop`, gọi `audioDeviceError(reason)` | ~975 |
| Interruption kết thúc | `handleStatusChange(true)`: `setActive(true)`, `AudioOutputUnitStart`. **Không** gọi `audioDeviceAboutToStart` | ~975 |
| Media services reset | `handleStatusChange(true)`, **không** tạo lại AudioUnit (sau reset, unit cũ không còn hợp lệ) | ~187 |
| Route change | Gọi `audioDeviceError(reason)` nếu đang chạy. Với cắm/rút thiết bị: đọc lại phần cứng (async) → `callDeviceChangeListeners` | ~993 |

**Options JUCE đặt** (hàm `setAudioSessionCategory`, ~364):
- Luôn có `MixWithOthers`, trừ khi build với `JUCE_DISABLE_AUDIO_MIXING_WITH_OTHER_APPS=1`.
- Với `PlayAndRecord`: `DefaultToSpeaker | AllowAirPlay | AllowBluetoothA2DP | AllowBluetoothHFP`
  (với SDK iOS 26 là `AllowBluetoothHFP`, SDK cũ hơn là `AllowBluetooth`: cùng một bit).

→ **JUCE bật HFP.** Với HFP, khi kết nối AirPods, iOS có thể chuyển sang profile cuộc gọi (SR 8–24 kHz, mono,
chất lượng thấp). Điều này trái yêu cầu của dự án.

**Mode:** JUCE **không** đặt mode trong `open()`. Mode chỉ đổi qua `iOSAudioIODevice::setAudioPreprocessingEnabled(bool)`:
`true` → `AVAudioSessionModeDefault`, `false` → `AVAudioSessionModeMeasurement`. Nếu không ai gọi thì mode là
`Default` (mặc định của hệ thống).

## 2. Engine đặt lại

Sau **mỗi** lần JUCE mở device (`JuceDeviceIO::start` và `restart`), engine gọi `session::applyCategory(mode, withInput)`:

```objc
[session setCategory:AVAudioSessionCategoryPlayAndRecord            // Playback nếu numInputChannels = 0
                mode:(measurement ? AVAudioSessionModeMeasurement : AVAudioSessionModeDefault)
             options:AVAudioSessionCategoryOptionMixWithOthers
                   | AVAudioSessionCategoryOptionDefaultToSpeaker
                   | AVAudioSessionCategoryOptionAllowBluetoothA2DP
               error:&err];
```

- **Không HFP**: bỏ `AllowBluetoothHFP`. AirPods chỉ còn là đầu ra A2DP; mic vẫn là mic trong máy.
- **Không AirPlay**: độ trễ AirPlay (~2 giây) vô dụng với looper. **Đã chốt** (người dùng, 29/09/2026).
- **Giữ `MixWithOthers`** như JUCE (chạy cùng app nhạc khác, cần cho Link ở P4). **Đã chốt** (người dùng, 29/09/2026).
- Đổi category trong lúc session đang active sẽ sinh route change với lý do `CategoryChange`. JUCE gọi
  `audioDeviceError`, engine chỉ đếm. Observer của engine **bỏ qua** lý do này, để không phát `LE_EVT_ROUTE_CHANGED` giả.
- Đổi mode lúc đang chạy: `le_call {"op":"spike.setSessionMode","mode":"default"|"measurement"}`, áp dụng ngay.
- Kiểm tra trên máy: `le_call {"op":"spike.sessionInfo"}` trả category, mode, từng bit option
  (`allowBluetoothHFP` phải là `false`), SR, IO buffer, latency, danh sách cổng in/out hiện tại.

### Event
Observer Obj-C (`installObservers`) chỉ tăng bộ đếm atomic tĩnh, vì notification có thể đến từ thread phụ.
Timer 30Hz trên main so sánh bộ đếm với lần trước rồi phát:
- `LE_EVT_AUDIO_INTERRUPTED` a=1 (bắt đầu) / a=0 (kết thúc)
- `LE_EVT_ROUTE_CHANGED` a=1 nếu có cổng có dây / USB / line, b=1 nếu có Bluetooth; engine đọc lại latency

## 3. Mode `default` và `measurement`

| | `default` | `measurement` |
|---|---|---|
| Xử lý tín hiệu mic trong máy | Có AGC (tự chỉnh gain) và EQ của hệ thống | Tắt gần hết xử lý: đáp tuyến phẳng hơn, mức thu thường nhỏ hơn |
| Hợp với | Thu nói hoặc hát tự do, mức to đều | Mẫu cho sampler (giữ động lực, không bị "bơm"), đo latency |
| Rủi ro | AGC làm take bị bơm gain, pre-render sampler lệch mức | Một số máy cho loa ngoài nhỏ hơn ở mode này (cần nghe thử trên iPad 8) |

**Kết luận tạm (chờ nghe thử trên iPad 8):** spike mặc định dùng `default`. Nếu `measurement` thu sạch hơn và
loa ngoài không nhỏ đi rõ rệt thì chuyển mặc định sang `measurement` (ít nhất khi thu mẫu cho sampler, P3).

## 4. Rủi ro cần kiểm ở P0-10
1. **Interruption kết thúc**: JUCE tự `AudioOutputUnitStart` nhưng không báo `audioDeviceAboutToStart`. Cần xem sau Siri /
   FaceTime âm thanh có tự chạy lại không (bảng P0-10).
2. **Media services reset**: JUCE không tạo lại AudioUnit → nhiều khả năng mất tiếng. Engine xử lý bằng cách `stop()` + `start()` device trên main khi nhận `AVAudioSessionMediaServicesWereResetNotification` (P1, Engine::pump).
3. **RT**: `AudioDeviceManager::audioDeviceIOCallbackInt` giữ `ScopedLock audioCallbackLock` (lock chặn thật) trên audio
   thread. Lock này chỉ bị tranh chấp khi main đổi callback hoặc cấu hình device. Callback RemoteIO của JUCE dùng `ScopedTryLock`.
   Ngoài ra, nếu iOS gửi block **lớn hơn** buffer đã cấp phát, JUCE sẽ resize buffer ngay trên audio thread (`setFloatBufferSize`,
   ~1104). Nếu P0 thấy xrun lúc khoá màn hình hoặc đổi route thì đây là nghi phạm đầu tiên → cân nhắc Plan B (03 §8).
4. Phía Flutter: **không** plugin nào khác được gọi `setCategory` / `setActive` (chỉ xin quyền mic).
