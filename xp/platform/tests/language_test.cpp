#include "xp/platform/language.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void expect(int id, const char* expected) {
    const auto actual = xp::platform::language_from_ps4_id(id);
    if (actual != expected) {
        std::cerr << "language id " << id << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}
}
int main() {
    expect(0, "ja-JP");
    expect(1, "en-US");
    expect(21, "ar");
    expect(30, "uk-UA");
    expect(-1, "en-US");
    expect(31, "en-US");
    return 0;
}
