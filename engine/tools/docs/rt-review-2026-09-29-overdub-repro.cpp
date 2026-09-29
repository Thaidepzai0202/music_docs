// Tái hiện R1 của engine/tools/docs/rt-review-2026-09-29.md (agent 80) — KHÔNG nằm trong build.
// Bật lại overdub trong lúc RT còn kết thúc lượt trước và main chưa pump. App thật pump 30 Hz (~12 block @128)
// → ở đây render nhiều block giữa hai lần pump (sim.advance / ScenarioRunner pump sau MỖI block nên không lộ ra).
//
// Build (dùng cờ của le-engine-bench trong build dir của bạn, VD mac-asan):
//   c++ <cờ compile của tools/engine_bench_main.cpp> -c rt-review-2026-09-29-overdub-repro.cpp -o repro.o
//   c++ repro.o -o repro <LINK_LIBRARIES của le-engine-bench>
//   ./repro       → "RT VẪN GHI vào target main đã gỡ", RECORDING_FINISHED tổng = 1 (lượt 2 không được lưu)
//   ./repro uaf   → bản ASan: heap-use-after-free tại Recorder::processOverdub (Recorder.cpp:50)
#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
std::atomic<int> g_recFinished{0};
void onEvent(int32_t type, int32_t, int32_t, int64_t, double) {
    if (type == LE_EVT_RECORDING_FINISHED) g_recFinished.fetch_add(1);
}
LeCommand cmd(int32_t type, int track = -1, int slot = -1, int i0 = 0) {
    LeCommand c{};
    c.type = static_cast<decltype(c.type)>(type);
    c.track = static_cast<decltype(c.track)>(track);
    c.slot = static_cast<decltype(c.slot)>(slot);
    c.i0 = i0;
    return c;
}
} // namespace

int main(int argc, char** argv) {
    const bool uaf = argc > 1 && std::string(argv[1]) == "uaf";
    LeConfig cfg{};
    cfg.apiVersion = LE_API_VERSION;
    cfg.numInputChannels = 1;
    cfg.preferredBufferSize = 128;
    cfg.preferredSampleRate = 48000.0;
    auto devOwned = std::make_unique<le::io::OfflineDeviceIO>(128);
    auto* dev = devOwned.get();
    dev->setLatencies(1000, 1000);   // L = 2000 sample
    le::core::setEventCallback(&onEvent);
    le::core::Engine eng(cfg, std::move(devOwned));
    if (eng.audioStart() != LE_OK) return 2;

    std::vector<float> in(128, 0.25f), outL(128), outR(128);   // input hằng 0.25 → overdub cộng vào clip
    const float* ins[1] = {in.data()};
    float* outs[2] = {outL.data(), outR.data()};
    auto render = [&](int blocks) { for (int i = 0; i < blocks; ++i) dev->render(ins, 1, outs, 2, 128); };

    const std::string req = std::string(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"c0","lengthBeats":4,"originalBpm":120,"file":")") +
                            LE_ENGINE_DIR "/tests/fixtures/clip_silence_4beats_120.wav\"}";
    std::printf("setAudio: %s\n", eng.call(req.c_str()).c_str());
    for (int i = 0; i < 400 && !eng.model().clips[0][0].has_value(); ++i) {
        eng.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!eng.model().clips[0][0].has_value()) { std::printf("clip không nạp được\n"); return 2; }

    eng.send(cmd(LE_CMD_SET_QUANTIZE, -1, -1, LE_Q_NONE));
    eng.send(cmd(LE_CMD_CLIP_LAUNCH, 0, 0));
    render(4); eng.pump();

    eng.send(cmd(LE_CMD_OVERDUB_TOGGLE, 0));   // BẬT lần 1 → main tạo target T
    render(24); eng.pump();
    const auto* TD = eng.model().clips[0][0]->audio.get();
    const float* T = TD->channel(0);
    auto sumT = [&] { double s = 0; for (std::int64_t i = 0; i < TD->numFrames(); ++i) s += T[i]; return s; };
    std::printf("sau overdub 1 đang chạy: tổng T = %g\n", sumT());

    eng.send(cmd(LE_CMD_OVERDUB_TOGGLE, 0));   // TẮT
    render(2);                                  // < L: RT vẫn đang ghi nốt phần bù latency
    eng.send(cmd(LE_CMD_OVERDUB_TOGGLE, 0));   // BẬT lại ngay (double-tap / footswitch dội) — main: target != null → bỏ qua
    render(24);                                 // RT: xong lượt 1 (OverdubFinished) rồi MỞ lượt 2 với T cũ
    eng.pump();                                 // main: xử lý OverdubFinished → xoá overdub_[0], gỡ target, persist
    int evAfterFirst = 0;
    for (int i = 0; i < 200 && g_recFinished.load() == 0; ++i) { eng.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    evAfterFirst = g_recFinished.load();

    LeState st{};
    le::core::globalStatePublisher().read(st);
    std::printf("state ô (0,0) = %d (OVERDUBBING = %d)\n", st.clipState[0][0], LE_CLIP_OVERDUBBING);
    std::printf("model còn giữ T? %s\n", eng.model().clips[0][0]->audio.get() == TD ? "có" : "không");
    const double before = sumT();
    render(48); eng.pump();                     // main nghĩ overdub đã xong — RT còn ghi vào T?
    const double after = sumT();
    std::printf("tổng T trước = %g, sau = %g → %s\n", before, after, after > before + 1.0 ? "RT VẪN GHI vào target main đã gỡ" : "không đổi");

    if (uaf) {   // lượt 2 (không ai theo dõi) đang ghi vào T → xoá clip: model + snapshot thả T
        std::printf("clip.clear: %s\n", eng.call(R"({"op":"clip.clear","track":0,"slot":0})").c_str());
        for (int k = 0; k < 20; ++k) { render(2); eng.pump(); }   // Retire → delete snapshot trên main; RT còn ghi phần bù L
        std::printf("uaf mode xong (ASan im lặng = không có use-after-free)\n");
        return 0;
    }
    eng.send(cmd(LE_CMD_OVERDUB_TOGGLE, 0));   // TẮT lượt 2
    render(40);
    for (int i = 0; i < 100; ++i) { eng.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    le::core::globalStatePublisher().read(st);
    std::printf("RECORDING_FINISHED: sau lượt 1 = %d, tổng = %d (mong đợi 2 nếu lượt 2 được lưu)\n", evAfterFirst, g_recFinished.load());
    std::printf("state cuối = %d\n", st.clipState[0][0]);
    return 0;
}
