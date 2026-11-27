#include "core/data_dir.hpp"

#include "xp/core/storage_paths.hpp"
#include "xp/core/directory_store.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <utility>

namespace playtube::core {

namespace {

std::string gDataRoot;

bool isDirectory(const std::string& path) {
    return xp::core::is_directory(path);
}

}  // namespace

std::optional<std::string> resolveInside(const std::string& root, std::string_view relative) {
    // Keep the legacy app-facing API stable while delegating path validation
    // to the XP-owned core implementation.
    const xp::core::StoragePaths paths(root, root);
    return paths.data_file(relative);
}

void setDataRoot(std::string root) { gDataRoot = std::move(root); }

const std::string& dataRoot() { return gDataRoot; }

std::optional<std::string> dataPath(std::string_view relative) { return resolveInside(gDataRoot, relative); }

std::optional<std::string> packagePath(std::string_view name) {
    if (name.find('/') != std::string_view::npos) {
        return std::nullopt;
    }
#ifdef PLAYTUBE_PS4
    // GoldHEN's folder: created when missing (one level, under /data), never changed (no chmod as for our own)
    const std::string folder = "/data/pkg";
    if (!xp::core::ensure_single_directory(folder, 0777)) {
        return std::nullopt;
    }
#else
    const auto inside = dataPath("pkg");
    if (!inside || !xp::core::ensure_directory_tree(*inside, 0755)) {
        return std::nullopt;
    }
    const std::string folder = *inside;
#endif
    return resolveInside(folder, name);
}

std::optional<std::string> packageFolder() {
#ifdef PLAYTUBE_PS4
    const std::string folder = "/data/pkg";
#else
    const auto inside = dataPath("pkg");
    if (!inside) {
        return std::nullopt;
    }
    const std::string folder = *inside;
#endif
    return isDirectory(folder) ? std::optional<std::string>(folder) : std::nullopt;
}

bool makeDirs(const std::string& path) {
#ifdef PLAYTUBE_PS4
    // a folder created under /data may not end up owned by the app's user: 0755 then forbids creating files in it
    // (seen on the first console runs). Existing folders, from earlier runs, are opened up too.
    return xp::core::ensure_directory_tree(path, 0777, nullptr, true);
#else
    return xp::core::ensure_directory_tree(path, 0755);
#endif
}

bool makeDirsWith(const std::string& path, int (*makeDir)(const char*, mode_t), mode_t mode) {
    return xp::core::ensure_directory_tree(path, mode, makeDir);
}

}  // namespace playtube::core
