#include "core/data_dir.hpp"

#include "xp/core/storage_context.hpp"
#include "xp/core/storage_paths.hpp"
#include "xp/core/directory_store.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <utility>

namespace playtube::core {

namespace {

xp::core::StorageContext gStorageContext;

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

void setDataRoot(std::string root) {
    if (root.empty()) {
        gStorageContext.reset();
        return;
    }
#ifdef PLAYTUBE_PS4
    // Preserve the PS4 Package Installer location; it is not inside app data.
    gStorageContext.configure(std::move(root), "/data/pkg");
#else
    std::string packageRoot = root;
    if (!packageRoot.empty() && packageRoot.back() != '/') packageRoot.push_back('/');
    packageRoot += "pkg";
    gStorageContext.configure(std::move(root), std::move(packageRoot));
#endif
}

const std::string& dataRoot() {
    return gStorageContext.data_root();
}

std::optional<std::string> dataPath(std::string_view relative) {
    return gStorageContext.data_file(relative);
}

std::optional<std::string> packagePath(std::string_view name) {
    if (!gStorageContext.configured()) return std::nullopt;
#ifdef PLAYTUBE_PS4
    // GoldHEN's folder: create one level under /data, without changing its mode.
    if (!xp::core::ensure_single_directory(gStorageContext.package_root(), 0777)) {
        return std::nullopt;
    }
#else
    if (!xp::core::ensure_directory_tree(gStorageContext.package_root(), 0755)) {
        return std::nullopt;
    }
#endif
    return gStorageContext.package_file(name);
}

std::optional<std::string> packageFolder() {
    if (!gStorageContext.configured()) return std::nullopt;
    const auto& folder = gStorageContext.package_root();
    return isDirectory(folder) ? std::optional<std::string>(folder) : std::nullopt;
}

bool makeDirs(const std::string& path) {
#ifdef PLAYTUBE_PS4
    // Existing PS4 data folders are opened up too, preserving the established permission policy.
    return xp::core::ensure_directory_tree(path, 0777, nullptr, true);
#else
    return xp::core::ensure_directory_tree(path, 0755);
#endif
}

bool makeDirsWith(const std::string& path, int (*makeDir)(const char*, mode_t), mode_t mode) {
    return xp::core::ensure_directory_tree(path, mode, makeDir);
}

}  // namespace playtube::core
