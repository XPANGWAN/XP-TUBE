#include "xp/platform/display.hpp"

#include <limits>

namespace xp::platform {

namespace {
constexpr std::uint32_t kMaxDisplayWidth = 7680;
constexpr std::uint32_t kMaxDisplayHeight = 4320;
constexpr std::size_t kBytesPerPixel = 4;
}

bool DisplayInfo::valid() const noexcept {
    return width > 0 && height > 0 &&
           width <= kMaxDisplayWidth && height <= kMaxDisplayHeight &&
           mode != DisplayMode::Unknown;
}

bool PresentationFrame::valid() const noexcept {
    if (rgba == nullptr || width == 0 || height == 0 ||
        width > kMaxDisplayWidth || height > kMaxDisplayHeight) {
        return false;
    }

    if (width > std::numeric_limits<std::size_t>::max() / kBytesPerPixel) {
        return false;
    }
    const std::size_t minimum_stride = static_cast<std::size_t>(width) * kBytesPerPixel;
    if (row_stride < minimum_stride || height > std::numeric_limits<std::size_t>::max() / row_stride) {
        return false;
    }
    return byte_size >= row_stride * static_cast<std::size_t>(height);
}

} // namespace xp::platform
