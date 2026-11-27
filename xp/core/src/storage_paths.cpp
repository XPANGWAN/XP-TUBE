#include "xp/core/storage_paths.hpp"

#include "xp/core/path.hpp"

#include <utility>

namespace xp::core {

StoragePaths::StoragePaths(std::string data_root, std::string package_root)
    : data_root_(std::move(data_root)),
      package_root_(std::move(package_root)) {}

std::optional<std::string> StoragePaths::data_file(
    std::string_view relative) const {
    return resolve_inside(data_root_, relative);
}

std::optional<std::string> StoragePaths::package_file(
    std::string_view name) const {
    // Package output is deliberately a single filename, never a subpath.
    if (name.find('/') != std::string_view::npos ||
        name.find('\\') != std::string_view::npos) {
        return std::nullopt;
    }
    return resolve_inside(package_root_, name);
}

} // namespace xp::core
