#include "player/seek.hpp"

#include <algorithm>

namespace playtube::player {

namespace {
constexpr std::int64_t kSettleMs = 300;
}

void SeekAccumulator::press(double deltaSeconds, std::int64_t nowMs, double position, double duration) {
    const double from = pending_ ? target_ : position;
    const double last = std::max(0.0, duration - 1.0);
    target_ = std::clamp(from + deltaSeconds, 0.0, last);
    pending_ = true;
    lastPressMs_ = nowMs;
}

std::optional<double> SeekAccumulator::due(std::int64_t nowMs) {
    if (!pending_ || nowMs - lastPressMs_ < kSettleMs) {
        return std::nullopt;
    }
    pending_ = false;
    return target_;
}

double seekStep(std::int64_t heldMs) {
    if (heldMs >= 6000) {
        return 120.0;
    }
    if (heldMs >= 4000) {
        return 60.0;
    }
    return heldMs >= 2000 ? 30.0 : 10.0;
}

void ButtonRepeat::press(int direction, std::int64_t nowMs) {
    direction_ = direction;
    nextMs_ = nowMs + kDelayMs;
}

int ButtonRepeat::poll(std::int64_t nowMs) {
    if (direction_ == 0 || nowMs < nextMs_) {
        return 0;
    }
    const std::int64_t due = (nowMs - nextMs_) / kIntervalMs + 1;
    nextMs_ += due * kIntervalMs;
    return static_cast<int>(due < kMaxCatchUp ? due : kMaxCatchUp);
}

void HoldToSkip::press(int direction, std::int64_t nowMs) {
    direction_ = direction;
    startMs_ = nowMs;
}

double HoldToSkip::progress(std::int64_t nowMs) const {
    if (direction_ == 0) {
        return 0;
    }
    const double done = static_cast<double>(nowMs - startMs_) / static_cast<double>(kHoldMs);
    return done < 0 ? 0 : done > 1 ? 1 : done;
}

bool HoldToSkip::due(std::int64_t nowMs) {
    if (direction_ == 0 || nowMs - startMs_ < kHoldMs) {
        return false;
    }
    direction_ = 0;
    return true;
}

int chapterAt(const std::vector<double>& starts, double seconds) {
    int index = -1;
    for (std::size_t i = 0; i < starts.size(); ++i) {
        if (starts[i] <= seconds + 0.01) {
            index = static_cast<int>(i);
        }
    }
    return index;
}

double chapterTarget(const std::vector<double>& starts, double seconds, int direction) {
    if (starts.empty()) {
        return -1;
    }
    const int current = std::max(0, chapterAt(starts, seconds));
    int target = current + direction;
    if (direction < 0 && seconds - starts[static_cast<std::size_t>(current)] > 3) {
        target = current;
    }
    if (target < 0) {
        return starts.front();  // the first one: its start again
    }
    if (target >= static_cast<int>(starts.size())) {
        return -1;
    }
    return starts[static_cast<std::size_t>(target)];
}

}  // namespace playtube::player
