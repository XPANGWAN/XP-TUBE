#include "xp/core/config_file.hpp"

#include "xp/core/file_store.hpp"

#include <cstdio>
#include <utility>

namespace xp::core {

Result<std::optional<std::string>> read_config_file(const std::string& path,
                                                    std::size_t max_bytes) {
    auto loaded = read_file(path, max_bytes);
    if (loaded) {
        return Result<std::optional<std::string>>::success(
            std::optional<std::string>(std::move(loaded.value())));
    }
    if (loaded.error().code == ErrorCode::not_found) {
        return Result<std::optional<std::string>>::success(std::nullopt);
    }
    return Result<std::optional<std::string>>::failure(loaded.error());
}

Result<void> write_config_file(const std::string& path, const std::string& contents) {
    return write_file_atomic(path, contents);
}

Result<void> quarantine_config_file(const std::string& path) {
    if (path.empty()) {
        return Result<void>::failure(
            Error::make(ErrorCode::invalid_argument, "Invalid configuration path", path));
    }
    const std::string damaged = path + ".bad";
    if (std::rename(path.c_str(), damaged.c_str()) != 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Could not quarantine damaged configuration", path));
    }
    return Result<void>::success();
}

} // namespace xp::core
