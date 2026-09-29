#include "render/PeakBuilder.h"

#include "le/engine_api.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace le::render {

static_assert(std::endian::native == std::endian::little, "định dạng .peaks giả định máy little-endian");

namespace {
constexpr char     kMagic[4] = {'L', 'E', 'P', 'K'};
constexpr uint32_t kVersion = 1;
constexpr uint32_t kHeaderBytes = 64;

int64_t pointsFor(int64_t frames, int spp) { return frames <= 0 ? 0 : (frames + spp - 1) / spp; }

bool fail(std::string* error, const std::string& msg) {
    if (error != nullptr) *error = msg;
    return false;
}

template <class T>
void put(std::vector<char>& buf, size_t offset, T v) {
    std::memcpy(buf.data() + offset, &v, sizeof(T));
}
template <class T>
T get(const char* p, size_t offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof(T));
    return v;
}
} // namespace

int32_t Peaks::copyTo(int level, float* out, int32_t maxPairs, int64_t firstPoint) const noexcept {
    if (level < 0 || level >= kLevels || out == nullptr || maxPairs < 0 || firstPoint < 0) return LE_ERR_INVALID_ARG;
    const int64_t avail = std::max<int64_t>(0, numPoints(level) - firstPoint);
    const auto n = static_cast<int32_t>(std::min<int64_t>(avail, maxPairs));
    if (n > 0) std::memcpy(out, levels[static_cast<size_t>(level)].data() + 2 * firstPoint, sizeof(float) * 2 * static_cast<size_t>(n));
    return n;
}

// [worker]
Peaks buildPeaks(const dsp::AudioData& data, const std::atomic<bool>* cancel) {
    Peaks p;
    const int64_t nf = data.numFrames();
    const int nc = data.numChannels();

    // Mức 0: trực tiếp từ dữ liệu, mỗi 256 frame (gộp mọi kênh)
    const int spp0 = Peaks::kSamplesPerPoint[0];
    std::vector<float>& l0 = p.levels[0];
    l0.resize(static_cast<size_t>(2 * pointsFor(nf, spp0)));
    for (int64_t pt = 0, start = 0; start < nf; ++pt, start += spp0) {
        if ((pt & 1023) == 0 && cancel != nullptr && cancel->load(std::memory_order_relaxed)) return Peaks{};
        const int64_t end = std::min<int64_t>(nf, start + spp0);
        float mn = std::numeric_limits<float>::max(), mx = std::numeric_limits<float>::lowest();
        for (int c = 0; c < nc; ++c) {
            const float* x = data.channel(c);
            for (int64_t i = start; i < end; ++i) {
                mn = std::min(mn, x[i]);
                mx = std::max(mx, x[i]);
            }
        }
        l0[static_cast<size_t>(2 * pt)] = mn;
        l0[static_cast<size_t>(2 * pt + 1)] = mx;
    }

    // Mức 1, 2: gộp mỗi 8 điểm của mức dưới (2048 = 8·256, 16384 = 8·2048)
    for (int lv = 1; lv < Peaks::kLevels; ++lv) {
        const int ratio = Peaks::kSamplesPerPoint[static_cast<size_t>(lv)] / Peaks::kSamplesPerPoint[static_cast<size_t>(lv - 1)];
        const std::vector<float>& lower = p.levels[static_cast<size_t>(lv - 1)];
        std::vector<float>& cur = p.levels[static_cast<size_t>(lv)];
        const int64_t lowerPts = static_cast<int64_t>(lower.size() / 2);
        cur.resize(static_cast<size_t>(2 * pointsFor(nf, Peaks::kSamplesPerPoint[static_cast<size_t>(lv)])));
        for (int64_t pt = 0; 2 * pt < static_cast<int64_t>(cur.size()); ++pt) {
            float mn = std::numeric_limits<float>::max(), mx = std::numeric_limits<float>::lowest();
            for (int64_t k = pt * ratio; k < std::min<int64_t>(lowerPts, (pt + 1) * ratio); ++k) {
                mn = std::min(mn, lower[static_cast<size_t>(2 * k)]);
                mx = std::max(mx, lower[static_cast<size_t>(2 * k + 1)]);
            }
            cur[static_cast<size_t>(2 * pt)] = mn;
            cur[static_cast<size_t>(2 * pt + 1)] = mx;
        }
    }
    p.numChannels = nc;
    p.sampleRate = data.sampleRate();
    p.numFrames = nf;
    return p;
}

// [worker]
bool writePeaksFile(const std::string& path, const Peaks& peaks, std::string* error) {
    std::vector<char> header(kHeaderBytes, 0);
    std::memcpy(header.data(), kMagic, 4);
    put<uint32_t>(header, 4, kVersion);
    put<uint32_t>(header, 8, kHeaderBytes);
    put<uint32_t>(header, 12, static_cast<uint32_t>(peaks.numChannels));
    put<double>(header, 16, peaks.sampleRate);
    put<int64_t>(header, 24, peaks.numFrames);
    put<uint32_t>(header, 32, static_cast<uint32_t>(Peaks::kLevels));
    for (int lv = 0; lv < Peaks::kLevels; ++lv) {
        put<uint32_t>(header, 36 + 4 * static_cast<size_t>(lv), static_cast<uint32_t>(Peaks::kSamplesPerPoint[static_cast<size_t>(lv)]));
        put<uint32_t>(header, 48 + 4 * static_cast<size_t>(lv), static_cast<uint32_t>(peaks.numPoints(lv)));
    }

    std::error_code ec;
    const std::filesystem::path target(path);
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return fail(error, "không mở được để ghi: " + tmp);
        f.write(header.data(), static_cast<std::streamsize>(header.size()));
        for (const auto& lv : peaks.levels)
            f.write(reinterpret_cast<const char*>(lv.data()), static_cast<std::streamsize>(lv.size() * sizeof(float)));
        if (!f.flush()) return fail(error, "ghi thất bại: " + tmp);
    }
    std::filesystem::rename(tmp, target, ec);   // thay thế nguyên tử trên cùng ổ đĩa
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return fail(error, "không đổi tên được " + tmp + " → " + path);
    }
    return true;
}

// [worker]
bool readPeaksFile(const std::string& path, Peaks& out, std::string* error, int64_t expectFrames,
                   double expectSampleRate) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return fail(error, "không có file: " + path);
    const auto size = static_cast<int64_t>(f.tellg());
    if (size < static_cast<int64_t>(kHeaderBytes)) return fail(error, "file quá ngắn");
    f.seekg(0);
    char h[kHeaderBytes];
    f.read(h, kHeaderBytes);
    if (!f || std::memcmp(h, kMagic, 4) != 0) return fail(error, "sai magic");
    if (get<uint32_t>(h, 4) != kVersion) return fail(error, "khác version");
    if (get<uint32_t>(h, 8) != kHeaderBytes) return fail(error, "sai headerBytes");
    if (get<uint32_t>(h, 32) != static_cast<uint32_t>(Peaks::kLevels)) return fail(error, "sai số mức");

    Peaks p;
    p.numChannels = static_cast<int>(get<uint32_t>(h, 12));
    p.sampleRate = get<double>(h, 16);
    p.numFrames = get<int64_t>(h, 24);
    if (p.numFrames < 0 || p.numChannels < 1 || p.numChannels > dsp::AudioData::kMaxChannels || !(p.sampleRate > 0.0))
        return fail(error, "header không hợp lệ");
    if (expectFrames >= 0 && p.numFrames != expectFrames) return fail(error, "cache không khớp nguồn (số frame)");
    if (expectSampleRate > 0.0 && std::fabs(p.sampleRate - expectSampleRate) > 1e-6) return fail(error, "cache không khớp nguồn (sample rate)");

    int64_t total = 0;
    std::array<int64_t, Peaks::kLevels> pts{};
    for (int lv = 0; lv < Peaks::kLevels; ++lv) {
        const auto spp = static_cast<int>(get<uint32_t>(h, 36 + 4 * static_cast<size_t>(lv)));
        if (spp != Peaks::kSamplesPerPoint[static_cast<size_t>(lv)]) return fail(error, "sai samplesPerPoint");
        pts[static_cast<size_t>(lv)] = get<uint32_t>(h, 48 + 4 * static_cast<size_t>(lv));
        if (pts[static_cast<size_t>(lv)] != pointsFor(p.numFrames, spp)) return fail(error, "sai numPoints");
        total += pts[static_cast<size_t>(lv)];
    }
    if (size != static_cast<int64_t>(kHeaderBytes) + 8 * total) return fail(error, "kích thước file không khớp header");

    for (int lv = 0; lv < Peaks::kLevels; ++lv) {
        auto& v = p.levels[static_cast<size_t>(lv)];
        v.resize(static_cast<size_t>(2 * pts[static_cast<size_t>(lv)]));
        f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(v.size() * sizeof(float)));
        if (!f) return fail(error, "đọc dữ liệu thất bại");
    }
    out = std::move(p);
    return true;
}

} // namespace le::render
