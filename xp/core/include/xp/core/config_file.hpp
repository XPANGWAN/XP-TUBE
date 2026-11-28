#pragma once

#include "xp/core/result.hpp"

#include <optional>
#include <string>

namespace xp::core {

// Application-owned configuration file operations. Missing files are not errors;
// reads are bounded, writes are atomic, and malformed files can be quarantined.
Result<std::optional<std::string>> read_config_file(const std::string& path,
                                                    std::size_t max_bytes);
Result<void> write_config_file(const std::string& path, const std::string& contents);
Result<void> quarantine_config_file(const std::string& path);

} // namespace xp::core
