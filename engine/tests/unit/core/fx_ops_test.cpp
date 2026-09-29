// P3-12 fx.set / fx.remove / LE_CMD_FX_* qua Engine thật (OfflineDeviceIO) · P3-15 master cố định.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"

using namespace le::core;

namespace {
struct Offline {
    le::io::OfflineDeviceIO* dev = nullptr;
    std::unique_ptr<Engine> e;
    explicit Offline(double sr = 48000.0) {
        LeConfig c{};
        c.apiVersion = LE_API_VERSION;
        c.preferredBufferSize = 128;
        c.preferredSampleRate = sr;
        auto d = std::make_unique<le::io::OfflineDeviceIO>(1024);
        dev = d.get();
        e = std::make_unique<Engine>(c, std::move(d));
        REQUIRE(e->audioStart() == LE_OK);
    }
    juce::var call(const std::string& j) {
        juce::var v;
        juce::JSON::parse(juce::String::fromUTF8(e->call(j.c_str()).c_str()), v);
        return v;
    }
    bool ok(const std::string& j) { return (bool) call(j)["ok"]; }
    std::string err(const std::string& j) { return call(j)["error"]["code"].toString().toStdString(); }
    bool send(uint16_t type, int track, int slot, int32_t i0, float f0 = 0.0f) {
        LeCommand c{};
        c.type = type;
        c.track = (int8_t) track;
        c.slot = (int8_t) slot;
        c.i0 = i0;
        c.f0 = f0;
        return e->send(c);
    }
    // Render (không pump) → trả kênh L.
    std::vector<float> renderRaw(int frames) {
        std::vector<float> L((size_t) frames), R((size_t) frames);
        for (int done = 0; done < frames; done += 128) {
            const int n = std::min(128, frames - done);
            float* outs[2] = {L.data() + done, R.data() + done};
            dev->render(nullptr, 0, outs, 2, n);
        }
        return L;
    }
    std::vector<float> render(int frames) {
        auto L = renderRaw(frames);
        e->pump();
        return L;
    }
};

constexpr const char* kFilter = R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"0":0,"1":800,"2":0.7}})";
} // namespace

TEST_CASE("fx.set / fx.remove: kiểm tham số theo 05 §3", "[core][fx][api]") {
    Offline o;
    REQUIRE(o.ok(kFilter));
    REQUIRE(o.ok(R"({"op":"fx.set","track":7,"index":2,"type":"comp"})"));         // "comp" là tên chuẩn
    REQUIRE(o.ok(R"({"op":"fx.set","track":6,"index":1,"type":"compressor"})"));   // bí danh
    REQUIRE(o.ok(R"({"op":"fx.set","track":1,"index":1,"type":"reverb","params":{},"bypass":true})"));
    REQUIRE(o.err(R"({"op":"fx.set","track":-1,"index":0,"type":"eq3"})") == "INVALID_ARG");   // master: LE_CMD_FX_PARAM
    REQUIRE(o.err(R"({"op":"fx.set","track":8,"index":0,"type":"eq3"})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":3,"type":"eq3"})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"chorus"})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"3":1}})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"x":1}})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"1":"a"}})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":[1]})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.set","track":0,"index":0,"type":"filter","bypass":3})") == "INVALID_ARG");
    REQUIRE(o.err(R"({"op":"fx.remove","track":-1,"index":0})") == "INVALID_ARG");   // master cố định
    REQUIRE(o.err(R"({"op":"fx.remove","track":0,"index":5})") == "INVALID_ARG");
    REQUIRE(o.ok(R"({"op":"fx.remove","track":3,"index":2})"));   // slot trống vẫn ok

    const auto& m = o.e->model();
    REQUIRE(m.tracks[0].fx[0]->type == le::dsp::FxType::Filter);
    REQUIRE(m.tracks[0].fx[0]->params[1] == 800.0f);
    REQUIRE(m.tracks[0].fx[0]->params[2] == 0.7f);
    REQUIRE(m.tracks[1].fx[1]->bypass);
    REQUIRE(m.tracks[1].fx[1]->params[3] == 0.25f);   // mặc định của reverb mix
    REQUIRE(m.tracks[6].fx[1]->type == le::dsp::FxType::Compressor);
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":1,"type":"filter","params":{"1":99999}})"));
    REQUIRE(m.tracks[0].fx[1]->params[1] == 20000.0f);   // kẹp theo ParamInfo
}

TEST_CASE("LE_CMD_FX_PARAM / FX_BYPASS: kiểm theo model, ghi tham số vào model", "[core][fx]") {
    Offline o;
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, 0, 0, 1, 500.0f));   // slot trống
    REQUIRE(o.ok(kFilter));
    REQUIRE(o.send(LE_CMD_FX_PARAM, 0, 0, 1, 500.0f));
    REQUIRE(o.e->model().tracks[0].fx[0]->params[1] == 500.0f);
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, 0, 0, 3, 1.0f));   // filter chỉ có 3 tham số
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, 0, 3, 0, 1.0f));   // slot 3 không tồn tại
    REQUIRE(o.send(LE_CMD_FX_BYPASS, 0, 0, 1));
    REQUIRE(o.e->model().tracks[0].fx[0]->bypass);
    REQUIRE_FALSE(o.send(LE_CMD_FX_BYPASS, 0, 1, 1));   // slot trống

    // Master cố định: slot 0 = EQ3 (band 0..2), slot 1 = limiter (0 trần, 1 release)
    REQUIRE(o.send(LE_CMD_FX_PARAM, -1, 0, 2, 4.5f));
    REQUIRE(o.send(LE_CMD_FX_PARAM, -1, 1, 0, -3.0f));
    REQUIRE(o.send(LE_CMD_FX_PARAM, -1, 1, 1, 120.0f));
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, -1, 0, 3, 1.0f));
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, -1, 1, 2, 1.0f));
    REQUIRE_FALSE(o.send(LE_CMD_FX_PARAM, -1, 2, 0, 1.0f));
    REQUIRE(o.send(LE_CMD_FX_BYPASS, -1, 0, 1));
    REQUIRE_FALSE(o.send(LE_CMD_FX_BYPASS, -1, 1, 1));
    const MasterModel& mm = o.e->model().master;
    REQUIRE(mm.eq3[2] == 4.5f);
    REQUIRE(mm.limiterCeilingDb == -3.0f);
    REQUIRE(mm.limiterReleaseMs == 120.0f);
    REQUIRE(mm.eqBypass);
    o.render(256);
    REQUIRE(o.e->rt().masterEq().gainDb(2) == 4.5f);
    REQUIRE(o.e->rt().mixer().limiter().ceiling() == std::pow(10.0f, -3.0f / 20.0f));

    // project.open: master về mặc định cả ở model lẫn RT (Dart chỉ gửi EQ khi khác phẳng)
    const std::string dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("le_fx_ops").getFullPathName().toStdString();
    REQUIRE(o.ok(R"({"op":"project.open","dir":")" + dir + R"("})"));
    REQUIRE(o.e->model().master.eq3[2] == 0.0f);
    REQUIRE_FALSE(o.e->model().tracks[0].fx[0].has_value());
    o.render(256);
    REQUIRE(o.e->rt().masterEq().gainDb(2) == 0.0f);
    REQUIRE(o.e->rt().mixer().limiter().ceiling() == std::pow(10.0f, -0.3f / 20.0f));
}

TEST_CASE("fx.set cùng loại giữ instance; khác loại / xoá → instance cũ bị huỷ trên main khi snapshot thu hồi", "[core][fx]") {
    Offline o;
    REQUIRE(o.ok(kFilter));
    const auto first = o.e->fxProcessor(0, 0);
    REQUIRE_FALSE(first.expired());
    o.render(2048);
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":0,"type":"filter","params":{"1":300}})")); // cùng loại
    REQUIRE(o.e->fxProcessor(0, 0).lock() == first.lock());
    REQUIRE(o.e->model().tracks[0].fx[0]->params[1] == 300.0f);
    REQUIRE(o.e->model().tracks[0].fx[0]->params[2] == 0.707f);   // không có trong params → mặc định

    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":0,"type":"delay"})"));   // khác loại → instance mới
    REQUIRE(o.e->fxProcessor(0, 0).lock() != first.lock());
    REQUIRE_FALSE(first.expired());   // RT còn đang fade nó
    o.renderRaw(4096);                // fade 20 ms xong, RT đã gửi Retire…
    REQUIRE_FALSE(first.expired());   // …nhưng chỉ main (pump) mới delete
    o.e->pump();
    REQUIRE(first.expired());

    const auto delay = o.e->fxProcessor(0, 0);
    REQUIRE(o.ok(R"({"op":"fx.remove","track":0,"index":0})"));
    REQUIRE(o.e->fxProcessor(0, 0).expired());
    o.render(4096);
    REQUIRE(delay.expired());
    REQUIRE(o.e->rt().fx(0).currentInstance(0) == 0);
}

TEST_CASE("FX trên clip đang phát: đổi/xoá không click; transport stop không cắt đuôi reverb", "[core][fx]") {
    Offline o;
    REQUIRE(o.ok(R"({"op":"clip.setAudio","track":0,"slot":0,"clipId":"s","file":")" LE_TEST_FIXTURES_DIR
                 R"(/clip_sine_4beats_120.wav","lengthBeats":4,"originalBpm":120,"warp":"repitch"})"));
    auto loaded = [&] { return o.e->model().clips[0][0].has_value() && o.e->model().clips[0][0]->audio != nullptr; };
    for (int i = 0; i < 400 && !loaded(); ++i) {   // job decode trên worker, continuation trên main (pump)
        juce::Thread::sleep(5);
        o.e->pump();
    }
    REQUIRE(loaded());
    REQUIRE(o.send(LE_CMD_CLIP_LAUNCH, 0, 0, 0));
    std::vector<float> all;
    auto add = [&](const std::vector<float>& x) { all.insert(all.end(), x.begin(), x.end()); };
    add(o.render(4800));
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":0,"type":"reverb","params":{"3":0.5}})"));
    add(o.render(4800));
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":1,"type":"filter","params":{"1":400}})"));
    add(o.render(4800));
    REQUIRE(o.send(LE_CMD_FX_BYPASS, 0, 1, 1));
    add(o.render(4800));
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":1,"type":"eq3","params":{"0":6}})"));
    add(o.render(4800));
    REQUIRE(o.ok(R"({"op":"fx.remove","track":0,"index":1})"));
    add(o.render(4800));
    float jump = 0.0f;
    for (size_t i = 1; i < all.size(); ++i) jump = std::max(jump, std::fabs(all[i] - all[i - 1]));
    REQUIRE(jump < 0.06f);   // sine 500 Hz 0.354 sau pan: bước ≈ 0.023; click sẽ lớn hơn nhiều

    o.send(LE_CMD_TRANSPORT_STOP, -1, -1, 0);
    const auto tail = o.render(9600);
    float late = 0.0f;   // 100..200 ms sau stop: clip đã fade 5 ms, chỉ còn đuôi reverb
    for (size_t i = 4800; i < tail.size(); ++i) late = std::max(late, std::fabs(tail[i]));
    REQUIRE(late > 1e-4f);
}

TEST_CASE("FX: sample rate đổi khi restart device → processor được prepare lại", "[core][fx]") {
    Offline o(44100.0);
    REQUIRE(o.ok(R"({"op":"fx.set","track":0,"index":0,"type":"delay","params":{"0":4}})"));
    o.render(1024);
    le::io::DeviceConfig dc;
    dc.sampleRate = 48000.0;
    dc.bufferSize = 128;
    dc.numOutputs = 2;
    REQUIRE(o.dev->restart(dc) == LE_OK);   // RtEngine::prepare(48k) → prepare lại mọi FX trong snapshot
    REQUIRE(o.e->rt().sampleRate() == 48000.0);
    o.render(4096);   // không crash / ASan: delay line đã cấp phát lại theo 48k
    REQUIRE(o.e->fxProcessor(0, 0).lock() != nullptr);
}
