#pragma once

#include <string>
#include <string_view>

namespace xp::core {

// Lowercase ASCII A-Z only; preserve every other byte, including UTF-8.
std::string ascii_lower(std::string_view text);

// Remove ASCII space, tab, carriage return and line feed from both ends.
std::string_view trim_ascii(std::string_view text);

}  // namespace xp::core
