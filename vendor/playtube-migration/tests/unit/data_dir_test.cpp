#include <doctest/doctest.h>

#include <sys/stat.h>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/data_dir.hpp"

using playtube::core::resolveInside;

TEST_CASE("resolveInside joins plain relative paths") {
    CHECK(resolveInside("/data/Playtube", "playtube.log") == std::string("/data/Playtube/playtube.log"));
    CHECK(resolveInside("/data/Playtube/", "cache/img/a.png") == std::string("/data/Playtube/cache/img/a.png"));
}

TEST_CASE("resolveInside rejects anything that could leave the root") {
    const std::string root = "/data/Playtube";
    for (std::string_view bad : {"", "/etc/passwd", "../x", "a/../../x", "a/./b", "a//b", "a/", ".", "..",
                                 "a\\b", "..\\..\\x"}) {
        INFO("input: " << std::string(bad));
        CHECK_FALSE(resolveInside(root, bad).has_value());
    }
    CHECK_FALSE(resolveInside(root, std::string_view("a\0b", 3)).has_value());
    CHECK_FALSE(resolveInside("", "a").has_value());
}

TEST_CASE("dataPath uses the root set by the platform") {
    playtube::core::setDataRoot("/data/Playtube");
    CHECK(playtube::core::dataRoot() == "/data/Playtube");
    CHECK(playtube::core::dataPath("x.log") == std::string("/data/Playtube/x.log"));
    CHECK_FALSE(playtube::core::dataPath("../x.log").has_value());
}

TEST_CASE("makeDirs creates nested directories and accepts existing ones") {
    char tmpl[] = "/tmp/playtube-test-XXXXXX";
    REQUIRE(mkdtemp(tmpl) != nullptr);
    const std::string base = tmpl;
    CHECK(playtube::core::makeDirs(base + "/a/b/c"));
    CHECK(std::filesystem::is_directory(base + "/a/b/c"));
    CHECK(playtube::core::makeDirs(base + "/a/b/c"));
    std::filesystem::remove_all(base);
}

namespace {

// The PS4 sandbox refuses mkdir on existing system folders ("/", "/data") with a permission error, not EEXIST.
int refusingMkdir(const char* path, mode_t mode) {
    struct stat info {};
    if (stat(path, &info) == 0) {
        errno = EPERM;
        return -1;
    }
    return mkdir(path, mode);
}

}  // namespace

TEST_CASE("makeDirs skips existing folders even when mkdir refuses them") {
    char tmpl[] = "/tmp/playtube-test-XXXXXX";
    REQUIRE(mkdtemp(tmpl) != nullptr);
    const std::string base = tmpl;
    CHECK(playtube::core::makeDirsWith(base + "/Playtube/cache", refusingMkdir));
    CHECK(std::filesystem::is_directory(base + "/Playtube/cache"));
    std::filesystem::remove_all(base);
}

TEST_CASE("makeDirsWith gives the folders it creates the requested mode, whatever the umask") {
    char tmpl[] = "/tmp/playtube-test-XXXXXX";
    REQUIRE(mkdtemp(tmpl) != nullptr);
    const std::string base = tmpl;
    const mode_t previous = umask(022);
    CHECK(playtube::core::makeDirsWith(base + "/Playtube/cache", ::mkdir, 0777));
    umask(previous);
    struct stat info {};
    REQUIRE(stat((base + "/Playtube").c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0777);
    REQUIRE(stat((base + "/Playtube/cache").c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0777);
    std::filesystem::remove_all(base);
}
