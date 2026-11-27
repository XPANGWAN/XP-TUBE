#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xp::core {

// Owns the configured roots for application data and package output.
// The caller chooses the package root for the target platform (for example,
// /data/pkg on PS4). This class performs lexical validation only; it does
// not resolve symlinks or create directories.
class StoragePaths {
public:
    StoragePaths(std::string data_root, std::string package_root);

    std::optional<std::string> data_file(std::string_view relative) const;
    std::optional<std::string> package_file(std::string_view name) const;

    const std::string& data_root() const noexcept { return data_root_; }
    const std::string& package_root() const noexcept { return package_root_; }

private:
    std::string data_root_;
    std::string package_root_;
};

} // namespace xp::core
