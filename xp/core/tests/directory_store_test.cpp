#include "xp/core/directory_store.hpp"

#include <sys/stat.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
int refusing_mkdir(const char* path, mode_t mode) {
    struct stat info {};
    if (::stat(path, &info) == 0) {
        errno = EPERM;
        return -1;
    }
    return ::mkdir(path, mode);
}
}

int main() {
    char tmpl[] = "/tmp/xp-directory-store-XXXXXX";
    assert(::mkdtemp(tmpl) != nullptr);
    const std::string root = tmpl;

    assert(!xp::core::is_directory(""));
    assert(xp::core::ensure_directory_tree(root + "/a/b/c"));
    assert(xp::core::is_directory(root + "/a/b/c"));
    assert(xp::core::ensure_directory_tree(root + "/a/b/c"));
    assert(xp::core::ensure_directory_tree(root + "/a/b/d", 0755, refusing_mkdir));

    const mode_t old_mask = ::umask(022);
    assert(xp::core::ensure_directory_tree(root + "/mode/sub", 0777));
    ::umask(old_mask);
    struct stat info {};
    assert(::stat((root + "/mode").c_str(), &info) == 0);
    assert((info.st_mode & 0777) == 0777);
    assert(::stat((root + "/mode/sub").c_str(), &info) == 0);
    assert((info.st_mode & 0777) == 0777);

    assert(xp::core::ensure_single_directory(root + "/pkg", 0777));
    assert(xp::core::ensure_single_directory(root + "/pkg", 0777, refusing_mkdir));
    assert(!xp::core::ensure_single_directory(""));

    std::filesystem::remove_all(root);
    std::cout << "XP directory storage tests passed\n";
}
