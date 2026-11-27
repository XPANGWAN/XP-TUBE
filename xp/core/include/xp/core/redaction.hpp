#pragma once

#include <string>
#include <string_view>

namespace xp::core {

// Masks credential values in diagnostic text. Matching is ASCII case-insensitive.
std::string redact_secrets(std::string_view text);

}  // namespace xp::core
