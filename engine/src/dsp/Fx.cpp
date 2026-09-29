// Bảng tham số + factory cho Processor (dsp/Processor.h). Id và dải khớp 04 §9.
#include "dsp/Processor.h"

#include "dsp/fx/CompressorFx.h"
#include "dsp/fx/DelayFx.h"
#include "dsp/fx/Eq3Fx.h"
#include "dsp/fx/FilterFx.h"
#include "dsp/fx/ReverbFx.h"

#include <algorithm>
#include <cmath>
#include <array>

namespace le::dsp {

namespace {
//                                  id  name         min      max      default  log    int
constexpr std::array<ParamInfo, 3> kFilter = {{{0, "mode", 0.0f, 2.0f, 0.0f, false, true},
                                                {1, "cutoff", 20.0f, 20000.0f, 20000.0f, true, false},
                                                {2, "resonance", 0.5f, 10.0f, 0.707f, true, false}}};
constexpr std::array<ParamInfo, 4> kDelay = {{{0, "time", 0.0f, 10.0f, 4.0f, false, true},   // chỉ số DelayFx::kNoteBeats
                                               {1, "feedback", 0.0f, 0.95f, 0.35f, false, false},
                                               {2, "mix", 0.0f, 1.0f, 0.3f, false, false},
                                               {3, "pingpong", 0.0f, 1.0f, 0.0f, false, true}}};
constexpr std::array<ParamInfo, 4> kReverb = {{{0, "size", 0.0f, 1.0f, 0.5f, false, false},
                                                {1, "damping", 0.0f, 1.0f, 0.5f, false, false},
                                                {2, "width", 0.0f, 1.0f, 1.0f, false, false},
                                                {3, "mix", 0.0f, 1.0f, 0.25f, false, false}}};
constexpr std::array<ParamInfo, 3> kEq3 = {{{0, "low", -15.0f, 15.0f, 0.0f, false, false},
                                             {1, "mid", -15.0f, 15.0f, 0.0f, false, false},
                                             {2, "high", -15.0f, 15.0f, 0.0f, false, false}}};
constexpr std::array<ParamInfo, 5> kCompressor = {{{0, "threshold", -60.0f, 0.0f, -18.0f, false, false},
                                                    {1, "ratio", 1.0f, 20.0f, 4.0f, true, false},
                                                    {2, "attack", 0.1f, 100.0f, 10.0f, true, false},
                                                    {3, "release", 10.0f, 1000.0f, 100.0f, true, false},
                                                    {4, "makeup", 0.0f, 24.0f, 0.0f, false, false}}};
constexpr std::array<const char*, kNumFxTypes> kNames = {"filter", "delay", "reverb", "eq3", "comp"};   // tên chuẩn (05 fx.set)
} // namespace

std::span<const ParamInfo> paramInfo(FxType type) noexcept [[clang::nonblocking]] {
    switch (type) {
        case FxType::Filter: return kFilter;
        case FxType::Delay: return kDelay;
        case FxType::Reverb: return kReverb;
        case FxType::EQ3: return kEq3;
        case FxType::Compressor: return kCompressor;
    }
    return {};
}

const char* fxTypeName(FxType type) noexcept [[clang::nonblocking]] {
    const auto i = static_cast<int>(type);
    return (i >= 0 && i < kNumFxTypes) ? kNames[static_cast<size_t>(i)] : "";
}

bool fxTypeFromName(std::string_view name, FxType& out) noexcept {
    for (int i = 0; i < kNumFxTypes; ++i)
        if (name == kNames[static_cast<size_t>(i)]) {
            out = static_cast<FxType>(i);
            return true;
        }
    if (name == "compressor") {   // bí danh (chỉ nhận vào; fxTypeName luôn trả "comp")
        out = FxType::Compressor;
        return true;
    }
    return false;
}

float clampParam(FxType type, int id, float v) noexcept [[clang::nonblocking]] {
    for (const ParamInfo& p : paramInfo(type))
        if (p.id == id) {
            if (std::isnan(v)) return p.defaultValue;   // NaN không được vào smoother (±Inf thì kẹp về biên)
            float x = std::clamp(v, p.min, p.max);
            if (p.integer) x = static_cast<float>(static_cast<int>(x + 0.5f));
            return x;
        }
    return v;
}

// [main]
std::unique_ptr<Processor> createProcessor(FxType type) {
    switch (type) {
        case FxType::Filter: return std::make_unique<FilterFx>();
        case FxType::Delay: return std::make_unique<DelayFx>();
        case FxType::Reverb: return std::make_unique<ReverbFx>();
        case FxType::EQ3: return std::make_unique<Eq3Fx>();
        case FxType::Compressor: return std::make_unique<CompressorFx>();
    }
    return nullptr;
}

} // namespace le::dsp
