#include "xp/platform/display.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    using namespace xp::platform;

    DisplayInfo display{1920, 1080, 60000, DisplayMode::Fullscreen};
    assert(display.valid());
    display.mode = DisplayMode::Unknown;
    assert(!display.valid());
    display = {7680, 4320, 60000, DisplayMode::Fullscreen};
    assert(display.valid());
    display.width = 7681;
    assert(!display.valid());

    std::array<std::uint8_t, 16> pixels{};
    PresentationFrame frame{pixels.data(), 2, 2, 8, pixels.size(), true};
    assert(frame.valid());

    frame.rgba = nullptr;
    assert(!frame.valid());
    frame.rgba = pixels.data();
    frame.row_stride = 7;
    assert(!frame.valid());
    frame.row_stride = 8;
    frame.byte_size = 15;
    assert(!frame.valid());
    frame.byte_size = 16;
    frame.width = 0;
    assert(!frame.valid());
}
