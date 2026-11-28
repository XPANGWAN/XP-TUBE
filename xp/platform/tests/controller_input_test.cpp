#include "xp/platform/controller_input.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using namespace xp::platform;
    ControllerInputTranslator translator;

    const auto down = translator.button(17, ControllerButton::Confirm, true, 10);
    assert(down && down->type == EventType::ControllerButtonDown);
    assert(down->device_id == 17 && down->button == ControllerButton::Confirm);
    assert(down->timestamp_ms == 10);

    const auto up = translator.button(17, ControllerButton::Confirm, false, 11);
    assert(up && up->type == EventType::ControllerButtonUp);
    assert(!translator.button(17, ControllerButton::Unknown, true));

    const auto stick = translator.axis(17, ControllerAxis::LeftX, -12345, 20);
    assert(stick && stick->type == EventType::ControllerAxisChanged);
    assert(stick->device_id == 17 && stick->axis == ControllerAxis::LeftX);
    assert(stick->axis_value == -12345 && stick->timestamp_ms == 20);

    // Trigger hysteresis is isolated per controller.
    assert(!translator.axis(17, ControllerAxis::LeftTrigger, 16000, 30));
    const auto l2Down = translator.axis(17, ControllerAxis::LeftTrigger, 16001, 31);
    assert(l2Down && l2Down->type == EventType::ControllerButtonDown);
    assert(l2Down->button == ControllerButton::L2 && l2Down->device_id == 17);
    assert(!translator.axis(17, ControllerAxis::LeftTrigger, 9000, 32));
    assert(!translator.axis(17, ControllerAxis::LeftTrigger, 8000, 33));
    const auto l2Up = translator.axis(17, ControllerAxis::LeftTrigger, 7999, 34);
    assert(l2Up && l2Up->type == EventType::ControllerButtonUp);
    assert(l2Up->button == ControllerButton::L2);

    // A second device has independent trigger state and cannot release device 17.
    const auto r2Down = translator.axis(22, ControllerAxis::RightTrigger, 20000, 40);
    assert(r2Down && r2Down->button == ControllerButton::R2);
    assert(!translator.axis(22, ControllerAxis::RightTrigger, 9000, 41));
    assert(!translator.axis(17, ControllerAxis::RightTrigger, 9000, 42));
    const auto r2Up = translator.axis(22, ControllerAxis::RightTrigger, 0, 43);
    assert(r2Up && r2Up->type == EventType::ControllerButtonUp);

    // Disconnect clears held state; reconnect starts neutral.
    assert(translator.axis(17, ControllerAxis::LeftTrigger, 20000, 50));
    translator.disconnect(17);
    assert(!translator.axis(17, ControllerAxis::LeftTrigger, 9000, 51));
    const auto afterReconnect = translator.axis(17, ControllerAxis::LeftTrigger, 20000, 52);
    assert(afterReconnect && afterReconnect->type == EventType::ControllerButtonDown);

    // Fixed capacity is bounded; no ninth trigger state is allocated.
    ControllerInputTranslator bounded;
    for (std::uint32_t id = 1; id <= ControllerInputTranslator::kMaxDevices; ++id) {
        assert(bounded.axis(id, ControllerAxis::LeftTrigger, 1000, id));
    }
    assert(!bounded.axis(99, ControllerAxis::LeftTrigger, 20000, 99));
}
