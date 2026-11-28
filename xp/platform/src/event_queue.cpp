#include "xp/platform/event_queue.hpp"

#include <utility>

namespace xp::platform {

EventQueue::EventQueue(std::size_t capacity) : capacity_(capacity) {}

bool EventQueue::push(const Event& event) {
    if (closed_ || capacity_ == 0 || events_.size() >= capacity_) {
        ++dropped_;
        return false;
    }
    events_.push_back(event);
    return true;
}

std::optional<Event> EventQueue::poll() {
    if (events_.empty()) return std::nullopt;
    Event event = events_.front();
    events_.pop_front();
    return event;
}

void EventQueue::close() noexcept {
    closed_ = true;
    events_.clear();
}

} // namespace xp::platform
