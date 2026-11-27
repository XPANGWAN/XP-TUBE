#include "core/strings.hpp"

#include "xp/core/text.hpp"

namespace playtube::core {

std::string asciiLower(std::string_view text) {
    return xp::core::ascii_lower(text);
}

std::string_view trimmed(std::string_view text) {
    return xp::core::trim_ascii(text);
}

}  // namespace playtube::core
