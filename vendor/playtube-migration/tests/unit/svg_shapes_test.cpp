#include <doctest/doctest.h>

#include <string>

#include "support/files.hpp"
#include "ui/svg_shapes.hpp"

using playtube::ui::rasterizeSvg;
using playtube::ui::SvgPicture;

namespace {
// premultiplied RGBA at x, y
const std::uint8_t* at(const SvgPicture& p, int x, int y) {
    return p.pixels.data() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(p.width) + static_cast<std::size_t>(x)) * 4;
}
}  // namespace

TEST_CASE("svg: paths and polygons drawn at a height, in order, holes kept (nonzero rule), edges smooth") {
    const auto picture = rasterizeSvg(playtube::test::readFixture("svg/logo.svg"), 50);
    REQUIRE(picture);
    CHECK(picture->width == 200);
    CHECK(picture->height == 50);
    const auto* red = at(*picture, 5, 25);  // the rounded box's left side
    CHECK(red[0] == 255);
    CHECK(red[1] == 0);
    CHECK(red[3] == 255);
    const auto* white = at(*picture, 28, 25);  // the triangle over it
    CHECK(white[0] == 255);
    CHECK(white[1] == 255);
    CHECK(at(*picture, 85, 25)[3] == 255);    // the bar
    CHECK(at(*picture, 120, 25)[3] == 0);     // its hole (drawn the other way round)
    CHECK(at(*picture, 70, 25)[3] == 0);      // between the shapes
    CHECK(at(*picture, 0, 0)[3] < 255);       // a rounded corner: partly covered
    const auto half = rasterizeSvg(playtube::test::readFixture("svg/logo.svg"), 25);
    REQUIRE(half);
    CHECK(half->width == 100);
}

TEST_CASE("svg: what it cannot draw faithfully gives nothing") {
    CHECK_FALSE(rasterizeSvg("", 40));
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10 10\"></svg>", 40));                                      // no shape
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10 10\"><path d=\"M0,0 A5,5 0 0 1 10,10z\"/></svg>", 40));  // an arc
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10 10\"><path transform=\"scale(2)\" d=\"M0,0h5v5z\"/></svg>", 40));
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10 10\"><path fill=\"url(#g)\" d=\"M0,0h5v5z\"/></svg>", 40));
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10 10\"><path d=\"M0,0h5v5z\"/></svg>", 0));
    CHECK_FALSE(rasterizeSvg("<svg viewBox=\"0 0 10000 1\"><path d=\"M0,0h5v1z\"/></svg>", 40));  // far too wide
    CHECK(rasterizeSvg("<svg viewBox=\"0 0 10 10\"><path d=\"M0,0h5v5z\"/></svg>", 40));             // black by default
}
