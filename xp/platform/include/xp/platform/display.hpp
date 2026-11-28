#pragma once

#include <cstddef>
#include <cstdint>

namespace xp::platform {

enum class DisplayMode : std::uint8_t {
    Unknown,
    Windowed,
    Fullscreen
};

struct DisplayInfo {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t refresh_rate_millihz{0};
    DisplayMode mode{DisplayMode::Unknown};

    bool valid() const noexcept;
};

struct PresentationFrame {
    const std::uint8_t* rgba{nullptr};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::size_t row_stride{0};
    std::size_t byte_size{0};
    bool changed{false};

    bool valid() const noexcept;
};

} // namespace xp::platform
