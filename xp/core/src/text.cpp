#include "xp/core/text.hpp"

namespace xp::core {
namespace {

bool is_trim_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

}  // namespace

std::string ascii_lower(std::string_view text) {
    std::string result(text);
    for (char& value : result) {
        if (value >= 'A' && value <= 'Z') {
            value = static_cast<char>(value - 'A' + 'a');
        }
    }
    return result;
}

std::string_view trim_ascii(std::string_view text) {
    while (!text.empty() && is_trim_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_trim_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

}  // namespace xp::core
