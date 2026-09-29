#include "io/SfzLoader.h"

#include "le/engine_api.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unordered_map>

namespace le::io {

namespace {

using dsp::LoopMode;
using dsp::Zone;

enum class Scope { None, Control, Global, Master, Group, Region, Unknown };

struct Op {
    std::string name, value;
    int line = 0;
};

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}
bool isBlank(char c) { return c == ' ' || c == '\t'; }

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) s.remove_suffix(1);
    return s;
}

std::string slashes(std::string_view s) {
    std::string out(s);
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

// Xoá comment // … và /* … */ nhưng GIỮ ký tự xuống dòng để số dòng báo lỗi vẫn đúng.
std::string stripComments(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        if (text.compare(i, 2, "//") == 0) {
            while (i < text.size() && text[i] != '\n') ++i;
        } else if (text.compare(i, 2, "/*") == 0) {
            i += 2;
            while (i < text.size() && text.compare(i, 2, "*/") != 0) {
                if (text[i] == '\n') out.push_back('\n');
                ++i;
            }
            i = std::min(text.size(), i + 2);
        } else {
            out.push_back(text[i++]);
        }
    }
    return out;
}

bool parseInt(std::string_view s, int64_t& out) {
    s = trim(s);
    if (!s.empty() && s.front() == '+') s.remove_prefix(1);
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

bool parseFloat(std::string_view s, float& out) {
    // from_chars cho float chưa có trên mọi libc++ cũ → dùng istringstream (worker, chậm cũng được).
    std::istringstream is{std::string(trim(s))};
    is.imbue(std::locale::classic());
    float v = 0.0f;
    is >> v;
    if (is.fail() || !is.eof() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

// Opcode được hỗ trợ ở <global>/<master>/<group>/<region>, và giá trị có hợp lệ không.
enum class Kind { Path, Note, Int, Float, LoopModeOp, Unsupported };
Kind kindOf(const std::string& n) {
    if (n == "sample") return Kind::Path;
    if (n == "lokey" || n == "hikey" || n == "key" || n == "pitch_keycenter") return Kind::Note;
    if (n == "lovel" || n == "hivel" || n == "loop_start" || n == "loop_end" || n == "group" || n == "off_by")
        return Kind::Int;
    if (n == "tune" || n == "volume" || n == "pan" || n == "ampeg_attack" || n == "ampeg_decay" ||
        n == "ampeg_sustain" || n == "ampeg_release")
        return Kind::Float;
    if (n == "loop_mode") return Kind::LoopModeOp;
    return Kind::Unsupported;
}

struct Parser {
    std::string baseDir;
    SfzParseResult result;

    std::string defaultPath;
    std::vector<Op> global, master, group, region;
    int regionLine = 0;
    bool inRegion = false;
    Scope scope = Scope::None;

    std::map<std::string, std::pair<int, int>> unsupportedOps;   // tên → (số lần, dòng đầu)
    std::map<std::string, int> unsupportedHeaders;               // tên → dòng đầu

    void warn(std::string msg) { result.warnings.push_back(std::move(msg)); }

    // Kiểm tra giá trị ngay lúc gặp (mỗi opcode chỉ cảnh báo 1 lần dù được kế thừa xuống nhiều region).
    bool validate(const std::string& name, const std::string& value, int line) {
        bool ok = true;
        int64_t i = 0;
        float f = 0.0f;
        switch (kindOf(name)) {
            case Kind::Path: ok = !value.empty(); break;
            case Kind::Note: ok = parseSfzNote(value) >= 0; break;
            case Kind::Int: ok = parseInt(value, i); break;
            case Kind::Float: ok = parseFloat(value, f); break;
            case Kind::LoopModeOp: {
                const std::string v = lower(value);
                ok = v == "no_loop" || v == "one_shot" || v == "loop_continuous" || v == "loop_sustain";
                if (v == "loop_sustain")
                    warn("Dòng " + std::to_string(line) + ": loop_mode=loop_sustain chưa hỗ trợ, xử lý như loop_continuous");
                break;
            }
            case Kind::Unsupported: {
                auto& e = unsupportedOps[name];
                if (e.first++ == 0) e.second = line;
                return false;
            }
        }
        if (!ok) warn("Dòng " + std::to_string(line) + ": giá trị không hợp lệ " + name + "=" + value + ", bỏ qua");
        return ok;
    }

    void opcode(const std::string& name, const std::string& value, int line) {
        switch (scope) {
            case Scope::Control:
                if (name == "default_path") defaultPath = slashes(value);
                else validate(name, value, line);   // opcode khác của <control> → cảnh báo "không hỗ trợ"
                return;
            case Scope::Unknown:
                return;                              // header lạ đã được cảnh báo
            case Scope::None:
                warn("Dòng " + std::to_string(line) + ": opcode " + name + " nằm ngoài header nào, bỏ qua");
                return;
            case Scope::Global:
            case Scope::Master:
            case Scope::Group:
            case Scope::Region:
                break;
        }
        if (!validate(name, value, line)) return;
        std::vector<Op>& dst = scope == Scope::Global ? global : scope == Scope::Master ? master
                             : scope == Scope::Group  ? group  : region;
        dst.push_back({name, value, line});
    }

    void header(const std::string& name, int line) {
        flushRegion();
        if (name == "control") scope = Scope::Control;
        else if (name == "global") { scope = Scope::Global; global.clear(); master.clear(); group.clear(); }
        else if (name == "master") { scope = Scope::Master; master.clear(); group.clear(); }
        else if (name == "group") { scope = Scope::Group; group.clear(); }
        else if (name == "region") { scope = Scope::Region; region.clear(); regionLine = line; inRegion = true; }
        else {
            scope = Scope::Unknown;
            if (unsupportedHeaders.find(name) == unsupportedHeaders.end()) unsupportedHeaders[name] = line;
        }
    }

    static void apply(Zone& z, const Op& op, std::string& sample) {
        const std::string& n = op.name;
        int64_t i = 0;
        float f = 0.0f;
        if (n == "sample") sample = op.value;
        else if (n == "key") {
            const int k = parseSfzNote(op.value);
            z.loKey = z.hiKey = z.rootKey = static_cast<int16_t>(k);
        }
        else if (n == "lokey") z.loKey = static_cast<int16_t>(parseSfzNote(op.value));
        else if (n == "hikey") z.hiKey = static_cast<int16_t>(parseSfzNote(op.value));
        else if (n == "pitch_keycenter") z.rootKey = static_cast<int16_t>(parseSfzNote(op.value));
        else if (n == "lovel" && parseInt(op.value, i)) z.loVel = static_cast<int16_t>(std::clamp<int64_t>(i, 1, 127));
        else if (n == "hivel" && parseInt(op.value, i)) z.hiVel = static_cast<int16_t>(std::clamp<int64_t>(i, 1, 127));
        else if (n == "tune" && parseFloat(op.value, f)) z.tuneCents = f;
        else if (n == "volume" && parseFloat(op.value, f)) z.gainDb = f;
        else if (n == "pan" && parseFloat(op.value, f)) z.pan = std::clamp(f / 100.0f, -1.0f, 1.0f);
        else if (n == "loop_mode") {
            const std::string v = lower(op.value);
            z.loopMode = v == "one_shot" ? LoopMode::OneShot
                       : (v == "loop_continuous" || v == "loop_sustain") ? LoopMode::LoopContinuous
                       : LoopMode::NoLoop;
        }
        else if (n == "loop_start" && parseInt(op.value, i)) z.loopStart = std::max<int64_t>(0, i);
        else if (n == "loop_end" && parseInt(op.value, i)) z.loopEnd = i + 1;   // SFZ: loop_end GỒM sample cuối
        else if (n == "ampeg_attack" && parseFloat(op.value, f)) z.env.attack = std::max(0.0f, f);
        else if (n == "ampeg_decay" && parseFloat(op.value, f)) z.env.decay = std::max(0.0f, f);
        else if (n == "ampeg_sustain" && parseFloat(op.value, f)) z.env.sustain = std::clamp(f / 100.0f, 0.0f, 1.0f);
        else if (n == "ampeg_release" && parseFloat(op.value, f)) z.env.release = std::max(0.0f, f);
        else if (n == "group" && parseInt(op.value, i)) z.group = static_cast<int32_t>(i);
        else if (n == "off_by" && parseInt(op.value, i)) z.offBy = static_cast<int32_t>(i);
    }

    void flushRegion() {
        if (!inRegion) return;
        inRegion = false;
        SfzRegion r;
        r.line = regionLine;
        // Giá trị mặc định của SFZ: release 0.001 s (Adsr tự nâng lên sàn 5 ms), sustain 100 %
        r.zone.env = {0.0f, 0.0f, 1.0f, 0.001f};
        std::string sample;
        for (const auto* level : {&global, &master, &group, &region})
            for (const Op& op : *level) apply(r.zone, op, sample);

        const std::string where = "Region ở dòng " + std::to_string(regionLine);
        if (sample.empty()) {
            warn(where + " không có sample=, bỏ qua");
            return;
        }
        if (r.zone.loKey > r.zone.hiKey) {
            warn(where + ": lokey > hikey, bỏ qua");
            return;
        }
        if (r.zone.loVel > r.zone.hiVel) {
            warn(where + ": lovel > hivel, bỏ qua");
            return;
        }
        r.sampleName = sample;
        const std::filesystem::path s = slashes(sample);
        std::filesystem::path full = s.is_absolute() ? s : std::filesystem::path(baseDir) / defaultPath / s;
        r.samplePath = full.lexically_normal().generic_string();
        result.regions.push_back(std::move(r));
    }

    void finish() {
        flushRegion();
        for (const auto& [name, e] : unsupportedOps)
            warn("Bỏ qua opcode không hỗ trợ '" + name + "' (" + std::to_string(e.first) + " lần, lần đầu ở dòng " +
                 std::to_string(e.second) + ")");
        for (const auto& [name, line] : unsupportedHeaders)
            warn("Bỏ qua header không hỗ trợ <" + name + "> (dòng " + std::to_string(line) + ") cùng các opcode của nó");
    }
};

// Tìm chỗ kết thúc giá trị đường dẫn (có thể chứa dấu cách): hết dòng, gặp '<', hoặc gặp " tên=".
size_t pathValueEnd(std::string_view t, size_t i) {
    for (size_t k = i; k < t.size(); ++k) {
        const char c = t[k];
        if (c == '\n' || c == '\r' || c == '<') return k;
        if (isBlank(c)) {
            size_t p = k;
            while (p < t.size() && isBlank(t[p])) ++p;
            size_t q = p;
            while (q < t.size() && isIdentChar(t[q])) ++q;
            if (q > p && q < t.size() && t[q] == '=') return k;
        }
    }
    return t.size();
}

} // namespace

int parseSfzNote(std::string_view s) {
    s = trim(s);
    if (s.empty()) return -1;
    int64_t n = 0;
    if (parseInt(s, n)) return (n >= 0 && n <= 127) ? static_cast<int>(n) : -1;

    static constexpr int kSemis[7] = {9, 11, 0, 2, 4, 5, 7};   // a b c d e f g
    const char letter = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
    if (letter < 'a' || letter > 'g') return -1;
    int semi = kSemis[letter - 'a'];
    size_t i = 1;
    if (i < s.size() && s[i] == '#') { ++semi; ++i; }
    else if (i + 1 < s.size() && (s[i] == 'b' || s[i] == 'B')) { --semi; ++i; }   // "bb4", "db4": b là dấu giáng
    int64_t octave = 0;
    if (!parseInt(s.substr(i), octave)) return -1;
    const int64_t note = (octave + 1) * 12 + semi;              // c4 = 60 (quy ước SFZ)
    return (note >= 0 && note <= 127) ? static_cast<int>(note) : -1;
}

// [worker]
SfzParseResult parseSfz(std::string_view rawText, const std::string& baseDir) {
    Parser p;
    p.baseDir = baseDir;
    if (rawText.size() >= 3 && rawText.compare(0, 3, "\xEF\xBB\xBF") == 0) rawText.remove_prefix(3);   // BOM UTF-8
    const std::string text = stripComments(rawText);
    const std::string_view t = text;

    int line = 1;
    size_t i = 0;
    while (i < t.size()) {
        const char c = t[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (std::isspace(static_cast<unsigned char>(c)) != 0) { ++i; continue; }
        if (c == '<') {
            const size_t j = t.find('>', i);
            if (j == std::string_view::npos || t.substr(i, j - i).find('\n') != std::string_view::npos) {
                p.result.error = LE_ERR_FILE_FORMAT;
                p.result.message = "Dòng " + std::to_string(line) + ": header thiếu dấu '>'";
                return p.result;
            }
            p.header(lower(trim(t.substr(i + 1, j - i - 1))), line);
            i = j + 1;
            continue;
        }
        if (c == '#') {   // #define / #include
            size_t e = i;
            while (e < t.size() && t[e] != '\n') ++e;
            p.warn("Dòng " + std::to_string(line) + ": chỉ thị '" + std::string(trim(t.substr(i, e - i))) + "' không hỗ trợ, bỏ qua");
            i = e;
            continue;
        }
        size_t j = i;
        while (j < t.size() && isIdentChar(t[j])) ++j;
        if (j == i || j >= t.size() || t[j] != '=') {
            size_t e = i;
            while (e < t.size() && std::isspace(static_cast<unsigned char>(t[e])) == 0) ++e;
            p.warn("Dòng " + std::to_string(line) + ": không hiểu '" + std::string(t.substr(i, e - i)) + "', bỏ qua");
            i = std::max(e, i + 1);
            continue;
        }
        const std::string name = lower(t.substr(i, j - i));
        const size_t vs = j + 1;
        size_t ve = vs;
        if (name == "sample" || name == "default_path") {
            ve = pathValueEnd(t, vs);
        } else {
            while (ve < t.size() && std::isspace(static_cast<unsigned char>(t[ve])) == 0 && t[ve] != '<') ++ve;
        }
        p.opcode(name, std::string(trim(t.substr(vs, ve - vs))), line);
        i = ve;
    }
    p.finish();
    p.result.ok = true;
    p.result.error = LE_OK;
    return p.result;
}

// [worker]
SfzLoadResult loadSfzText(std::string_view text, const std::string& baseDir, const std::string& name,
                          const SfzLoadOptions& options) {
    SfzLoadResult out;
    if (!options.loadSample) {
        out.error = LE_ERR_INVALID_ARG;
        out.message = "SfzLoadOptions::loadSample chưa được đặt";
        return out;
    }
    SfzParseResult parsed = parseSfz(text, baseDir);
    out.warnings = std::move(parsed.warnings);
    if (!parsed.ok) {
        out.error = parsed.error;
        out.message = parsed.message;
        return out;
    }

    auto inst = std::make_shared<dsp::Instrument>();
    inst->name = name;
    std::unordered_map<std::string, dsp::AudioDataPtr> cache;   // mỗi file sample chỉ nạp 1 lần

    for (SfzRegion& r : parsed.regions) {
        if (options.cancel != nullptr && options.cancel->load(std::memory_order_relaxed)) {
            out.error = LE_ERR_JOB_CANCELLED;
            out.message = "Đã huỷ nạp " + name;
            return out;
        }
        auto it = cache.find(r.samplePath);
        if (it == cache.end()) {
            DecodeResult d = options.loadSample(r.samplePath);
            const bool empty = d.data == nullptr || d.data->numFrames() == 0;
            if (d.error != LE_OK || empty) {
                // Giữ mã lỗi của loader (FILE_NOT_FOUND / FILE_FORMAT / OUT_OF_MEMORY); file rỗng = FILE_FORMAT
                out.error = d.error != LE_OK ? d.error : LE_ERR_FILE_FORMAT;
                out.message = "Không nạp được sample '" + r.sampleName + "' (dòng " + std::to_string(r.line) +
                              ", đường dẫn " + r.samplePath + ")" +
                              (d.message.empty() ? (empty && d.error == LE_OK ? ": file rỗng" : "") : ": " + d.message);
                return out;
            }
            it = cache.emplace(r.samplePath, std::move(d.data)).first;
            inst->samples.push_back(it->second);
        }
        Zone z = r.zone;
        z.data = it->second.get();
        const int64_t n = z.data->numFrames();
        if (z.loopMode == LoopMode::LoopContinuous && (z.loopEnd > n || z.loopStart >= n)) {
            out.warnings.push_back("Region ở dòng " + std::to_string(r.line) + ": loop vượt quá độ dài sample (" +
                                   std::to_string(n) + " frame), đã kẹp lại");
            z.loopEnd = std::min(z.loopEnd, n);
            z.loopStart = std::min(z.loopStart, std::max<int64_t>(0, n - 1));
        }
        inst->zones.push_back(z);
    }
    if (inst->zones.empty()) {
        out.error = LE_ERR_FILE_FORMAT;
        out.message = "Không có region nào dùng được trong " + name;
        return out;
    }
    std::stable_sort(inst->zones.begin(), inst->zones.end(),
                     [](const Zone& a, const Zone& b) { return a.loKey < b.loKey; });

    out.regions = static_cast<int>(inst->zones.size());
    out.samplesLoaded = static_cast<int>(inst->samples.size());
    out.instrument = std::move(inst);
    out.ok = true;
    out.error = LE_OK;
    return out;
}

// [worker]
SfzLoadResult loadSfzFile(const std::string& sfzPath, const SfzLoadOptions& options) {
    const std::filesystem::path path(sfzPath);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SfzLoadResult out;
        out.error = LE_ERR_FILE_NOT_FOUND;
        out.message = "Không mở được file SFZ: " + sfzPath;
        return out;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadSfzText(ss.str(), path.parent_path().generic_string(), path.stem().string(), options);
}

} // namespace le::io
