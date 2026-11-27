#include "xp/core/path.hpp"

#include <cassert>
#include <string>

int main() {
    using xp::core::resolve_inside;

    assert(resolve_inside("/data/XP-TUBE", "settings.json") ==
           std::optional<std::string>("/data/XP-TUBE/settings.json"));
    assert(resolve_inside("/data/XP-TUBE/", "cache/images/icon.png") ==
           std::optional<std::string>("/data/XP-TUBE/cache/images/icon.png"));

    assert(!resolve_inside("", "file"));
    assert(!resolve_inside("/data/XP-TUBE", ""));
    assert(!resolve_inside("/data/XP-TUBE", "/etc/passwd"));
    assert(!resolve_inside("/data/XP-TUBE", "../outside"));
    assert(!resolve_inside("/data/XP-TUBE", "cache/../outside"));
    assert(!resolve_inside("/data/XP-TUBE", "./settings"));
    assert(!resolve_inside("/data/XP-TUBE", "cache//file"));
    assert(!resolve_inside("/data/XP-TUBE", "cache/"));
    assert(!resolve_inside("/data/XP-TUBE", "cache\\\\file"));
    assert(!resolve_inside("/data/XP-TUBE", std::string_view("a\\0b", 3)));
}
