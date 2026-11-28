#include "xp/platform/controller_registry.hpp"

namespace xp::platform {

bool ControllerRegistry::connect(std::uint32_t instance_id) noexcept {
    if (contains(instance_id) || full()) return false;
    for (std::size_t i = 0; i < kCapacity; ++i) {
        if (!occupied_[i]) {
            ids_[i] = instance_id;
            occupied_[i] = true;
            ++count_;
            return true;
        }
    }
    return false;
}

bool ControllerRegistry::disconnect(std::uint32_t instance_id) noexcept {
    for (std::size_t i = 0; i < kCapacity; ++i) {
        if (occupied_[i] && ids_[i] == instance_id) {
            occupied_[i] = false;
            --count_;
            return true;
        }
    }
    return false;
}

bool ControllerRegistry::contains(std::uint32_t instance_id) const noexcept {
    for (std::size_t i = 0; i < kCapacity; ++i) {
        if (occupied_[i] && ids_[i] == instance_id) return true;
    }
    return false;
}

} // namespace xp::platform
