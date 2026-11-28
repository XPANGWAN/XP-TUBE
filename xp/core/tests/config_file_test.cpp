#include "xp/core/config_file.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {
std::string contents(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}
void check(bool value) { if (!value) std::exit(1); }
}

int main() {
    char directory[] = "/tmp/xp-config-file-XXXXXX";
    check(::mkdtemp(directory) != nullptr);
    const std::string path = std::string(directory) + "/settings.json";

    auto missing = xp::core::read_config_file(path, 128);
    check(missing && !missing.value().has_value());

    check(static_cast<bool>(xp::core::write_config_file(path, "{\"version\":1}")));
    auto loaded = xp::core::read_config_file(path, 128);
    check(loaded && loaded.value().has_value());
    check(*loaded.value() == "{\"version\":1}");

    auto oversized = xp::core::read_config_file(path, 2);
    check(!oversized);

    check(static_cast<bool>(xp::core::quarantine_config_file(path)));
    check(::access(path.c_str(), F_OK) != 0);
    check(contents(path + ".bad") == "{\"version\":1}");

    std::remove((path + ".bad").c_str());
    ::rmdir(directory);
    return 0;
}
