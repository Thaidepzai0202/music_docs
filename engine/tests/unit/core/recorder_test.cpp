// P1-18 (arm + RecordBuffer), P1-21 (ghi đĩa + RECORDING_FINISHED + clip.info) với Engine offline.
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/OfflineDeviceIO.h"

using namespace le;

namespace {
struct Ev {
    int32_t type, a, b;
    double value;
};
std::vector<Ev>& events() {
    static std::vector<Ev> v;
    return v;
}
void onEvent(int32_t type, int32_t a, int32_t b, int64_t, double value) { events().push_back({type, a, b, value}); }

struct Rig {
    io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<core::Engine> e;
    std::vector<float> in = std::vector<float>(128), L = std::vector<float>(128), R = std::vector<float>(128);
    std::int64_t t = 0;
    explicit Rig(int inputs = 1) {
        events().clear();
        core::setEventCallback(&onEvent);
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredSampleRate = 48000.0;
        c.preferredBufferSize = 128;
        c.numInputChannels = inputs;
        auto d = std::make_unique<io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<core::Engine>(c, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
    }
    ~Rig() {
        e.reset();
        core::setEventCallback(nullptr);
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    bool send(uint16_t type, int track = -1, int slot = -1, int32_t i0 = 0) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        return e->send(c);
    }
    // Render n frame với input = sine 440 Hz (0.5) theo thời gian tuyệt đối → dữ liệu take biết trước.
    void render(std::int64_t frames) {
        for (std::int64_t done = 0; done < frames; done += 128) {
            for (int i = 0; i < 128; ++i) in[(size_t) i] = 0.5f * (float) std::sin(2.0 * M_PI * 440.0 * (double) (t + i) / 48000.0);
            const float* ins[1] = {in.data()};
            float* outs[2] = {L.data(), R.data()};
            dev->render(ins, 1, outs, 2, 128);
            t += 128;
            e->pump();
        }
    }
    bool waitEvent(int32_t type, int ms = 3000) {
        for (int i = 0; i < ms / 5; ++i) {
            e->pump();
            for (const auto& ev : events())
                if (ev.type == type) return true;
            juce::Thread::sleep(5);
        }
        return false;
    }
};
} // namespace

TEST_CASE("P1-21: thu 1 bar → clip.info có file audio/<id>.caf, RECORDING_FINISHED sau khi đóng file, đọc lại giống từng bit",
          "[core][recorder]") {
    const juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-rec", "", false);
    dir.createDirectory();
    {
        Rig r;
        REQUIRE((bool) r.call(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})")["ok"]);
        REQUIRE(r.send(LE_CMD_CLIP_RECORD, 1, 2, 1));   // transport dừng → play ở beat 0, thu 1 bar (không arm trước: tự cấp phát)
        r.render(96000 + 1024);                          // qua E + đuôi
        REQUIRE(r.waitEvent(LE_EVT_RECORDING_FINISHED));
        const auto& fin = events().back();
        REQUIRE(fin.type == LE_EVT_RECORDING_FINISHED);
        REQUIRE(fin.a == 1);
        REQUIRE(fin.b == 2);

        const auto info = r.call(R"({"op":"clip.info","track":1,"slot":2})")["result"];
        REQUIRE(info["kind"].toString() == "audio");
        REQUIRE((double) info["lengthBeats"] == 4.0);
        REQUIRE((double) info["originalBpm"] == 120.0);
        const std::string file = info["file"].toString().toStdString();
        REQUIRE(file.rfind("audio/rec_", 0) == 0);
        REQUIRE(file.size() > 4);
        REQUIRE(file.substr(file.size() - 4) == ".caf");

        const auto* snap = r.e->rt().currentSnapshot();
        const auto* take = snap->clips[1][2].audio.get();
        REQUIRE(take != nullptr);
        REQUIRE(take->numFrames() >= 96000);
        // Nội dung take = input từ sample 0 (latency 0): sine ở thời điểm tuyệt đối i
        for (int i : {0, 1, 1000, 95999})
            REQUIRE(take->channel(0)[i] == 0.5f * (float) std::sin(2.0 * M_PI * 440.0 * (double) i / 48000.0));

        const auto dec = io::decodeAudioFile(dir.getChildFile(file).getFullPathName().toStdString());
        REQUIRE(dec.error == LE_OK);
        REQUIRE(dec.data->numFrames() == take->numFrames());
        REQUIRE(dec.data->sampleRate() == 48000.0);
        bool same = true;
        for (std::int64_t i = 0; i < take->numFrames(); ++i) same &= dec.data->channel(0)[i] == take->channel(0)[i];
        REQUIRE(same);   // giống từng bit
    }
    dir.deleteRecursively();
}

TEST_CASE("P1-21: project.open trong lúc thu → take bị bỏ, không có clip", "[core][recorder]") {
    Rig r;
    REQUIRE(r.send(LE_CMD_CLIP_RECORD, 0, 0, 1));
    r.render(40000);
    r.call(R"({"op":"project.open","dir":"/tmp/le-rec-x"})");
    r.render(96000);
    REQUIRE(r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["kind"].toString() == "empty");
    for (const auto& ev : events()) REQUIRE(ev.type != LE_EVT_RECORDING_FINISHED);
}

TEST_CASE("P1-19: hai take liên tiếp cùng track (arena) đều vào model đúng độ dài", "[core][recorder]") {
    Rig r;
    REQUIRE(r.send(LE_CMD_CLIP_RECORD, 0, 0, 1));
    r.render(24000);
    REQUIRE(r.send(LE_CMD_CLIP_RECORD, 0, 1, 2));   // xếp ngay sau: bắt đầu ở beat 4 khi take 1 vừa xong
    r.render(96000 * 3 + 2048);
    REQUIRE(r.call(R"({"op":"clip.info","track":0,"slot":0})")["result"]["lengthBeats"].toString() == "4.0");
    REQUIRE((double) r.call(R"({"op":"clip.info","track":0,"slot":1})")["result"]["lengthBeats"] == 8.0);
    const auto* snap = r.e->rt().currentSnapshot();
    const auto* take2 = snap->clips[0][1].audio.get();
    REQUIRE(take2 != nullptr);
    // take 2 bắt đầu ở sample 96000 → phần tử i = input tại 96000 + i (tránh điểm cắt 0 của sine)
    for (int i : {17, 100, 12345, 191000})
        REQUIRE(take2->channel(0)[i] == 0.5f * (float) std::sin(2.0 * M_PI * 440.0 * (double) (96000 + i) / 48000.0));
}

TEST_CASE("P1-18: arm / disarm / record liên tục trong lúc audio thread đang chạy (RTSan/TSan)", "[core][recorder][stress]") {
    Rig r;
    std::atomic<bool> running{true};
    std::thread audio([&] {
        std::vector<float> in(128, 0.1f), L(128), R(128);
        const float* ins[1] = {in.data()};
        float* outs[2] = {L.data(), R.data()};
        while (running.load()) r.dev->render(ins, 1, outs, 2, 128);
    });
    for (int i = 0; i < 400; ++i) {
        const int t = i % LE_MAX_TRACKS;
        REQUIRE(r.send(LE_CMD_TRACK_ARM, t, -1, i % 2));   // arm lần đầu: main cấp phát buffer TRƯỚC khi push
        if (i % 7 == 0) r.send(LE_CMD_CLIP_RECORD, t, i % 8, 1);
        if (i % 11 == 0) r.send(LE_CMD_TRANSPORT_STOP);
        r.e->pump();
        juce::Thread::sleep(1);
    }
    running = false;
    audio.join();
    r.e->pump();
    SUCCEED();
}

TEST_CASE("P1-22: overdub 1 vòng rồi clip.undoOverdub → giống bản trước overdub TỪNG BIT (cùng object)", "[core][recorder][overdub]") {
    const juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("le-od", "", false);
    dir.createDirectory();
    {
        Rig r;
        REQUIRE((bool) r.call(R"({"op":"project.open","dir":")" + dir.getFullPathName().toStdString() + R"("})")["ok"]);
        REQUIRE(r.send(LE_CMD_CLIP_RECORD, 0, 0, 1));   // take 1 bar (input = sine) làm clip gốc
        r.render(96000 + 1024);
        REQUIRE(r.waitEvent(LE_EVT_RECORDING_FINISHED));
        const auto* original = r.e->rt().currentSnapshot()->clips[0][0].audio.get();
        REQUIRE(original != nullptr);
        std::vector<float> before(original->channel(0), original->channel(0) + original->numFrames());

        REQUIRE(r.call(R"({"op":"clip.undoOverdub","track":0,"slot":0})")["error"]["code"].toString() == "INVALID_ARG");   // chưa overdub
        events().clear();
        REQUIRE(r.send(LE_CMD_OVERDUB_TOGGLE, 0));       // bật (main chuẩn bị bản copy trước khi push)
        r.render(96000);                                 // 1 vòng
        REQUIRE(r.call(R"({"op":"clip.undoOverdub","track":0,"slot":0})")["error"]["code"].toString() == "INVALID_ARG");   // đang overdub
        REQUIRE(r.send(LE_CMD_OVERDUB_TOGGLE, 0));       // tắt
        r.render(2048);
        REQUIRE(r.waitEvent(LE_EVT_RECORDING_FINISHED)); // file + peaks của bản sau overdub
        const auto* after = r.e->rt().currentSnapshot()->clips[0][0].audio.get();
        REQUIRE(after != original);
        bool changed = false;
        for (std::int64_t i = 0; i < 96000; ++i) changed |= after->channel(0)[i] != before[(size_t) i];
        REQUIRE(changed);   // input đã được cộng vào
        REQUIRE(original->channel(0)[1234] == before[1234]);   // bản gốc KHÔNG bị sửa tại chỗ

        REQUIRE((bool) r.call(R"({"op":"clip.undoOverdub","track":0,"slot":0})")["ok"]);
        r.render(128);
        const auto* undone = r.e->rt().currentSnapshot()->clips[0][0].audio.get();
        REQUIRE(undone == original);
        bool same = true;
        for (std::int64_t i = 0; i < undone->numFrames(); ++i) same &= undone->channel(0)[i] == before[(size_t) i];
        REQUIRE(same);
        REQUIRE(r.call(R"({"op":"clip.undoOverdub","track":0,"slot":0})")["error"]["code"].toString() == "INVALID_ARG");   // chỉ 1 lớp
    }
    dir.deleteRecursively();
}
