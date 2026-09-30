// 05 §3 preview.play / preview.stop: nghe thử trong Browser qua kênh preview riêng (PreviewPlayer, ngoài 8 track).
// [main] Mọi hàm ở đây chạy trên main thread.
//   source.base: "library" (mặc định) | "project" (bản thu trong project đang mở).
//   preview.play → lượt mới (id tăng) → cache LRU 3 mục có sẵn thì phát ngay, không thì job nạp im lặng trên worker
//   (SfzLoader / decodeAudioFile) → xong mà vẫn là lượt mới nhất thì phát; lượt đã đổi thì chỉ vào cache.
//   Vé = lịch nốt dựng sẵn trên main: kit → groove kick-hat-snare-hat 1 bar (BPM đang chạy, transport dừng → 120);
//   nhạc cụ → arpeggio C-E-G-C (note, +4, +7, +12) chia đều durationMs; audio → phát từ đầu tới durationMs rồi fade.
//   Không đụng model / snapshot / transport. Vé + dữ liệu sống tới khi RT báo PreviewReleased (độc lập với cache).
//   track.setInstrument cùng file SFZ đang trong cache → dùng CHUNG Instrument (không nạp lại, không tốn RAM gấp đôi).
//   Offline (test / sim): chờ worker xong → lượt bắt đầu ở block sau pump kế tiếp (tất định).
#include <algorithm>
#include <cmath>

#include <juce_core/juce_core.h>

#include "core/Engine.h"
#include "io/AudioFileIO.h"
#include "io/SfzLoader.h"

namespace le::core {

namespace {

using Ticket = PreviewPlayer::Ticket;

void addNote(Ticket& t, double on, double off, int note, float vel) {
    if (t.numEvents + 2 > PreviewPlayer::kMaxEvents) return;
    t.events[t.numEvents++] = {on, (std::uint8_t) note, vel, true};
    t.events[t.numEvents++] = {off, (std::uint8_t) note, 0.0f, false};
}

// Kit = đa số zone one-shot (drum kit SFZ, 06 §4).
bool isKit(const dsp::Instrument& inst) {
    int oneShot = 0;
    for (const auto& z : inst.zones) oneShot += z.loopMode == dsp::LoopMode::OneShot ? 1 : 0;
    return !inst.zones.empty() && 2 * oneShot >= (int) inst.zones.size();
}

// Nốt GM (36 kick, 38 snare, 42 hat đóng) nếu kit có; không thì zone thứ `fallback` (theo phím) — kit tự làm vẫn kêu.
int kitNote(const dsp::Instrument& inst, int gm, std::size_t fallback) {
    if (inst.findZone(gm, 100) != nullptr) return gm;
    std::vector<int> keys;
    for (const auto& z : inst.zones) {
        const int k = std::clamp<int>(z.loKey, 0, 127);
        if (z.data != nullptr && inst.findZone(k, 100) != nullptr && std::find(keys.begin(), keys.end(), k) == keys.end())
            keys.push_back(k);
    }
    if (keys.empty()) return gm;
    std::sort(keys.begin(), keys.end());
    return keys[std::min(fallback, keys.size() - 1)];
}

} // namespace

void Engine::offerPreview(const PreviewEntry& e, std::uint32_t id, int note, double durationMs) {
    auto t = std::make_unique<Ticket>();
    t->id = id;
    t->instrument = e.instrument.get();
    t->audio = e.instrument != nullptr ? nullptr : e.audio.get();
    if (e.instrument != nullptr && isKit(*e.instrument)) {
        LeState st{};
        globalStatePublisher().read(st);
        const double bpm = st.playing != 0 && st.bpm >= 20.0 && st.bpm <= 300.0 ? st.bpm : 120.0;
        const double eighth = 30.0 / bpm;
        const int hits = std::clamp(2 * model_.beatsPerBar, 2, PreviewPlayer::kMaxEvents / 2);   // 1 bar nốt móc đơn
        const int kick = kitNote(*e.instrument, 36, 0), snare = kitNote(*e.instrument, 38, 1), hat = kitNote(*e.instrument, 42, 2);
        for (int i = 0; i < hits; ++i) {
            const bool odd = (i % 2) == 1;
            const int n = odd ? hat : ((i % 4) == 0 ? kick : snare);
            addNote(*t, i * eighth, (i + 0.5) * eighth, n, odd ? 0.55f : ((i % 4) == 0 ? 0.9f : 0.8f));
        }
        t->lengthSec = hits * eighth;
    } else if (e.instrument != nullptr) {
        const double step = durationMs / 4000.0;
        const int notes[4] = {note, std::min(127, note + 4), std::min(127, note + 7), note + 12 <= 127 ? note + 12 : note};
        for (int k = 0; k < 4; ++k) addNote(*t, k * step, (k + 1) * step, notes[k], 0.8f);   // off trước on kế tiếp
        t->lengthSec = durationMs / 1000.0;
    } else {
        t->lengthSec = durationMs / 1000.0;
    }
    Ticket* raw = t.get();
    previewLive_.push_back({std::move(t), e.instrument, e.audio});
    if (Ticket* back = rt_.preview().offer(raw)) releasePreviewTicket(back->id);   // RT chưa lấy vé trước → nhả ngay
}

void Engine::releasePreviewTicket(std::uint32_t id) {
    previewLive_.erase(std::remove_if(previewLive_.begin(), previewLive_.end(),
                                      [id](const PreviewLive& l) { return l.ticket->id == id; }),
                       previewLive_.end());
}

// {source:{kind:"sfz",path,base?} | {kind:"audio",file,base?}, note?:60, durationMs?:1500} → {}
Reply Engine::opPreviewPlay(const juce::var& req) {
    const juce::var src = req.getProperty("source", {});
    std::string kind, file;
    if (!src.isObject() || !args::getString(src, "kind", kind)) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu \"source\":{kind,…}");
    if (kind == "sfz") {
        if (!args::getString(src, "path", file) || file.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu source.path");
    } else if (kind == "audio") {
        if (!args::getString(src, "file", file) || file.empty()) return Reply::fail(LE_ERR_INVALID_ARG, "thiếu source.file");
    } else {
        return Reply::fail(LE_ERR_INVALID_ARG, "source.kind phải là \"sfz\" hoặc \"audio\"");
    }
    int note = 60;
    double durationMs = 1500.0;
    if (req.hasProperty("note") && !args::getInt(req, "note", note, 0, 127)) return Reply::fail(LE_ERR_INVALID_ARG, "note phải là 0..127");
    if (req.hasProperty("durationMs") && !args::getDouble(req, "durationMs", durationMs, 50.0, 30000.0))
        return Reply::fail(LE_ERR_INVALID_ARG, "durationMs phải trong 50..30000");
    // base: "library" (mặc định, theo libraryDir) | "project" (theo project.open — "Bản thu của tôi"). Tuyệt đối: dùng nguyên.
    std::string base = "library";
    if (src.hasProperty("base") && (!args::getString(src, "base", base) || (base != "library" && base != "project")))
        return Reply::fail(LE_ERR_INVALID_ARG, "source.base phải là \"library\" hoặc \"project\"");
    std::string abs = file;
    if (!juce::File::isAbsolutePath(juce::String::fromUTF8(file.c_str()))) {
        const std::string& dir = base == "project" ? model_.projectDir : libraryDir_;
        if (dir.empty())
            return Reply::fail(LE_ERR_INVALID_ARG, base == "project" ? "base:\"project\" nhưng chưa project.open" : "chưa có libraryDir");
        abs = juce::File(juce::String::fromUTF8(dir.c_str())).getChildFile(juce::String::fromUTF8(file.c_str())).getFullPathName().toStdString();
    }
    if (!juce::File(juce::String::fromUTF8(abs.c_str())).existsAsFile()) return Reply::fail(LE_ERR_FILE_NOT_FOUND, "không có file " + file);

    const std::uint32_t id = ++previewSeq_;
    if (previewJob_ != 0) {   // lượt nạp cũ không còn cần (nạp dở thì bỏ, không vào cache)
        (void) jobs_->cancel(previewJob_);
        previewJob_ = 0;
    }
    const std::string key = kind + ":" + abs;
    const auto hit = std::find_if(previewCache_.begin(), previewCache_.end(), [&](const PreviewEntry& x) { return x.key == key; });
    if (hit != previewCache_.end()) {
        std::rotate(previewCache_.begin(), hit, hit + 1);   // → dùng gần nhất
        offerPreview(previewCache_.front(), id, note, durationMs);
        return Reply::ok();
    }

    auto sh = std::make_shared<PreviewEntry>();   // worker ghi trước khi xong; main đọc trong onDone (acquire)
    sh->key = key;
    const bool sfz = kind == "sfz";
    const auto job = jobs_->submit(
        "preview",
        [abs, sfz, sh](JobSystem::Context& ctx) {   // [worker]
            if (sfz) {
                io::SfzLoadOptions o;
                o.loadSample = &io::decodeAudioFile;
                o.cancel = &ctx.cancel;
                io::SfzLoadResult r = io::loadSfzFile(abs, o);
                if (!r.ok) return JobOutcome::fail(r.error != LE_OK ? r.error : LE_ERR_FILE_FORMAT, r.message);
                sh->name = r.instrument->name;
                sh->regions = r.regions;
                sh->samplesLoaded = r.samplesLoaded;
                sh->warnings = std::move(r.warnings);
                sh->instrument = std::move(r.instrument);
            } else {
                io::DecodeResult d = io::decodeAudioFile(abs);
                if (d.error != LE_OK) return JobOutcome::fail(d.error, d.message);
                sh->audio = std::move(d.data);
            }
            return JobOutcome::ok({});
        },
        [this, id, note, durationMs, sh](JobOutcome& out) {   // [main]
            if (previewSeq_ == id) previewJob_ = 0;
            if (out.error != LE_OK) {
                DBG("preview: " << juce::String::fromUTF8(out.message.c_str()));
                return;
            }
            // Hai job cùng file (bấm A, B, A thật nhanh) → một mục
            previewCache_.erase(std::remove_if(previewCache_.begin(), previewCache_.end(),
                                               [&](const PreviewEntry& x) { return x.key == sh->key; }),
                                previewCache_.end());
            previewCache_.insert(previewCache_.begin(), *sh);
            if (previewCache_.size() > kPreviewCacheSize) previewCache_.resize(kPreviewCacheSize);   // vé đang phát giữ bản riêng
            if (previewSeq_ == id) offerPreview(previewCache_.front(), id, note, durationMs);
        },
        false);   // job nội bộ: Dart không biết jobId (reply là {})
    previewJob_ = job;
    if (offlineDevice()) (void) jobs_->waitWorker(job, 60000);   // tất định: phát ở block sau pump kế tiếp
    return Reply::ok();
}

const Engine::PreviewEntry* Engine::previewCachedSfz(const std::string& absPath) {
    const std::string key = "sfz:" + absPath;
    const auto hit = std::find_if(previewCache_.begin(), previewCache_.end(), [&](const PreviewEntry& x) { return x.key == key; });
    if (hit == previewCache_.end() || hit->instrument == nullptr) return nullptr;
    std::rotate(previewCache_.begin(), hit, hit + 1);
    return &previewCache_.front();
}

// {} → {}: dừng lượt đang nghe (fade) và bỏ lượt đang nạp.
Reply Engine::opPreviewStop(const juce::var&) {
    ++previewSeq_;
    if (previewJob_ != 0) {
        (void) jobs_->cancel(previewJob_);
        previewJob_ = 0;
    }
    if (Ticket* back = rt_.preview().offer(nullptr)) releasePreviewTicket(back->id);   // vé RT chưa kịp lấy
    rt_.preview().requestStop();
    return Reply::ok();
}

} // namespace le::core
