// P3-08 (04 §10): hook đổi tempo phía NRT. [main] Mọi hàm ở đây chạy trên main thread.
//   BPM đổi → [RT] mọi clip audio Re-Pitch ngay (tiếng liên tục, cao độ lệch tạm thời)
//          → [NRT] BPM đứng yên 300 ms → với mỗi clip warp = stretch có originalBpm ≠ BPM: job WarpRenderer (80)
//            trên worker (đọc cache <project>/cache/stretched/<clipId>@<bpm>.caf nếu có) → bản stretched vào model
//            → snapshot → AudioClipPlayer chuyển sang ở ranh giới bar kế tiếp, crossfade 10 ms (P3-10).
//   BPM lại đổi trong lúc render → huỷ job cũ; job xong mà BPM / audio đã khác → bỏ kết quả.
//   Mỗi clip giữ tối đa 1 bản stretched (bản cũ ở lại tới khi bản mới thay → quay lại BPM cũ vẫn dùng được).
//   Offline (test / sim): debounce tính theo audio đã render và chờ worker xong → kết quả tất định.
#include <cmath>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/OfflineDeviceIO.h"
#include "render/WarpRenderer.h"

namespace le::core {

bool Engine::offlineDevice() const {
    const auto* off = dynamic_cast<const io::OfflineDeviceIO*>(device_.get());
    return off != nullptr && off->isRunning();
}

double Engine::debounceClockMs() const {
    if (offlineDevice()) {
        const auto* off = static_cast<const io::OfflineDeviceIO*>(device_.get());
        return (double) off->renderedFrames() * 1000.0 / off->sampleRate();
    }
    return juce::Time::getMillisecondCounterHiRes();
}

void Engine::refreshAllWarps() {
    for (int t = 0; t < LE_MAX_TRACKS; ++t)
        for (int s = 0; s < LE_MAX_SCENES; ++s) refreshWarp(t, s);
}

void Engine::refreshWarp(int t, int s) {
    WarpJob& job = warp_[t][s];
    auto cancelJob = [&] {
        if (job.id != 0) jobs_->cancel(job.id);
        job = WarpJob{};
    };
    const auto& cm = model_.clips[t][s];
    const bool need = cm && cm->kind == ClipKind::Audio && cm->audio != nullptr && cm->warp == WarpMode::Stretch &&
                      cm->originalBpm > 0.0 && std::fabs(cm->originalBpm - bpm_) > kBpmMatch;
    if (!need) {   // Re-Pitch, cùng BPM gốc, hoặc ô trống → không cần bản stretched
        cancelJob();
        return;
    }
    if (cm->stretchedValid() && std::fabs(cm->stretchedBpm - bpm_) <= kBpmMatch) {   // đã có đúng bản
        cancelJob();
        return;
    }
    if (job.id != 0 && jobs_->exists(job.id) && std::fabs(job.bpm - bpm_) <= kBpmMatch && job.source.lock() == cm->audio)
        return;   // đang render đúng bản này
    cancelJob();

    struct Shared {   // worker ghi out trước khi xong; main đọc trong onDone (sau acquire của status)
        dsp::AudioDataPtr out;
        std::int64_t id = 0;
    };
    auto sh = std::make_shared<Shared>();
    const dsp::AudioDataPtr src = cm->audio;
    const double orig = cm->originalBpm, target = bpm_;
    const std::string cache = model_.projectDir.empty() ? std::string()
                                                         : render::stretchedCachePath(model_.projectDir, cm->clipId, target);
    const auto id = jobs_->submit(
        "warp",
        [src, orig, target, cache, sh](JobSystem::Context& ctx) {   // [worker]
            const std::int64_t frames = render::warpedLength(src->numFrames(), orig, target);
            if (!cache.empty()) {
                if (auto hit = render::readStretchedCache(cache, frames, src->sampleRate())) {
                    sh->out = std::move(hit);
                    return JobOutcome::ok({});
                }
            }
            render::WarpConfig cfg;
            cfg.originalBpm = orig;
            cfg.newBpm = target;
            cfg.cancel = &ctx.cancel;
            cfg.progress = &ctx.progress;
            render::WarpResult r = render::renderWarp(*src, cfg);
            if (!r.ok) return JobOutcome::fail(r.error, r.message);
            if (!cache.empty()) {
                juce::File(juce::String::fromUTF8(cache.c_str())).getParentDirectory().createDirectory();
                (void) render::writeStretchedCache(cache, *r.data);   // ghi cache lỗi: vẫn dùng bản trong RAM
            }
            sh->out = std::move(r.data);
            return JobOutcome::ok({});
        },
        [this, t, s, src, target, sh](JobOutcome& o) {   // [main]
            if (warp_[t][s].id == sh->id) warp_[t][s] = WarpJob{};
            if (o.error != LE_OK || sh->out == nullptr) return;   // huỷ / lỗi
            auto& c = model_.clips[t][s];
            if (!c || c->audio != src || c->warp != WarpMode::Stretch || std::fabs(bpm_ - target) > kBpmMatch) return;   // đã cũ
            c->stretched = std::move(sh->out);
            c->stretchedBpm = target;
            c->stretchedSource = src;
            publishSnapshot();
        },
        false);   // job nội bộ: không phát JOB_* cho Dart
    sh->id = id;
    job.id = id;
    job.bpm = target;
    job.source = src;
    if (offlineDevice()) (void) jobs_->waitWorker(id, 60000);   // tất định: onDone áp ở pump kế tiếp
}

} // namespace le::core
