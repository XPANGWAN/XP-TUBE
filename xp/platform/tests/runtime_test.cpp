#include "xp/platform/runtime.hpp"

#include <cassert>
#include <deque>

namespace {
class FakeBackend final : public xp::platform::Backend {
public:
    bool initialize() noexcept override {
        ++initialize_calls;
        return initialize_ok;
    }

    bool poll_event(xp::platform::Event& out_event) noexcept override {
        if (pending.empty()) return false;
        out_event = pending.front();
        pending.pop_front();
        return true;
    }

    void shutdown() noexcept override { ++shutdown_calls; }

    bool initialize_ok{true};
    int initialize_calls{0};
    int shutdown_calls{0};
    std::deque<xp::platform::Event> pending;
};
}

int main() {
    using namespace xp::platform;

    FakeBackend backend;
    Runtime runtime(backend, 2);
    assert(runtime.state() == RuntimeState::Stopped);
    assert(!runtime.pump_once());
    assert(runtime.start());
    assert(runtime.start());
    assert(backend.initialize_calls == 1);

    Event first;
    first.type = EventType::ControllerButtonDown;
    first.button = ControllerButton::Confirm;
    backend.pending.push_back(first);

    Event second;
    second.type = EventType::AppResumed;
    backend.pending.push_back(second);

    assert(runtime.pump_once());
    assert(runtime.pump_once());
    assert(!runtime.pump_once());

    const auto received_first = runtime.events().poll();
    const auto received_second = runtime.events().poll();
    assert(received_first && received_first->button == ControllerButton::Confirm);
    assert(received_second && received_second->type == EventType::AppResumed);

    runtime.stop();
    assert(runtime.state() == RuntimeState::Stopped);
    assert(backend.shutdown_calls == 1);
    assert(!runtime.pump_once());
    assert(!runtime.start());
    assert(backend.initialize_calls == 1);

    FakeBackend failing_backend;
    failing_backend.initialize_ok = false;
    Runtime failing_runtime(failing_backend, 1);
    assert(!failing_runtime.start());
    assert(failing_runtime.state() == RuntimeState::Failed);
    assert(!failing_runtime.start());
    assert(failing_backend.shutdown_calls == 0);
    failing_runtime.stop();
    assert(failing_runtime.state() == RuntimeState::Stopped);

    FakeBackend overflow_backend;
    Runtime overflow_runtime(overflow_backend, 1);
    assert(overflow_runtime.start());
    Event queued;
    queued.type = EventType::AppSuspended;
    Event overflow;
    overflow.type = EventType::AppResumed;
    overflow_backend.pending.push_back(queued);
    overflow_backend.pending.push_back(overflow);
    assert(overflow_runtime.pump_once());
    assert(!overflow_runtime.pump_once());
    assert(overflow_runtime.events().size() == 1);
    assert(overflow_runtime.events().dropped() == 1);
    const auto retained = overflow_runtime.events().poll();
    assert(retained && retained->type == EventType::AppSuspended);
    overflow_runtime.stop();
}
