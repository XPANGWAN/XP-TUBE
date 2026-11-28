#include "xp/platform/event_queue.hpp"

#include <cassert>

int main() {
    using namespace xp::platform;

    EventQueue queue(2);
    Event confirm;
    confirm.type = EventType::ControllerButtonDown;
    confirm.button = ControllerButton::Confirm;
    confirm.timestamp_ms = 42;

    Event resumed;
    resumed.type = EventType::AppResumed;

    assert(queue.capacity() == 2);
    assert(queue.size() == 0);
    assert(queue.push(confirm));
    assert(queue.push(resumed));
    assert(queue.size() == 2);

    Event overflow;
    overflow.type = EventType::QuitRequested;
    assert(!queue.push(overflow));
    assert(queue.dropped() == 1);

    const auto first = queue.poll();
    assert(first.has_value());
    assert(first->type == EventType::ControllerButtonDown);
    assert(first->button == ControllerButton::Confirm);
    assert(first->timestamp_ms == 42);

    const auto second = queue.poll();
    assert(second.has_value());
    assert(second->type == EventType::AppResumed);
    assert(!queue.poll().has_value());

    queue.close();
    assert(queue.closed());
    assert(!queue.push(overflow));
    assert(queue.dropped() == 2);
    assert(!queue.poll().has_value());

    EventQueue disabled(0);
    assert(!disabled.push(confirm));
    assert(disabled.dropped() == 1);
}
