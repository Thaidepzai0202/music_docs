#pragma once
// Các kênh giữa audio thread và phần còn lại (03 §2: SPSC queue, atomic, StatePublisher).
// rigtorp::SPSCQueue: 1 thread push, 1 thread pop, không lock. Bộ nhớ cấp phát 1 lần trong constructor
// (trên main), sau đó try_push / front / pop không cấp phát.
#include <cstdint>

#include <rigtorp/SPSCQueue.h>

#include "le/engine_api.h"

namespace le::core {

constexpr std::size_t kRtCommandCapacity = 1024;
constexpr std::size_t kRtToNrtCapacity = 1024;

// UI (main) → RT
using CommandQueue = rigtorp::SPSCQueue<LeCommand>;

// RT → main. P1-06 sẽ thêm Retire{snapshot}.
struct RtMessage {
    enum Kind : std::int32_t { Event = 1 };
    Kind kind = Event;
    std::int32_t type = 0;   // LeEventType
    std::int32_t a = 0;
    std::int32_t b = 0;
    double value = 0.0;
};
using RtToNrtQueue = rigtorp::SPSCQueue<RtMessage>;

} // namespace le::core
