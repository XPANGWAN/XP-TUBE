#pragma once

#include "xp/platform/event.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace xp::platform::compat {

// Transitional boundary for the inherited input action vocabulary.
// The XP platform layer owns the resulting event; no SDL or upstream headers
// are required. Unknown actions are deliberately ignored.
std::optional<Event> translate_legacy_action(std::string_view action_name,
                                             bool pressed,
                                             std::uint64_t timestamp_ms = 0) noexcept;

} // namespace xp::platform::compat
