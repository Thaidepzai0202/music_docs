#include "io/ExportWriter.h"

#include "io/ExportBackends.h"
#include "le/engine_api.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace le::io {

namespace {
bool fail(std::string* error, const std::string& msg) {
    if (error != nullptr) *error = msg;
    return false;
}

// ── WAV PCM 24-bit ─────────────────────────────────────────────────────────────
class Wav24Backend final : public ExportWriter::Backend {
public:
    Wav24Backend(std::ofstream f, int numChannels, uint32_t seed, double sampleRate)
        : f_(std::move(f)), nc_(numChannels), rng_(seed | 1u), sampleRate_(sampleRate) {}

    int32_t write(const float* const* ch, int n, int64_t& clipped, std::string* error) override {
        bytes_.resize(static_cast<size_t>(n) * static_cast<size_t>(nc_) * 3);
        uint8_t* p = bytes_.data();
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < nc_; ++c) {
                const float x = ch[c][i];
                // TPDF: tổng 2 nhiễu đều [−½, ½) LSB → tam giác [−1, 1) LSB, rồi làm tròn
                const double v = static_cast<double>(x) * 8388608.0 + uniform() + uniform();
                auto q = static_cast<int32_t>(std::lround(v));
                if (q > 8388607) { q = 8388607; ++clipped; }
                else if (q < -8388608) { q = -8388608; ++clipped; }
                const auto u = static_cast<uint32_t>(q);
                *p++ = static_cast<uint8_t>(u & 0xFF);
                *p++ = static_cast<uint8_t>((u >> 8) & 0xFF);
                *p++ = static_cast<uint8_t>((u >> 16) & 0xFF);
            }
        f_.write(reinterpret_cast<const char*>(bytes_.data()), static_cast<std::streamsize>(bytes_.size()));
        dataBytes_ += static_cast<uint64_t>(bytes_.size());
        if (!f_) {
            fail(error, "ghi WAV thất bại (đầy đĩa?)");
            return LE_ERR_DISK_FULL;
        }
        return LE_OK;
    }

    int32_t close(std::string* error) override {
        if (dataBytes_ % 2 == 1) f_.put('\0');                   // chunk RIFF phải chẵn byte
        writeHeader();
        f_.close();
        if (!f_) {
            fail(error, "đóng file WAV thất bại");
            return LE_ERR_DISK_FULL;
        }
        return LE_OK;
    }
    void closeQuietly() noexcept override { f_.close(); }

    // Header 44 byte (PCM, fmt 16 byte). Ghi lúc mở (kích thước 0) và ghi lại lúc đóng.
    void writeHeader() {
        const uint32_t data = static_cast<uint32_t>(std::min<uint64_t>(dataBytes_, 0xFFFFFFFFull - 44));
        const uint32_t riff = 36 + data + (data % 2);
        uint8_t h[44];
        auto u16 = [&](int at, uint16_t v) { h[at] = static_cast<uint8_t>(v); h[at + 1] = static_cast<uint8_t>(v >> 8); };
        auto u32 = [&](int at, uint32_t v) { for (int k = 0; k < 4; ++k) h[at + k] = static_cast<uint8_t>(v >> (8 * k)); };
        std::memcpy(h, "RIFF", 4);
        u32(4, riff);
        std::memcpy(h + 8, "WAVEfmt ", 8);
        u32(16, 16);
        u16(20, 1);                                                // PCM
        u16(22, static_cast<uint16_t>(nc_));
        u32(24, static_cast<uint32_t>(std::lround(sampleRate_)));
        u32(28, static_cast<uint32_t>(std::lround(sampleRate_)) * static_cast<uint32_t>(nc_) * 3);
        u16(32, static_cast<uint16_t>(nc_ * 3));
        u16(34, 24);
        std::memcpy(h + 36, "data", 4);
        u32(40, data);
        const auto pos = f_.tellp();
        f_.seekp(0);
        f_.write(reinterpret_cast<const char*>(h), 44);
        if (pos > 44) f_.seekp(pos);
    }

private:
    double uniform() {   // xorshift32 → [−0.5, 0.5)
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return static_cast<double>(rng_ >> 8) / 16777216.0 - 0.5;
    }

    std::ofstream f_;
    int nc_;
    uint32_t rng_;
    double sampleRate_;
    uint64_t dataBytes_ = 0;
    std::vector<uint8_t> bytes_;
};
} // namespace

std::unique_ptr<ExportWriter::Backend> makeWav24Backend(const std::string& tmpPath, double sampleRate, int numChannels,
                                                        uint32_t ditherSeed, int32_t& code, std::string* error) {
    std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
    if (!f) {
        fail(error, "không mở được để ghi: " + tmpPath);
        code = LE_ERR_FILE_NOT_FOUND;
        return nullptr;
    }
    auto b = std::make_unique<Wav24Backend>(std::move(f), numChannels, ditherSeed, sampleRate);
    b->writeHeader();
    code = LE_OK;
    return b;
}

const char* exportExtension(ExportFormat f) noexcept { return f == ExportFormat::M4aAac ? "m4a" : "wav"; }

ExportWriter::ExportWriter() = default;
ExportWriter::~ExportWriter() { abort(); }

int32_t ExportWriter::open(const std::string& path, double sampleRate, int numChannels, const ExportOptions& opt,
                           std::string* error) {
    abort();
    if (path.empty() || !(sampleRate > 0.0) || numChannels < 1 || numChannels > dsp::AudioData::kMaxChannels) {
        fail(error, "tham số export không hợp lệ");
        return LE_ERR_INVALID_ARG;
    }
    std::error_code ec;
    const std::filesystem::path target(path);
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), ec);
    path_ = path;
    tmpPath_ = path + ".tmp";
    frames_ = clipped_ = 0;
    bitrate_ = 0;
    int32_t code = LE_OK;
    if (opt.format == ExportFormat::Wav24) {
        backend_ = makeWav24Backend(tmpPath_, sampleRate, numChannels, opt.ditherSeed, code, error);
    } else {
        backend_ = makeAacBackend(tmpPath_, sampleRate, numChannels, opt.aacBitrate, bitrate_, code, error);
    }
    if (backend_ == nullptr) {
        std::filesystem::remove(tmpPath_, ec);
        return code != LE_OK ? code : LE_ERR_INTERNAL;
    }
    return LE_OK;
}

int32_t ExportWriter::write(const float* const* channels, int numFrames, std::string* error) {
    if (backend_ == nullptr) {
        fail(error, "chưa open");
        return LE_ERR_INVALID_ARG;
    }
    if (numFrames <= 0) return LE_OK;
    const int32_t code = backend_->write(channels, numFrames, clipped_, error);
    if (code != LE_OK) {
        abort();
        return code;
    }
    frames_ += numFrames;
    return LE_OK;
}

int32_t ExportWriter::finish(std::string* error) {
    if (backend_ == nullptr) {
        fail(error, "chưa open");
        return LE_ERR_INVALID_ARG;
    }
    const int32_t code = backend_->close(error);
    backend_.reset();
    std::error_code ec;
    if (code != LE_OK) {
        std::filesystem::remove(tmpPath_, ec);
        return code;
    }
    std::filesystem::rename(tmpPath_, path_, ec);   // thay thế nguyên tử trên cùng ổ đĩa
    if (ec) {
        std::filesystem::remove(tmpPath_, ec);
        fail(error, "không đổi tên được " + tmpPath_ + " → " + path_);
        return LE_ERR_DISK_FULL;
    }
    return LE_OK;
}

void ExportWriter::abort() noexcept {
    if (backend_ == nullptr) return;
    backend_->closeQuietly();
    backend_.reset();
    std::error_code ec;
    std::filesystem::remove(tmpPath_, ec);
}

ExportResult exportAudio(const std::string& path, const dsp::AudioData& data, const ExportOptions& opt) {
    ExportResult r;
    ExportWriter w;
    r.error = w.open(path, data.sampleRate(), data.numChannels(), opt, &r.message);
    if (r.error != LE_OK) return r;
    const float* ch[dsp::AudioData::kMaxChannels] = {data.channel(0), data.channelOrMono(1)};
    constexpr int64_t kChunk = 1 << 16;
    for (int64_t done = 0; done < data.numFrames(); done += kChunk) {
        const auto n = static_cast<int>(std::min(kChunk, data.numFrames() - done));
        const float* p[dsp::AudioData::kMaxChannels] = {ch[0] + done, ch[1] + done};
        r.error = w.write(p, n, &r.message);
        if (r.error != LE_OK) return r;
    }
    r.frames = w.framesWritten();
    r.clippedSamples = w.clippedSamples();
    r.bitrate = w.bitrate();
    r.error = w.finish(&r.message);
    r.ok = r.error == LE_OK;
    return r;
}

} // namespace le::io
