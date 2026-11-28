#pragma once

#include "xp/platform/event.hpp"

#include <cstdint>
#include <optional>

namespace xp::platform {

// Converts already-normalized controller input into XP-owned events.
// Hardware discovery and polling remain the responsibility of the platform backend.
class ControllerInputTranslator {
public:
    std::optional<Event> button(std::uint32_t device_id, ControllerButton button,
                                bool pressed, std::uint64_t timestamp_ms = 0) noexcept;

    // Stick axes emit ControllerAxisChanged. Trigger axes preserve the inherited
    // press (>16000) / release (<8000) hysteresis and emit L2/R2 button events.
    std::optional<Event> axis(std::uint32_t device_id, ControllerAxis axis,
                              std::int16_t value, std::uint64_t timestamp_ms = 0) noexcept;

private:
    bool left_trigger_down_{false};
    bool right_trigger_down_{false};
};

} // namespace xp::platform
