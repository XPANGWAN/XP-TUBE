#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace playtube::ui {

// A picture made from a small SVG: premultiplied RGBA, `width` x `height`.
struct SvgPicture {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

// Draws the plain shapes of a small SVG (a logo) at `height` pixels, its width following the viewBox: <path> (M L H V
// C S Q T Z, absolute and relative) and <polygon> filled with a "#RRGGBB" colour (nonzero rule), in document order,
// anti-aliased. Anything it does not know inside a shape (a transform, an arc, a gradient fill) makes it give up:
// nothing rather than a wrong picture. Bounded: 64 KiB of text, 512 shapes, 50 000 points, 256 x 2048 pixels.
std::optional<SvgPicture> rasterizeSvg(std::string_view svg, int height);

}  // namespace playtube::ui
