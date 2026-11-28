#pragma once

#include <cstdint>

namespace xp::platform::ps4 {
bool initialize_system_services() noexcept;
int system_language_id() noexcept;
int utc_offset_minutes(std::int64_t utc_seconds) noexcept;
} // namespace xp::platform::ps4
