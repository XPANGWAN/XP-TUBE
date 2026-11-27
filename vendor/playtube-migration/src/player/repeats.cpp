#include "player/repeats.hpp"

namespace playtube::player {

std::optional<std::string> Repeats::line(const std::string& message) {
    for (auto& [text, count] : seen_) {
        if (text != message) {
            continue;
        }
        ++count;
        long power = 10;
        while (power < count) {
            power *= 10;
        }
        if (count == power) {
            return message + " (" + std::to_string(count) + " times)";
        }
        return std::nullopt;
    }
    if (seen_.size() >= kKept) {
        seen_.erase(seen_.begin());
    }
    seen_.emplace_back(message, 1);
    return message;
}

}  // namespace playtube::player
