#include "xp/platform/compat/legacy_action.hpp"

#include <cassert>

int main() {
    using namespace xp::platform;
    using xp::platform::compat::translate_legacy_action;

    const auto confirm = translate_legacy_action("Confirm", true, 42);
    assert(confirm);
    assert(confirm->type == EventType::ControllerButtonDown);
    assert(confirm->button == ControllerButton::Confirm);
    assert(confirm->timestamp_ms == 42);

    const auto back = translate_legacy_action("Back", false, 99);
    assert(back);
    assert(back->type == EventType::ControllerButtonUp);
    assert(back->button == ControllerButton::Cancel);
    assert(back->timestamp_ms == 99);

    const auto menu = translate_legacy_action("Menu", true);
    assert(menu && menu->button == ControllerButton::Menu);

    const auto context = translate_legacy_action("Context", true);
    assert(context && context->button == ControllerButton::Context);

    const auto touchpad = translate_legacy_action("Touchpad", false);
    assert(touchpad && touchpad->type == EventType::ControllerButtonUp);
    assert(touchpad->button == ControllerButton::Touchpad);

    assert(!translate_legacy_action("unrecognized", true));
}
