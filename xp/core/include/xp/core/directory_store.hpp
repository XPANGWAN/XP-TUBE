#pragma once

#include <sys/types.h>

#include <string>

namespace xp::core {

using MakeDirectory = int (*)(const char*, mode_t);

// Filesystem directory operations used by the application storage layer.
// Callers must pass trusted application-owned paths.
bool is_directory(const std::string& path);
bool ensure_directory_tree(const std::string& path, mode_t mode = 0755,
                           MakeDirectory make_directory = nullptr,
                           bool normalize_existing_leaf = false);
bool ensure_single_directory(const std::string& path, mode_t mode = 0755,
                             MakeDirectory make_directory = nullptr);

} // namespace xp::core
