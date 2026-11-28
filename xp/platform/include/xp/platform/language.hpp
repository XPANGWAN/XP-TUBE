#pragma once

#include <string>

namespace xp::platform {
// Maps the numeric language identifier returned by the PS4 system service to
// a BCP-47 locale. Unknown identifiers intentionally fall back to en-US.
std::string language_from_ps4_id(int id);
} // namespace xp::platform
