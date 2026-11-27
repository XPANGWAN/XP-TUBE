#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "player/webp.hpp"
#include "support/files.hpp"
#include "ui/images.hpp"

using namespace playtube;

TEST_CASE("WebP (DeArrow's thumbnails): decoded by FFmpeg, and through the UI's picture decoder") {
    const std::string bytes = test::readFixture("dearrow/thumbnail.webp");
    REQUIRE(ui::isWebp(bytes));
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    REQUIRE(player::decodeWebp(bytes, width, height, rgba, 4096));
    CHECK(width == 64);
    CHECK(height == 36);
    CHECK(rgba.size() == 64u * 36u * 4u);
    CHECK_FALSE(player::decodeWebp(bytes, width, height, rgba, 32));  // past the limit
    CHECK_FALSE(player::decodeWebp(test::readFixture("youtube/search.json"), width, height, rgba, 4096));

    ui::setExtraDecoder({});
    CHECK_FALSE(ui::decodeImage(bytes, 32, 18));  // no decoder for it
    ui::setExtraDecoder([](const std::string& data, int& w, int& h, std::vector<std::uint8_t>& out) {
        return player::decodeWebp(data, w, h, out, ui::kMaxImageSide);
    });
    const auto picture = ui::decodeImage(bytes, 32, 18);
    REQUIRE(picture);
    CHECK(picture->width == 32);
    CHECK(picture->height == 18);
    CHECK(picture->opaque);
    ui::setExtraDecoder({});
}
