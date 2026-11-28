#include "xp/platform/controller_input.hpp"

namespace xp::platform {

std::optional<Event> ControllerInputTranslator::button(std::uint32_t device_id,
                                                        ControllerButton button_value,
                                                        bool pressed,
                                                        std::uint64_t timestamp_ms) noexcept {
    if (button_value == ControllerButton::Unknown) return std::nullopt;

    Event event;
    event.type = pressed ? EventType::ControllerButtonDown : EventType::ControllerButtonUp;
    event.timestamp_ms = timestamp_ms;
    event.device_id = device_id;
    event.button = button_value;
    return event;
}

std::optional<Event> ControllerInputTranslator::axis(std::uint32_t device_id,
                                                      ControllerAxis axis_value,
                                                      std::int16_t value,
                                                      std::uint64_t timestamp_ms) noexcept {
    if (axis_value == ControllerAxis::LeftTrigger || axis_value == ControllerAxis::RightTrigger) {
        bool& down = axis_value == ControllerAxis::LeftTrigger ? left_trigger_down_ : right_trigger_down_;
        if (!down && value > 16000) {
            down = true;
            return button(device_id, axis_value == ControllerAxis::LeftTrigger
                                         ? ControllerButton::L2 : ControllerButton::R2,
                          true, timestamp_ms);
        }
        if (down && value < 8000) {
            down = false;
            return button(device_id, axis_value == ControllerAxis::LeftTrigger
                                         ? ControllerButton::L2 : ControllerButton::R2,
                          false, timestamp_ms);
        }
        return std::nullopt;
    }

    Event event;
    event.type = EventType::ControllerAxisChanged;
    event.timestamp_ms = timestamp_ms;
    event.device_id = device_id;
    event.axis = axis_value;
    event.axis_value = value;
    return event;
}

} // namespace xp::platform
