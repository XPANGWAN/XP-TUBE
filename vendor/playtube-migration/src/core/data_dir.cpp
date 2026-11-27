#include "core/data_dir.hpp"

#include "xp/core/storage_paths.hpp"

#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <utility>

namespace playtube::core {

namespace {

std::string gDataRoot;

bool isDirectory(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
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
    if (!isDirectory(folder) && ::mkdir(folder.c_str(), 0777) != 0 && !isDirectory(folder)) {
        return std::nullopt;
    }
#else
    const auto inside = dataPath("pkg");
    if (!inside || !makeDirs(*inside)) {
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
    const bool ok = makeDirsWith(path, ::mkdir, 0777);
    if (ok) {
        chmod(path.c_str(), 0777);
    }
    return ok;
#else
    return makeDirsWith(path, ::mkdir);
#endif
}

bool makeDirsWith(const std::string& path, int (*makeDir)(const char*, mode_t), mode_t mode) {
    if (path.empty()) {
        return false;
    }
    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            const std::string prefix = path.substr(0, i);
            if (isDirectory(prefix)) {
                continue;  // never mkdir what exists: the PS4 sandbox refuses it for "/" and "/data" (not EEXIST)
            }
            if (makeDir(prefix.c_str(), mode) != 0) {
                if (errno != EEXIST) {
                    return false;
                }
            } else {
                chmod(prefix.c_str(), mode);  // the umask would narrow it
            }
        }
    }
    return isDirectory(path);
}

}  // namespace playtube::core
