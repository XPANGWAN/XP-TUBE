#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace playtube::player {

// Keeps a repeated message out of the log. Some videos make the decoder warn about every picture group ("Late SEI
// is not implemented", every few seconds): four hours of one filled both log files and pushed everything else out
// (seen 2026-11-21). A message is written the first time, then at its 10th, 100th, 1000th… repeat with the count.
class Repeats {
public:
    // What to write for this message now, if anything: the message itself the first time, then
    // "<message> (N times)" at each power of ten.
    std::optional<std::string> line(const std::string& message);

private:
    static constexpr std::size_t kKept = 32;  // distinct messages remembered (the oldest is forgotten)
    std::vector<std::pair<std::string, long>> seen_;
};

}  // namespace playtube::player
