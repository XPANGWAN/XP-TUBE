#include "xp/platform/controller_registry.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using xp::platform::ControllerRegistry;
    ControllerRegistry registry;

    assert(registry.size() == 0);
    assert(!registry.contains(0));
    assert(!registry.disconnect(0));

    // Zero is a valid platform instance identifier.
    assert(registry.connect(0));
    assert(registry.contains(0));
    assert(registry.size() == 1);
    assert(!registry.connect(0)); // duplicate connection is ignored

    assert(registry.connect(42));
    assert(registry.connect(7));
    assert(registry.size() == 3);
    assert(registry.disconnect(42));
    assert(!registry.contains(42));
    assert(!registry.disconnect(42));
    assert(registry.size() == 2);

    for (std::uint32_t id = 100; id < 100 + ControllerRegistry::kCapacity - 2; ++id) {
        assert(registry.connect(id));
    }
    assert(registry.full());
    assert(registry.size() == ControllerRegistry::kCapacity);
    assert(!registry.connect(999)); // bounded capacity
    assert(registry.disconnect(0));
    assert(!registry.full());
    assert(registry.connect(999)); // freed slot is reusable
    assert(registry.size() == ControllerRegistry::kCapacity);
}
