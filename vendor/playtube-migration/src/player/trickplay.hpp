#pragma once

#include <optional>
#include <vector>

#include <cstdint>

namespace playtube::player {

// A video's seek preview pictures: sheets of tileWidth x tileHeight pictures of width x height pixels, one picture
// every intervalMs, thumbnailCount in all.
struct TrickplayInfo {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int thumbnailCount = 0;
    std::int64_t intervalMs = 0;
};

// Where the picture for a time is: the sheet's number and its rectangle in the sheet, in pixels.
struct TrickplayFrame {
    int sheet = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// The picture for `seconds` (clamped to the first and last pictures).
TrickplayFrame trickplayFrame(const TrickplayInfo& info, double seconds);
// How many sheets the pictures fill.
int trickplaySheets(const TrickplayInfo& info);
// The image decoder's limits (ui::kMaxImageSide, ui::kMaxImagePixels): a bigger sheet would never show.
constexpr int kMaxSheetSide = 4096;
constexpr long long kMaxSheetPixels = 8LL * 1024 * 1024;

// The width to show on a 1080p TV, among those whose sheets the decoder takes: the smallest at least 240 pixels
// wide, else the largest; none when there is none.
std::optional<TrickplayInfo> pickTrickplay(const std::vector<TrickplayInfo>& widths);

}  // namespace playtube::player
