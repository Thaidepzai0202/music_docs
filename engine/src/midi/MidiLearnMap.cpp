#include "midi/MidiLearnMap.h"

#include "midi/MidiInputRouter.h"

namespace le::midi {

bool learnKeyFor(const MidiInputEvent& e, LearnKey& out) noexcept [[clang::nonblocking]] {
    const ParsedMidi p = parseMidi(e.status, e.data1, e.data2);
    if (p.kind == ParsedMidi::Kind::NoteOn || p.kind == ParsedMidi::Kind::NoteOff) out.kind = LearnKind::Note;
    else if (p.kind == ParsedMidi::Kind::ControlChange) out.kind = LearnKind::CC;
    else return false;
    out.source = e.source;
    out.channel = p.channel;
    out.number = p.number;
    return true;
}

// Vị trí đầu tiên có khoá ≥ key (tìm nhị phân trên mảng đã sắp).
int MidiLearnMap::lowerBound(uint64_t key) const noexcept [[clang::nonblocking]] {
    int lo = 0, hi = size_;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (entries_[static_cast<size_t>(mid)].key < key) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

bool MidiLearnMap::set(const LearnKey& key, const LearnTarget& target) noexcept {
    const uint64_t k = key.packed();
    const int i = lowerBound(k);
    if (i < size_ && entries_[static_cast<size_t>(i)].key == k) {   // thay mapping cũ
        entries_[static_cast<size_t>(i)].target = target;
        return true;
    }
    if (size_ >= kCapacity) return false;
    for (int j = size_; j > i; --j) entries_[static_cast<size_t>(j)] = entries_[static_cast<size_t>(j - 1)];
    entries_[static_cast<size_t>(i)] = {k, target};
    ++size_;
    return true;
}

bool MidiLearnMap::remove(const LearnKey& key) noexcept {
    const uint64_t k = key.packed();
    const int i = lowerBound(k);
    if (i >= size_ || entries_[static_cast<size_t>(i)].key != k) return false;
    for (int j = i; j + 1 < size_; ++j) entries_[static_cast<size_t>(j)] = entries_[static_cast<size_t>(j + 1)];
    --size_;
    return true;
}

const LearnTarget* MidiLearnMap::find(const LearnKey& key) const noexcept [[clang::nonblocking]] {
    const uint64_t k = key.packed();
    const int i = lowerBound(k);
    return (i < size_ && entries_[static_cast<size_t>(i)].key == k) ? &entries_[static_cast<size_t>(i)].target : nullptr;
}

} // namespace le::midi
