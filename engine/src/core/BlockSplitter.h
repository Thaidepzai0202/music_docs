#pragma once
// BlockSplitter (P1-09, 04 §2.2) [RT]: chia 1 block thành các segment tại "ranh giới sự kiện"
// (quantize của lệnh đang chờ, điểm loop, nốt MIDI, bắt đầu/kết thúc thu). Trong 1 segment không có
// sự kiện nào, nên mỗi subsystem render cả segment một lượt.
// - Ranh giới truyền vào là frame offset trong block (người gọi tính từ sample tuyệt đối của Transport,
//   nên cùng một sự kiện luôn rơi đúng một sample bất kể block dài bao nhiêu).
// - Offset ≤ 0 hoặc ≥ numFrames bị bỏ (sự kiện ở frame 0 áp dụng ở đầu segment đầu tiên).
// - Hai ranh giới cách nhau ≤ 1 sample được gộp về ranh giới sớm hơn.
// - Tối đa 16 segment: thừa thì gộp cặp ranh giới gần nhau nhất.
// Mọi mảng có kích thước cố định → không cấp phát.
#include <cstdint>

#include "core/Transport.h"

namespace le::core {

struct Segment {
    int startFrame = 0;          // offset trong block
    int numFrames = 0;
    std::int64_t startSample = 0;  // sample tuyệt đối của Transport (kể từ Play)
    double startBeat = 0.0;
    double endBeat = 0.0;
};

class BlockSplitter {
public:
    static constexpr int kMaxSegments = 16;
    static constexpr int kMaxBoundaries = 64;

    void begin(int numFrames) noexcept [[clang::nonblocking]] {
        n_ = numFrames;
        count_ = 0;
    }

    void add(int frame) noexcept [[clang::nonblocking]] {
        if (frame > 0 && frame < n_ && count_ < kMaxBoundaries) b_[count_++] = frame;
    }

    // Ghi segment vào `out` (cần ≥ kMaxSegments phần tử), trả số segment (≥ 1 nếu numFrames > 0).
    int split(const Transport& tr, std::int64_t blockStartSample, Segment* out) noexcept [[clang::nonblocking]] {
        if (n_ <= 0) return 0;
        // Sắp xếp chèn (≤ 64 phần tử, thường 0–3)
        for (int i = 1; i < count_; ++i) {
            const int v = b_[i];
            int j = i - 1;
            while (j >= 0 && b_[j] > v) { b_[j + 1] = b_[j]; --j; }
            b_[j + 1] = v;
        }
        // Gộp ranh giới trùng hoặc cách ≤ 1 sample (giữ cái sớm hơn)
        int m = 0;
        for (int i = 0; i < count_; ++i)
            if (m == 0 || b_[i] - b_[m - 1] > 1) b_[m++] = b_[i];
        // Quá 16 segment → bỏ ranh giới có khoảng cách tới ranh giới trước nhỏ nhất
        while (m + 1 > kMaxSegments) {
            int best = 0, bestGap = b_[0];
            for (int i = 1; i < m; ++i)
                if (b_[i] - b_[i - 1] < bestGap) { bestGap = b_[i] - b_[i - 1]; best = i; }
            for (int i = best; i + 1 < m; ++i) b_[i] = b_[i + 1];
            --m;
        }

        int prev = 0, segs = 0;
        auto emit = [&](int end) noexcept [[clang::nonblocking]] {
            Segment& s = out[segs++];
            s.startFrame = prev;
            s.numFrames = end - prev;
            s.startSample = blockStartSample + prev;
            s.startBeat = tr.playing() ? tr.beatAt(s.startSample) : tr.beatNow();
            s.endBeat = tr.playing() ? tr.beatAt(s.startSample + s.numFrames) : s.startBeat;
            prev = end;
        };
        for (int i = 0; i < m; ++i) emit(b_[i]);
        emit(n_);
        count_ = 0;
        return segs;
    }

private:
    int n_ = 0;
    int count_ = 0;
    int b_[kMaxBoundaries] = {};
};

} // namespace le::core
