#pragma once

#include "xp/platform/event.hpp"

namespace xp::platform {

// Hardware adapters implement this boundary. Implementations must translate
// native SDK/SDL events into XP-owned Event values and must not leak native
// headers or handles into the application layer.
class Backend {
public:
    virtual ~Backend() = default;

    virtual bool initialize() noexcept = 0;
    // Returns true only when one event was written to out_event.
    virtual bool poll_event(Event& out_event) noexcept = 0;
    virtual void shutdown() noexcept = 0;
};

} // namespace xp::platform
