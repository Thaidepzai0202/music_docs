#include "io/CafWriter.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

namespace le::io {

namespace {
void be16(std::vector<std::uint8_t>& b, std::uint16_t v) { b.push_back((std::uint8_t) (v >> 8)); b.push_back((std::uint8_t) v); }
void be32(std::vector<std::uint8_t>& b, std::uint32_t v) { for (int s = 24; s >= 0; s -= 8) b.push_back((std::uint8_t) (v >> s)); }
void be64(std::vector<std::uint8_t>& b, std::uint64_t v) { for (int s = 56; s >= 0; s -= 8) b.push_back((std::uint8_t) (v >> s)); }
void fourcc(std::vector<std::uint8_t>& b, const char* c) { b.insert(b.end(), c, c + 4); }
} // namespace

bool writeCafFloat32(const std::string& absolutePath, const dsp::AudioData& data, std::string* error) {
    const juce::File f(juce::String::fromUTF8(absolutePath.c_str()));
    if (!f.getParentDirectory().createDirectory()) {
        if (error) *error = "không tạo được thư mục cho " + absolutePath;
        return false;
    }
    const int ch = data.numChannels();
    const auto frames = (std::uint64_t) data.numFrames();

    std::vector<std::uint8_t> head;
    fourcc(head, "caff");
    be16(head, 1);   // version
    be16(head, 0);   // flags
    fourcc(head, "desc");
    be64(head, 32);
    std::uint64_t srBits = 0;
    const double sr = data.sampleRate();
    std::memcpy(&srBits, &sr, 8);
    be64(head, srBits);
    fourcc(head, "lpcm");
    be32(head, 1u | 2u);                       // kCAFLinearPCMFormatFlagIsFloat | IsLittleEndian
    be32(head, (std::uint32_t) (4 * ch));      // bytesPerPacket
    be32(head, 1);                             // framesPerPacket
    be32(head, (std::uint32_t) ch);            // channelsPerFrame
    be32(head, 32);                            // bitsPerChannel
    fourcc(head, "data");
    be64(head, 4 + frames * (std::uint64_t) ch * 4);
    be32(head, 0);                             // edit count

    // P4-19: ghi "<path>.tmp" rồi mới đổi tên → app bị kill giữa chừng không để lại .caf dở mang tên thật
    // (project.open dọn .tmp còn sót). Đổi tên trong cùng thư mục là nguyên tử.
    const juce::File tmp = f.getSiblingFile(f.getFileName() + ".tmp");
    tmp.deleteFile();
    auto os = std::make_unique<juce::FileOutputStream>(tmp);
    if (!os->openedOk()) {
        if (error) *error = "không mở được " + absolutePath;
        return false;
    }
    bool ok = os->write(head.data(), head.size());
    std::vector<float> chunk;
    constexpr std::uint64_t kChunk = 16384;
    for (std::uint64_t pos = 0; ok && pos < frames; pos += kChunk) {
        const auto n = std::min(kChunk, frames - pos);
        chunk.resize((size_t) (n * (std::uint64_t) ch));
        for (std::uint64_t i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c) chunk[(size_t) (i * (std::uint64_t) ch + (std::uint64_t) c)] = data.channel(c)[pos + i];
        ok = os->write(chunk.data(), chunk.size() * sizeof(float));   // máy Apple là little-endian
    }
    os->flush();
    ok = ok && !os->getStatus().failed();
    os.reset();   // đóng file trước khi đổi tên
    if (!ok || !tmp.moveFileTo(f)) {
        tmp.deleteFile();
        if (error) *error = "ghi file lỗi (đầy đĩa?): " + absolutePath;
        return false;
    }
    return true;
}

} // namespace le::io
