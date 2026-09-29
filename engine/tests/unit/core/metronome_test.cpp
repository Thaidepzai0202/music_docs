// P1-10 Metronome: click đúng sample k·samplesPerBeat, phách mạnh khác phách thường, các chế độ.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "core/BlockSplitter.h"
#include "core/Metronome.h"

using namespace le::core;

namespace {
// Render n sample qua metronome theo block `block` (không có ranh giới khác). Trả kênh L.
std::vector<float> render(Metronome& m, Transport& tr, int total, int block) {
    std::vector<float> L((size_t) total, 0.0f), R((size_t) total, 0.0f);
    BlockSplitter sp;
    Segment segs[BlockSplitter::kMaxSegments];
    for (int done = 0; done < total;) {
        const int n = std::min(block, total - done);
        sp.begin(n);
        const int ns = sp.split(tr, tr.samplePos(), segs);
        for (int i = 0; i < ns; ++i) m.render(tr, segs[i], L.data() + done, R.data() + done);
        tr.advance(n);
        done += n;
    }
    return L;
}

std::vector<int> onsets(const std::vector<float>& x, int gap = 64) {
    std::vector<int> out;
    int last = -gap - 1;
    for (int i = 0; i < (int) x.size(); ++i) {
        if (std::fabs(x[(size_t) i]) <= 1e-6f) continue;
        if (i - last > gap) out.push_back(i);
        last = i;
    }
    return out;
}
} // namespace

TEST_CASE("Metronome: click tại đúng k × 24000, như nhau ở mọi block size", "[core][metronome]") {
    for (int block : {1, 64, 128, 256, 1024, 333}) {
        INFO("block " << block);
        Metronome m;
        m.prepare(48000.0);
        m.setMode(Metronome::On, 1.0f);
        Transport tr;
        tr.prepare(48000.0);
        tr.play();
        const auto L = render(m, tr, 8 * 24000, block);
        const auto on = onsets(L);
        REQUIRE(on.size() == 8);
        for (int k = 0; k < 8; ++k) REQUIRE(on[(size_t) k] == k * 24000);
    }
}

TEST_CASE("Metronome: phách mạnh (1.5 kHz, to hơn) ở beat 0 của mỗi bar", "[core][metronome]") {
    Metronome m;
    m.prepare(48000.0);
    REQUIRE(m.clickLength() == 1440);   // 30 ms
    REQUIRE(m.accentClick()[0] != 0.0f);   // sample đầu khác 0 → onset đúng sample
    m.setMode(Metronome::On, 1.0f);
    Transport tr;
    tr.prepare(48000.0);
    tr.play();
    const auto L = render(m, tr, 5 * 24000, 128);
    auto peak = [&](int start) {
        float p = 0;
        for (int i = start; i < start + 1440; ++i) p = std::max(p, std::fabs(L[(size_t) i]));
        return p;
    };
    REQUIRE(peak(0) > peak(24000) * 1.4f);          // bar 1 beat 0 (accent) vs beat 1
    REQUIRE(peak(4 * 24000) == peak(0));            // bar 2 beat 0 cũng là accent
    REQUIRE(std::fabs(L[1440]) == 0.0f);            // hết 30 ms là im
}

TEST_CASE("Metronome: Off / RecordOnly / count-in / transport dừng", "[core][metronome]") {
    Transport tr;
    tr.prepare(48000.0);
    tr.play();
    Metronome m;
    m.prepare(48000.0);

    m.setMode(Metronome::Off, 1.0f);
    REQUIRE(onsets(render(m, tr, 48000, 128)).empty());

    m.setMode(Metronome::RecordOnly, 1.0f);
    REQUIRE_FALSE(m.audible());
    REQUIRE(onsets(render(m, tr, 48000, 128)).empty());
    m.setRecording(true);
    REQUIRE(m.audible());
    m.setRecording(false);
    m.setCountingIn(true);   // count-in luôn kêu, kể cả khi Off
    m.setMode(Metronome::Off, 1.0f);
    REQUIRE(m.audible());
    REQUIRE(onsets(render(m, tr, 48000, 128)).size() == 2);

    Transport stopped;
    stopped.prepare(48000.0);
    m.setMode(Metronome::On, 1.0f);
    REQUIRE(onsets(render(m, stopped, 48000, 128)).empty());   // transport dừng → không có click mới
}
