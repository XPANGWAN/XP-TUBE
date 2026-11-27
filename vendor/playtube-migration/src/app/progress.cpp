#include "app/progress.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/file_io.hpp"
#include "core/json.hpp"

namespace playtube::app {

namespace {
constexpr std::size_t kMaxFileBytes = 512 * 1024;
constexpr std::size_t kMaxIdBytes = 64;
}  // namespace

ProgressStore::ProgressStore(std::string path) : path_(std::move(path)) {
    std::string text;
    if (path_.empty() || core::readSmallFile(path_, kMaxFileBytes, text) != core::ReadStatus::Ok) {
        return;
    }
    const auto doc = core::json::Document::parse(text);
    if (!doc) {
        return;
    }
    yyjson_val* list = core::json::array(doc->root(), "videos");
    std::size_t index = 0;
    std::size_t max = 0;
    yyjson_val* entry = nullptr;
    yyjson_arr_foreach(list, index, max, entry) {
        const std::string id = core::json::string(entry, "id");
        WatchProgress progress;
        progress.position = core::json::integer(entry, "position", -1);
        progress.duration = core::json::integer(entry, "duration", 0);
        progress.updated = core::json::integer(entry, "updated", 0);
        if (!id.empty() && id.size() <= kMaxIdBytes && progress.duration > 0 && progress.position >= 0 &&
            progress.position <= progress.duration && videos_.size() < kMax) {
            videos_[id] = progress;
        }
    }
}

std::optional<WatchProgress> ProgressStore::find(const std::string& videoId) const {
    const auto found = videos_.find(videoId);
    return found == videos_.end() ? std::nullopt : std::optional<WatchProgress>(found->second);
}

void ProgressStore::set(const std::string& videoId, std::int64_t position, std::int64_t duration, std::int64_t now) {
    if (videoId.empty() || videoId.size() > kMaxIdBytes || duration <= 0) {
        return;
    }
    videos_[videoId] = {std::clamp<std::int64_t>(position, 0, duration), duration, now};
    if (videos_.size() > kMax) {  // the oldest goes
        auto oldest = videos_.begin();
        for (auto it = videos_.begin(); it != videos_.end(); ++it) {
            oldest = it->second.updated < oldest->second.updated ? it : oldest;
        }
        videos_.erase(oldest);
    }
    dirty_ = true;
}

int ProgressStore::percent(const std::string& videoId) const {
    const auto found = videos_.find(videoId);
    if (found == videos_.end()) {
        return -1;
    }
    const WatchProgress& p = found->second;
    if (p.position >= p.duration - kMargin) {
        return 100;
    }
    return static_cast<int>(p.position * 100 / p.duration);
}

bool ProgressStore::save() {
    if (!dirty_ || path_.empty()) {
        return true;
    }
    std::vector<std::pair<std::string, WatchProgress>> sorted(videos_.begin(), videos_.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second.updated > b.second.updated; });
    core::json::Writer writer;
    writer.object();
    writer.key("version").value(1);
    writer.key("videos").array();
    for (const auto& [id, p] : sorted) {
        writer.object();
        writer.key("id").value(id);
        writer.key("position").value(p.position);
        writer.key("duration").value(p.duration);
        writer.key("updated").value(p.updated);
        writer.end();
    }
    writer.end();
    writer.end();
    dirty_ = !core::writeFileAtomic(path_, writer.str());
    return !dirty_;
}

std::int64_t ProgressStore::resumeAt(const WatchProgress& progress) {
    if (progress.position < kMargin || progress.position >= progress.duration - kMargin) {
        return 0;
    }
    return progress.position;
}

}  // namespace playtube::app
