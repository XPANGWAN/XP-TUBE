#pragma once

#include "xp/platform/backend.hpp"
#include "xp/platform/event_queue.hpp"

#include <cstddef>

namespace xp::platform {

enum class RuntimeState {
    Stopped,
    Running,
    Failed
};

// Coordinates a backend and the application-facing event queue. The runtime
// is single-thread-owned; call start/pump_once/stop from the same event thread.
class Runtime {
public:
    Runtime(Backend& backend, std::size_t event_capacity) noexcept;

    bool start() noexcept;
    bool pump_once() noexcept;
    void stop() noexcept;

    RuntimeState state() const noexcept { return state_; }
    EventQueue& events() noexcept { return events_; }
    const EventQueue& events() const noexcept { return events_; }

private:
    Backend& backend_;
    EventQueue events_;
    RuntimeState state_{RuntimeState::Stopped};
};

} // namespace xp::platform
