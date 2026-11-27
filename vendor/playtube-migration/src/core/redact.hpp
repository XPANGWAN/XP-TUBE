#pragma once

#include <string>
#include <string_view>

namespace playtube::core {

// Masks secrets (API keys, auth headers, passwords, access tokens) in a log line: each value becomes "***".
// Keys are matched case-insensitively.
std::string redact(std::string_view text);

}  // namespace playtube::core
