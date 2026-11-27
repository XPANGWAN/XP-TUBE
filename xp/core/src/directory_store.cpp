#include "xp/core/directory_store.hpp"

#include <sys/stat.h>

#include <cerrno>

namespace xp::core {

namespace {
int default_make_directory(const char* path, mode_t mode) {
    return ::mkdir(path, mode);
}
}

bool is_directory(const std::string& path) {
    if (path.empty()) return false;
    struct stat info {};
    return ::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool ensure_directory_tree(const std::string& path, mode_t mode,
                           MakeDirectory make_directory,
                           bool normalize_existing_leaf) {
    if (path.empty()) return false;
    if (make_directory == nullptr) make_directory = default_make_directory;

    for (std::size_t i = 1; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != '/') continue;
        const std::string prefix = path.substr(0, i);
        if (is_directory(prefix)) continue;

        if (make_directory(prefix.c_str(), mode) != 0) {
            if (errno != EEXIST || !is_directory(prefix)) return false;
        } else {
            // mkdir is affected by umask; application-owned directories need
            // the exact mode requested by the platform policy.
            (void)::chmod(prefix.c_str(), mode);
        }
    }

    const bool exists = is_directory(path);
    if (exists && normalize_existing_leaf) (void)::chmod(path.c_str(), mode);
    return exists;
}

bool ensure_single_directory(const std::string& path, mode_t mode,
                             MakeDirectory make_directory) {
    if (path.empty()) return false;
    if (is_directory(path)) return true;
    if (make_directory == nullptr) make_directory = default_make_directory;
    if (make_directory(path.c_str(), mode) == 0) return is_directory(path);
    return errno == EEXIST && is_directory(path);
}

} // namespace xp::core
