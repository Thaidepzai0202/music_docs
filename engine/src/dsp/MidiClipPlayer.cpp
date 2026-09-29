#include "dsp/MidiClipPlayer.h"

#include <algorithm>
#include <cmath>

namespace le::dsp {

// [RT] Sắp chèn (ổn định): danh sách mỗi segment rất ngắn. Khoá = (offset, off trước on).
void MidiEventList::sort() noexcept [[clang::nonblocking]] {
    auto key = [](const MidiEvent& e) noexcept [[clang::nonblocking]] {
        return static_cast<int64_t>(e.offset) * 2 + (e.on ? 1 : 0);
    };
    for (int i = 1; i < count_; ++i) {
        const MidiEvent e = events_[static_cast<size_t>(i)];
        const int64_t k = key(e);
        int j = i - 1;
        while (j >= 0 && key(events_[static_cast<size_t>(j)]) > k) {
            events_[static_cast<size_t>(j + 1)] = events_[static_cast<size_t>(j)];
            --j;
        }
        events_[static_cast<size_t>(j + 1)] = e;
    }
}

// [main]
void MidiClipPlayer::prepare(double /*sampleRate*/, int /*maxBlock*/) {
    clip_ = nullptr;
    generation_ = 0;
    playing_ = false;
    needSeek_ = false;
    loop_ = 0;
    next_ = 0;
    offBeat_.fill(-1.0);
}

// Giống Transport::sampleAtBeat: sample đầu tiên có beat ≥ `beat`, tính từ đầu segment.
int32_t MidiClipPlayer::offsetOf(double beat, double segStartBeat, double spb) noexcept [[clang::nonblocking]] {
    const double x = std::ceil((beat - segStartBeat) * spb - 1e-6);
    return static_cast<int32_t>(std::clamp(x, -1.0e9, 1.0e9));
}

// [RT] Sau khi đổi clip lúc đang phát (xem setClip): đặt con trỏ "nốt kế tiếp" vào vị trí `beat` của clip MỚI và
// đối chiếu các nốt đang giữ. Một lượt O(số nốt tới pos + 128), mảng cố định trên stack → không cấp phát.
void MidiClipPlayer::seek(double beat, MidiEventList& out) noexcept [[clang::nonblocking]] {
    needSeek_ = false;
    loop_ = 0;
    next_ = 0;
    const bool valid = clip_ != nullptr && clip_->lengthBeats >= 1e-3;
    const double len = valid ? clip_->lengthBeats : 0.0;
    const double rel = beat - launchBeat_;
    double pos = 0.0;   // vị trí trong vòng hiện tại (toạ độ clip)
    if (valid && rel > 0.0) {
        loop_ = static_cast<int64_t>(std::floor(rel / len));
        pos = rel - static_cast<double>(loop_) * len;
    }
    std::array<const MidiNote*, 128> cover{};   // nốt bao trùm pos theo cao độ (nốt bắt đầu muộn nhất thắng)
    if (valid) {
        const auto& notes = clip_->notes;
        for (const MidiNote& nt : notes) {
            if (nt.startBeat > pos + 1e-9) break;   // notes sắp theo startBeat
            if (nt.pitch < 128 && nt.startBeat >= 0.0 && pos < nt.startBeat + nt.lengthBeats - 1e-9) cover[nt.pitch] = &nt;
        }
        const double c = pos - 1e-9;   // nốt ĐÚNG tại pos chưa phát (thuộc segment này) → vẫn phát, đúng 1 lần
        next_ = static_cast<size_t>(std::lower_bound(notes.begin(), notes.end(), c,
                                                     [](const MidiNote& n, double v) noexcept [[clang::nonblocking]] {
                                                         return n.startBeat < v;
                                                     }) - notes.begin());
    }
    const double loopStart = launchBeat_ + static_cast<double>(loop_) * len, loopEnd = loopStart + len;
    for (int p = 0; p < 128; ++p) {
        double& ob = offBeat_[static_cast<size_t>(p)];
        if (ob < 0.0) continue;
        if (const MidiNote* nt = cover[static_cast<size_t>(p)]) {
            ob = std::min(loopStart + nt->startBeat + nt->lengthBeats, loopEnd);   // > beat vì pos < start + length
        } else {
            out.push({0, static_cast<uint8_t>(p), 0, false});   // không còn trong nội dung mới → tắt ngay (có release)
            ob = -1.0;
        }
    }
}

// [RT]
void MidiClipPlayer::setClip(const MidiClip* clip, uint32_t generation) noexcept [[clang::nonblocking]] {
    generation_ = generation;
    if (clip == clip_) return;
    clip_ = clip;
    if (playing_) {   // phát tiếp cùng pha: tìm lại vị trí ở segment kế tiếp
        needSeek_ = true;
    }
}

// [RT]
void MidiClipPlayer::start(double launchBeat) noexcept [[clang::nonblocking]] {
    if (clip_ == nullptr) return;
    launchBeat_ = launchBeat;
    playing_ = true;
    needSeek_ = false;
    loop_ = 0;
    next_ = 0;
}

// [RT]
void MidiClipPlayer::stop(MidiEventList& out, int offset) noexcept [[clang::nonblocking]] {
    for (int p = 0; p < 128; ++p) {
        double& ob = offBeat_[static_cast<size_t>(p)];
        if (ob < 0.0) continue;
        out.push({offset, static_cast<uint8_t>(p), 0, false});
        ob = -1.0;
    }
    playing_ = false;
    needSeek_ = false;
}

// [RT]
void MidiClipPlayer::process(double segStartBeat, int n, double spb, MidiEventList& out) noexcept
    [[clang::nonblocking]] {
    if (n <= 0 || spb <= 0.0) return;
    if (clip_ == nullptr && playing_) {    // clip bị gỡ → tắt mọi nốt ngay đầu segment
        stop(out, 0);
        out.sort();
        return;
    }
    if (playing_ && needSeek_) seek(segStartBeat, out);

    const double minLen = 1.0 / spb;       // nốt dài tối thiểu 1 sample (tránh on/off cùng offset)
    // Độ dài vòng quá nhỏ (dữ liệu hỏng) sẽ làm vòng lặp dưới đây chạy rất lâu trên RT → không phát.
    const bool canPlay = playing_ && clip_ != nullptr && clip_->lengthBeats >= 1e-3 && !clip_->notes.empty();
    const double len = canPlay ? clip_->lengthBeats : 0.0;

    // Phát các note-off đến hạn có offset ≤ limit (đúng thứ tự thời gian so với note-on kế tiếp).
    auto emitOffsUpTo = [&](int32_t limit) noexcept [[clang::nonblocking]] {
        for (int p = 0; p < 128; ++p) {
            double& ob = offBeat_[static_cast<size_t>(p)];
            if (ob < 0.0) continue;
            const int32_t o = offsetOf(ob, segStartBeat, spb);
            if (o > limit) continue;
            out.push({std::max(0, o), static_cast<uint8_t>(p), 0, false});
            ob = -1.0;
        }
    };

    for (;;) {
        const MidiNote* note = nullptr;
        double abs = 0.0;
        int32_t onOffset = n;
        if (canPlay) {
            const auto& notes = clip_->notes;
            // Bỏ qua nốt ngoài [0, len) (dữ liệu hỏng), sang vòng kế tiếp khi hết danh sách
            for (int guard = 0; guard < 2 && note == nullptr; ) {
                if (next_ >= notes.size()) { ++loop_; next_ = 0; ++guard; continue; }
                const MidiNote& cand = notes[next_];
                if (cand.startBeat < 0.0 || cand.startBeat >= len || cand.pitch > 127) { ++next_; continue; }
                note = &cand;
            }
            if (note != nullptr) {
                abs = launchBeat_ + static_cast<double>(loop_) * len + note->startBeat;
                const int32_t o = offsetOf(abs, segStartBeat, spb);
                if (o < n) onOffset = std::max(0, o);
                else note = nullptr;       // nốt kế tiếp thuộc segment sau
            }
        }

        emitOffsUpTo(note != nullptr ? onOffset : n - 1);
        if (note == nullptr) break;

        double& ob = offBeat_[note->pitch];
        if (ob >= 0.0) {                   // bấm lại cao độ đang giữ → tắt nốt cũ trước
            out.push({onOffset, note->pitch, 0, false});
        }
        out.push({onOffset, note->pitch, std::clamp<uint8_t>(note->velocity, 1, 127), true});
        const double loopEnd = launchBeat_ + static_cast<double>(loop_ + 1) * len;   // cắt tại điểm loop
        ob = std::max(std::min(abs + note->lengthBeats, loopEnd), abs + minLen);
        ++next_;
    }
    out.sort();
}

int MidiClipPlayer::heldNotes() const noexcept [[clang::nonblocking]] {
    int c = 0;
    for (double ob : offBeat_) c += ob >= 0.0 ? 1 : 0;
    return c;
}

float MidiClipPlayer::progress(double beat) const noexcept [[clang::nonblocking]] {
    if (!playing_ || clip_ == nullptr || clip_->lengthBeats <= 0.0) return 0.0f;
    const double rel = beat - launchBeat_;
    return rel <= 0.0 ? 0.0f : static_cast<float>(std::fmod(rel, clip_->lengthBeats) / clip_->lengthBeats);
}

} // namespace le::dsp
