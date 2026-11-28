#include "xp/platform/runtime.hpp"

namespace xp::platform {

Runtime::Runtime(Backend& backend, std::size_t event_capacity) noexcept
    : backend_(backend), events_(event_capacity) {}

bool Runtime::start() noexcept {
    if (state_ == RuntimeState::Running) return true;
    if (state_ == RuntimeState::Failed) return false;

    if (!backend_.initialize()) {
        state_ = RuntimeState::Failed;
        return false;
    }
    state_ = RuntimeState::Running;
    return true;
}

bool Runtime::pump_once() noexcept {
    if (state_ != RuntimeState::Running) return false;

    Event event;
    if (!backend_.poll_event(event)) return false;
    return events_.push(event);
}

void Runtime::stop() noexcept {
    if (state_ == RuntimeState::Running) backend_.shutdown();
    events_.close();
    state_ = RuntimeState::Stopped;
}

} // namespace xp::platform
