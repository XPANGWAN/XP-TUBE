#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace playtube::youtube {

// YouTube's seek preview pictures of a video ("storyboards"): sheets of `columns` x `rows` pictures of `width` x
// `height` pixels, one picture every `intervalMs`, `count` in all.
struct Storyboard {
    int width = 0;
    int height = 0;
    int columns = 0;
    int rows = 0;
    int count = 0;
    std::int64_t intervalMs = 0;
    int level = 0;
    std::string base;  // the address with $L (the level) and $N (the sheet's name)
    std::string name;  // "M$M": $M is the sheet's number
    std::string sigh;  // the level's signature, added as "&sigh="
    // the address of a sheet (https only, else empty)
    std::string sheetUrl(int sheet) const;
};

// From the player reply's "playerStoryboardSpecRenderer.spec" ("base|w#h#count#cols#rows#interval#name#sigh|…"):
// the largest level of pictures at most 320 pixels wide whose sheets stay within the image decoder's limits;
// `durationSeconds` spreads a level without an interval over the video. None when the spec cannot be read.
std::optional<Storyboard> parseStoryboard(std::string_view spec, int durationSeconds);

}  // namespace playtube::youtube
