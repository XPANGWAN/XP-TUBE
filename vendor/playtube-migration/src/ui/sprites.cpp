#include "ui/sprites.hpp"

#include <nanovg.h>

#include <algorithm>
#include <cmath>

namespace playtube::ui {

std::vector<std::uint8_t> verticalFadeImage(int width, int height, int ramp, Color color, std::uint8_t from,
                                            std::uint8_t to) {
    width = std::max(width, 0);
    height = std::max(height, 0);
    if (width == 0 || height == 0) {
        return {};
    }
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    for (int y = 0; y < height; ++y) {
        const float t =
            ramp > 0 ? std::clamp((static_cast<float>(y) + 0.5f) / static_cast<float>(ramp), 0.0f, 1.0f) : 1.0f;
        const float alpha = (from + (to - from) * t) / 255.0f;
        const auto premultiplied = [alpha](std::uint8_t channel) {
            return static_cast<std::uint8_t>(std::lround(channel * alpha));
        };
        const std::uint8_t pixel[4] = {premultiplied(color.r), premultiplied(color.g), premultiplied(color.b),
                                       static_cast<std::uint8_t>(std::lround(alpha * 255))};
        std::uint8_t* row = &pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4];
        for (int x = 0; x < width; ++x) {
            std::copy(pixel, pixel + 4, row + x * 4);
        }
    }
    return pixels;
}

void drawVerticalFade(NVGcontext* vg, float x, float y, int width, int height, int ramp, Color color,
                      std::uint8_t from, std::uint8_t to) {
    if (width <= 0 || height <= 0) {
        return;
    }
    const float left = static_cast<float>(std::round(x));
    const float top = static_cast<float>(std::round(y));
    // A fade's rows are one colour each: solid rectangles, one per run of rows with the same alpha, on whole pixels
    // and without edge smoothing (no fringe: two runs never overlap). The software renderer blends a constant
    // colour several times faster than a translucent image.
    const auto alphaAt = [&](int row) {
        const float t = ramp > 0 ? std::clamp((static_cast<float>(row) + 0.5f) / static_cast<float>(ramp), 0.0f, 1.0f)
                                 : 1.0f;
        return static_cast<int>(std::lround(static_cast<float>(from) + static_cast<float>(to - from) * t));
    };
    nvgSave(vg);
    nvgShapeAntiAlias(vg, 0);
    int start = 0;
    while (start < height) {
        const int alpha = alphaAt(start);
        int end = start + 1;
        while (end < height && alphaAt(end) == alpha) {
            ++end;
        }
        if (alpha > 0) {
            nvgBeginPath(vg);
            nvgRect(vg, left, top + static_cast<float>(start), static_cast<float>(width),
                    static_cast<float>(end - start));
            nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, static_cast<unsigned char>(alpha)));
            nvgFill(vg);
        }
        start = end;
    }
    nvgRestore(vg);
}

}  // namespace playtube::ui
