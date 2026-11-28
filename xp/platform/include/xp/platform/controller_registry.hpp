#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xp::platform {

// Tracks controller instance identities without owning or polling the device.
// SDL/OpenOrbis remains the hardware owner during the staged migration.
class ControllerRegistry {
public:
    static constexpr std::size_t kCapacity = 8;

    bool connect(std::uint32_t instance_id) noexcept;
    bool disconnect(std::uint32_t instance_id) noexcept;
    bool contains(std::uint32_t instance_id) const noexcept;
    std::size_t size() const noexcept { return count_; }
    bool full() const noexcept { return count_ == kCapacity; }

private:
    std::array<std::uint32_t, kCapacity> ids_{};
    std::array<bool, kCapacity> occupied_{};
    std::size_t count_{0};
};

} // namespace xp::platform
