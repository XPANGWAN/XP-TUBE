#pragma once

#include "xp/platform/event.hpp"

#include <cstddef>
#include <deque>
#include <optional>

namespace xp::platform {

// A bounded, single-thread-owned queue. The PS4 event pump and host tests use
// the same overflow and shutdown semantics. Cross-thread producers must marshal
// events onto the owning thread until a synchronized backend is introduced.
class EventQueue {
public:
    explicit EventQueue(std::size_t capacity);

    bool push(const Event& event);
    std::optional<Event> poll();
    void close() noexcept;

    bool closed() const noexcept { return closed_; }
    std::size_t size() const noexcept { return events_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    std::size_t dropped() const noexcept { return dropped_; }

private:
    std::size_t capacity_;
    std::deque<Event> events_;
    std::size_t dropped_{0};
    bool closed_{false};
};

} // namespace xp::platform
