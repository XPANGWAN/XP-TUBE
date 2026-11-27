#include "player/trickplay.hpp"

#include <algorithm>
#include <cmath>

namespace playtube::player {

TrickplayFrame trickplayFrame(const TrickplayInfo& info, double seconds) {
    TrickplayFrame frame;
    const int perSheet = std::max(1, info.tileWidth * info.tileHeight);
    const double interval = info.intervalMs > 0 ? static_cast<double>(info.intervalMs) : 1000.0;
    const double wanted = std::floor(std::max(0.0, seconds) * 1000.0 / interval);
    const int index = static_cast<int>(std::min(wanted, static_cast<double>(std::max(0, info.thumbnailCount - 1))));
    frame.sheet = index / perSheet;
    const int inSheet = index % perSheet;
    const int columns = std::max(1, info.tileWidth);
    frame.x = (inSheet % columns) * info.width;
    frame.y = (inSheet / columns) * info.height;
    frame.width = info.width;
    frame.height = info.height;
    return frame;
}

int trickplaySheets(const TrickplayInfo& info) {
    const int perSheet = std::max(1, info.tileWidth * info.tileHeight);
    return (std::max(0, info.thumbnailCount) + perSheet - 1) / perSheet;
}

std::optional<TrickplayInfo> pickTrickplay(const std::vector<TrickplayInfo>& widths) {
    std::optional<TrickplayInfo> best;
    for (const auto& info : widths) {
        const long long sheetW = static_cast<long long>(info.width) * info.tileWidth;
        const long long sheetH = static_cast<long long>(info.height) * info.tileHeight;
        if (sheetW > kMaxSheetSide || sheetH > kMaxSheetSide || sheetW * sheetH > kMaxSheetPixels) {
            continue;
        }
        if (!best) {
            best = info;
            continue;
        }
        const bool bigEnough = info.width >= 240;
        const bool bestBigEnough = best->width >= 240;
        if ((bigEnough && (!bestBigEnough || info.width < best->width)) || (!bigEnough && !bestBigEnough && info.width > best->width)) {
            best = info;
        }
    }
    return best;
}

}  // namespace playtube::player
