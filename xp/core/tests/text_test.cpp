#include "xp/core/text.hpp"

#include <cassert>
#include <string>
#include <string_view>

int main() {
    using xp::core::ascii_lower;
    using xp::core::trim_ascii;

    assert(ascii_lower("XP-TUBE") == "xp-tube");
    assert(ascii_lower("MiXeD 123") == "mixed 123");
    assert(ascii_lower("\xC3\x89") == "\xC3\x89");
    assert(ascii_lower("").empty());

    assert(trim_ascii(" \t hello\r\n") == "hello");
    assert(trim_ascii(" \t\r\n").empty());
    assert(trim_ascii("no-trim") == "no-trim");
    assert(trim_ascii("  left") == "left");
    assert(trim_ascii("right  ") == "right");

    const std::string embedded{" a\0b ", 5};
    const auto trimmed = trim_ascii(std::string_view(embedded.data(), embedded.size()));
    assert(trimmed.size() == 3);
    assert(trimmed[1] == '\0');
}
