#pragma once

#include "xp/core/storage_paths.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace xp::core {

// Owns the configured application storage roots for the lifetime of the app.
// Platform startup configures the data root and package root once; legacy
// adapters can then resolve paths without keeping their own root globals.
class StorageContext {
public:
    void configure(std::string data_root, std::string package_root);
    void reset() noexcept;

    bool configured() const noexcept { return paths_.has_value(); }
    const std::string& data_root() const noexcept;
    const std::string& package_root() const noexcept;

    std::optional<std::string> data_file(std::string_view relative) const;
    std::optional<std::string> package_file(std::string_view name) const;

private:
    std::optional<StoragePaths> paths_;
};

} // namespace xp::core
