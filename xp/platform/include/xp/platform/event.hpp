#pragma once

#include <cstdint>
#include <string>

namespace xp::platform {

enum class EventType : std::uint8_t {
    None,
    QuitRequested,
    AppSuspended,
    AppResumed,
    ControllerConnected,
    ControllerDisconnected,
    ControllerButtonDown,
    ControllerButtonUp,
    ControllerAxisChanged,
    DisplayChanged,
    AudioFocusLost,
    AudioFocusRestored,
    TextInput,
    TextDelete
};

enum class ControllerButton : std::uint8_t {
    Unknown, Confirm, Cancel, Options, Menu, Up, Down, Left, Right,
    L1, R1, L2, R2, L3, R3, Context, Touchpad
};

enum class ControllerAxis : std::uint8_t {
    LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger
};

struct Event {
    EventType type{EventType::None};
    std::uint64_t timestamp_ms{0};
    std::uint32_t device_id{0};
    ControllerButton button{ControllerButton::Unknown};
    ControllerAxis axis{ControllerAxis::LeftX};
    std::int16_t axis_value{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::string text;
};

} // namespace xp::platform
