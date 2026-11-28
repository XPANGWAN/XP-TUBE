#include "xp/platform/compat/legacy_action.hpp"

namespace xp::platform::compat {

std::optional<Event> translate_legacy_action(std::string_view name,
                                             bool pressed,
                                             std::uint64_t timestamp_ms) noexcept {
    ControllerButton button = ControllerButton::Unknown;
    if (name == "Confirm") button = ControllerButton::Confirm;
    else if (name == "Back") button = ControllerButton::Cancel;
    else if (name == "Options") button = ControllerButton::Options;
    else if (name == "Menu") button = ControllerButton::Menu;
    else if (name == "Up") button = ControllerButton::Up;
    else if (name == "Down") button = ControllerButton::Down;
    else if (name == "Left") button = ControllerButton::Left;
    else if (name == "Right") button = ControllerButton::Right;
    else if (name == "L1") button = ControllerButton::L1;
    else if (name == "R1") button = ControllerButton::R1;
    else if (name == "L2") button = ControllerButton::L2;
    else if (name == "R2") button = ControllerButton::R2;
    else return std::nullopt;

    Event event;
    event.type = pressed ? EventType::ControllerButtonDown : EventType::ControllerButtonUp;
    event.timestamp_ms = timestamp_ms;
    event.button = button;
    return event;
}

} // namespace xp::platform::compat
