#pragma once

#include "xp/platform/event.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace xp::platform {

// Converts already-normalized controller input into XP-owned events.
// Hardware discovery and polling remain the responsibility of the platform backend.
class ControllerInputTranslator {
public:
    static constexpr std::size_t kMaxDevices = 8;

    std::optional<Event> button(std::uint32_t device_id, ControllerButton button,
                                bool pressed, std::uint64_t timestamp_ms = 0) noexcept;

    // Stick axes emit ControllerAxisChanged. Trigger axes preserve the inherited
    // press (>16000) / release (<8000) hysteresis independently for each device.
    std::optional<Event> axis(std::uint32_t device_id, ControllerAxis axis,
                              std::int16_t value, std::uint64_t timestamp_ms = 0) noexcept;

    // Clears trigger state on disconnect so reconnects cannot inherit a held trigger.
    void disconnect(std::uint32_t device_id) noexcept;

private:
    struct DeviceState {
        std::uint32_t id{0};
        bool occupied{false};
        bool left_trigger_down{false};
        bool right_trigger_down{false};
    };
    DeviceState* stateFor(std::uint32_t device_id) noexcept;
    std::array<DeviceState, kMaxDevices> devices_{};
};

} // namespace xp::platform
