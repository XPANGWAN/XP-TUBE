#include "xp/core/storage_context.hpp"

#include <cassert>
#include <iostream>

int main() {
    xp::core::StorageContext context;
    assert(!context.configured());
    assert(context.data_root().empty());
    assert(context.package_root().empty());
    assert(!context.data_file("settings.json"));
    assert(!context.package_file("update.pkg"));

    context.configure("/data/XP-TUBE", "/data/pkg");
    assert(context.configured());
    assert(context.data_root() == "/data/XP-TUBE");
    assert(context.package_root() == "/data/pkg");

    const auto settings = context.data_file("settings/config.json");
    assert(settings && *settings == "/data/XP-TUBE/settings/config.json");
    const auto update = context.package_file("update.pkg");
    assert(update && *update == "/data/pkg/update.pkg");

    assert(!context.data_file("../outside"));
    assert(!context.package_file("nested/update.pkg"));

    // Reconfiguration replaces both roots as one coherent layout.
    context.configure("/tmp/xp-data", "/tmp/xp-data/pkg");
    assert(context.data_root() == "/tmp/xp-data");
    assert(context.package_root() == "/tmp/xp-data/pkg");
    const auto moved = context.data_file("state.json");
    assert(moved && *moved == "/tmp/xp-data/state.json");

    context.reset();
    assert(!context.configured());
    assert(context.data_root().empty());
    assert(!context.data_file("state.json"));

    std::cout << "XP storage context tests passed\n";
    return 0;
}
