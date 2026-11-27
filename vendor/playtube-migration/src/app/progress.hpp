#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace playtube::app {

// Where a video was left (seconds) and how long it is, kept on the console (a profile's progress.json), signed out
// too; signed in, the account's YouTube history gets it as well when its setting is on (WatchHistory).
struct WatchProgress {
    std::int64_t position = 0;
    std::int64_t duration = 0;
    std::int64_t updated = 0;  // when (seconds since 1970): the oldest go first past kMax
};

class ProgressStore {
public:
    static constexpr std::size_t kMax = 1000;
    static constexpr std::int64_t kMargin = 30;  // less than this from the start or the end: not a place to resume

    // Read from `path` (missing or damaged: empty); "" keeps it in memory only.
    explicit ProgressStore(std::string path = {});

    std::optional<WatchProgress> find(const std::string& videoId) const;
    // A video's place; `duration` 0 (a live) is not kept. Saved by save().
    void set(const std::string& videoId, std::int64_t position, std::int64_t duration, std::int64_t now);
    // 0 to 100 for a card's bar (a video watched to its last kMargin seconds: 100), -1 when never played here.
    int percent(const std::string& videoId) const;
    bool save();

    // Where a video starts again: its place, unless it was barely started or watched to its end (from the start).
    static std::int64_t resumeAt(const WatchProgress& progress);

private:
    std::string path_;
    std::unordered_map<std::string, WatchProgress> videos_;
    bool dirty_ = false;
};

}  // namespace playtube::app
