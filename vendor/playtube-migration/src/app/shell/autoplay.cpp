#include "app/shell/autoplay.hpp"

#include <algorithm>

namespace playtube::app {

bool streamCutShort(double position, double duration, double lastCutAt) {
    if (duration <= 0 || duration - position <= kCutShortSeconds) {
        return false;
    }
    return lastCutAt < 0 || position >= lastCutAt + 5;
}

const youtube::Video* nextToPlay(const std::vector<youtube::Video>& related, bool shorts,
                                 const std::vector<std::string>& played) {
    const youtube::Video* first = nullptr;
    for (const youtube::Video& video : related) {
        if (video.live || video.playlist || video.isChannel || video.id.empty() || (video.isShort && !shorts)) {
            continue;
        }
        if (std::find(played.begin(), played.end(), video.id) == played.end()) {
            return &video;
        }
        if (first == nullptr) {
            first = &video;
        }
    }
    return first;
}

void rememberPlayed(std::vector<std::string>& played, const std::string& id) {
    if (id.empty()) {
        return;
    }
    played.erase(std::remove(played.begin(), played.end(), id), played.end());
    played.push_back(id);
    if (played.size() > kMaxPlayed) {
        played.erase(played.begin(), played.begin() + static_cast<std::ptrdiff_t>(played.size() - kMaxPlayed));
    }
}

}  // namespace playtube::app
