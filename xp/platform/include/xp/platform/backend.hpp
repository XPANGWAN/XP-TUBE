#pragma once

#include "xp/platform/event.hpp"

#include <cstdint>

namespace xp::platform {

class Backend {
public:
    virtual ~Backend() = default;
    virtual bool initialize() noexcept = 0;
    virtual bool poll_event(Event& out_event, std::uint64_t timestamp_ms) noexcept = 0;
    virtual void shutdown() noexcept = 0;
};

} // namespace xp::platform
