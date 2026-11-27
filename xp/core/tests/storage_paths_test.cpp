#include "xp/core/storage_paths.hpp"

#include <cassert>
#include <iostream>

int main() {
    const xp::core::StoragePaths paths("/data/XP-TUBE", "/data/pkg");

    const auto config = paths.data_file("settings/config.json");
    assert(config && *config == "/data/XP-TUBE/settings/config.json");
    assert(!paths.data_file("../outside"));
    assert(!paths.data_file("/absolute"));
    assert(!paths.data_file(""));

    const auto package = paths.package_file("update.pkg");
    assert(package && *package == "/data/pkg/update.pkg");
    assert(!paths.package_file("../update.pkg"));
    assert(!paths.package_file("nested/update.pkg"));
    assert(!paths.package_file(""));

    std::cout << "XP storage path tests passed\n";
    return 0;
}
