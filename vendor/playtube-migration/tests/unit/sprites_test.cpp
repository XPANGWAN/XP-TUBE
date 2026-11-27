#include <doctest/doctest.h>

#include <nanovg.h>

#include <cstdlib>

#include "render/nvg_sw.hpp"
#include "render/raster.hpp"
#include "ui/images.hpp"
#include "ui/sprites.hpp"

using namespace playtube;

TEST_CASE("a vertical fade: premultiplied, from transparent to its alpha over the ramp, even below") {
    const ui::Color ground{16, 32, 64, 255};
    const auto pixels = ui::verticalFadeImage(3, 10, 4, ground, 0, 200);
    REQUIRE(pixels.size() == 3 * 10 * 4);
    const auto alphaAt = [&](std::size_t x, std::size_t y) { return pixels[(y * 3 + x) * 4 + 3]; };
    CHECK(alphaAt(0, 0) == 25);   // the first row's centre: 1/8 of the way
    CHECK(alphaAt(2, 0) == 25);   // every column alike
    CHECK(alphaAt(0, 3) == 175);  // 7/8
    CHECK(alphaAt(0, 4) == 200);  // past the ramp
    CHECK(alphaAt(0, 9) == 200);
    const std::size_t last = (9 * 3) * 4;
    CHECK(pixels[last] == 13);      // 16 * 200 / 255
    CHECK(pixels[last + 1] == 25);  // 32 * 200 / 255
    CHECK(pixels[last + 2] == 50);  // 64 * 200 / 255
    CHECK(ui::verticalFadeImage(0, 5, 2, ground, 0, 255).empty());
}


TEST_CASE("a vertical fade is drawn as plain rows: the picture's pixels, without the image path") {
    // a fade's rows are one colour each: drawn as solid rectangles they take the renderer's constant-colour path,
    // several times cheaper than a translucent image (the player's shade is half a million pixels a frame)
    render::Surface surface{160, 120};
    NVGcontext* vg = render::createSoftwareCanvas(surface);
    const ui::Color ground{7, 7, 8, 255};
    const int width = 100;
    const int height = 60;
    const int ramp = 40;
    const auto expected = ui::verticalFadeImage(width, height, ramp, ground, 20, 220);
    const auto pixelAt = [&](int x, int y) {
        return &surface.rgba[(static_cast<std::size_t>(y) * 160 + static_cast<std::size_t>(x)) * 4];
    };

    surface.clear(0, 0, 0, 0);  // over the video: nothing under the fade
    render::resetRasterStats();
    nvgBeginFrame(vg, 160, 120, 1);
    ui::drawVerticalFade(vg, 30, 20, width, height, ramp, ground, 20, 220);
    nvgEndFrame(vg);
    const render::RasterStats stats = render::rasterStats();
    CHECK(stats.imaged == 0);
    CHECK(stats.shaded == 0);
    CHECK(stats.pixels == width * height);
    for (int y = 0; y < height; ++y) {
        for (int x : {0, width / 2, width - 1}) {
            const std::uint8_t* got = pixelAt(30 + x, 20 + y);
            const std::uint8_t* want = &expected[(static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4];
            for (int k = 0; k < 4; ++k) {
                CHECK(std::abs(got[k] - want[k]) <= 1);
            }
        }
    }
    // nothing outside its rectangle
    CHECK(pixelAt(29, 40)[3] == 0);
    CHECK(pixelAt(130, 40)[3] == 0);
    CHECK(pixelAt(60, 19)[3] == 0);
    CHECK(pixelAt(60, 80)[3] == 0);

    // over a picture: as the fade blended on it (no seam between two rows)
    surface.clear(200, 100, 50, 255);
    nvgBeginFrame(vg, 160, 120, 1);
    ui::drawVerticalFade(vg, 30, 20, width, height, ramp, ground, 20, 220);
    nvgEndFrame(vg);
    for (int y = 0; y < height; ++y) {
        const std::uint8_t* got = pixelAt(60, 20 + y);
        const std::uint8_t* src = &expected[static_cast<std::size_t>(y) * width * 4];
        const int back[3] = {200, 100, 50};
        for (int k = 0; k < 3; ++k) {
            const int want = src[k] + back[k] * (255 - src[3]) / 255;
            CHECK(std::abs(got[k] - want) <= 2);
        }
        CHECK(got[3] == 255);
    }
    render::destroySoftwareCanvas(vg);
}

TEST_CASE("a backdrop's shade is baked in: as the shade blended over it, then the ground below 70 %") {
    const int width = 2;
    const int height = 10;  // split at row 7
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width * height * 4), 200);
    ui::shadeBackdrop(rgba, width, height, 14, 14, 17);
    CHECK(ui::backdropSplit(height) == 7);
    const auto at = [&](std::size_t y, std::size_t c) { return static_cast<int>(rgba[y * width * 4 + c]); };
    // row 0: alpha (143 + 112 * 0.5 / 7) / 255 = 0.592: 200 * 0.408 + 14 * 0.592 = 89.9
    CHECK(at(0, 0) == doctest::Approx(90).epsilon(0.02));
    CHECK(at(0, 2) == doctest::Approx(92).epsilon(0.02));
    CHECK(at(6, 0) < at(0, 0));  // darker down the gradient
    CHECK(at(7, 0) == 14);       // the ground from the split
    CHECK(at(9, 2) == 17);
    for (std::size_t i = 3; i < rgba.size(); i += 4) {
        CHECK(rgba[i] == 255);  // opaque: drawn as a copy of rows
    }
}
