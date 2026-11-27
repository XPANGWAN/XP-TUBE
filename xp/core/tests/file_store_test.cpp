#include "xp/core/file_store.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::string read_raw(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void write_raw(const std::string& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

struct TempDirectory {
    std::string path;
    TempDirectory() {
        char pattern[] = "/tmp/xp-core-storage-XXXXXX";
        char* result = ::mkdtemp(pattern);
        if (result) path = result;
    }
    ~TempDirectory() {
        if (!path.empty()) {
            const std::string command = "rm -rf '" + path + "'";
            (void)::system(command.c_str());
        }
    }
};

void test_read_and_atomic_write(const std::string& root) {
    const std::string path = root + "/settings.json";
    auto missing = xp::core::read_file(path, 1024);
    check(!missing && missing.error().code == xp::core::ErrorCode::not_found,
          "missing file reports not_found");

    auto written = xp::core::write_file_atomic(path, "{\"theme\":\"dark\"}");
    check(static_cast<bool>(written), "atomic write succeeds");
    auto read = xp::core::read_file(path, 1024);
    check(read && read.value() == "{\"theme\":\"dark\"}", "read returns exact contents");

    auto too_small = xp::core::read_file(path, 2);
    check(!too_small, "oversized file is rejected");

    struct stat info {};
    check(::stat(path.c_str(), &info) == 0, "atomic output exists");
    if (::stat(path.c_str(), &info) == 0) {
        check((info.st_mode & 0777) == 0600, "atomic output is private mode 0600");
    }

    auto replaced = xp::core::write_file_atomic(path, "new");
    check(static_cast<bool>(replaced), "atomic replacement succeeds");
    check(read_raw(path) == "new", "atomic replacement contains new bytes");
}

void test_file_sink_commit_and_resume(const std::string& root) {
    const std::string path = root + "/download.bin";
    {
        auto opened = xp::core::FileSink::open(path);
        check(static_cast<bool>(opened), "new download sink opens");
        if (opened) {
            const std::string first = "first-";
            const std::string second = "second";
            check(static_cast<bool>(opened.value()->write(first.data(), first.size())),
                  "first download chunk writes");
            check(static_cast<bool>(opened.value()->write(second.data(), second.size())),
                  "second download chunk writes");
            check(opened.value()->written() == 12, "written byte count is tracked");
            check(static_cast<bool>(opened.value()->commit()), "download commits");
        }
    }
    check(read_raw(path) == "first-second", "committed download has complete contents");
    check(::access((path + ".part").c_str(), F_OK) != 0, "committed part file is renamed away");

    const std::string resume_path = root + "/resume.bin";
    write_raw(resume_path + ".part", "prefix");
    {
        auto resumed = xp::core::FileSink::open(resume_path, true);
        check(static_cast<bool>(resumed), "existing part file resumes");
        if (resumed) {
            check(resumed.value()->written() == 6, "resume starts at existing byte count");
            const std::string suffix = "-tail";
            check(static_cast<bool>(resumed.value()->write(suffix.data(), suffix.size())),
                  "resume appends new bytes");
            check(static_cast<bool>(resumed.value()->commit()), "resumed download commits");
        }
    }
    check(read_raw(resume_path) == "prefix-tail", "resume preserves old bytes and appends");
}

void test_keep_restart_and_abandon(const std::string& root) {
    const std::string path = root + "/paused.bin";
    {
        auto opened = xp::core::FileSink::open(path);
        check(static_cast<bool>(opened), "pause sink opens");
        if (opened) {
            const std::string bytes = "pause-data";
            check(static_cast<bool>(opened.value()->write(bytes.data(), bytes.size())),
                  "pause data accepted");
            check(static_cast<bool>(opened.value()->keep()), "keep preserves part file");
        }
    }
    check(read_raw(path + ".part") == "pause-data", "kept part survives object destruction");

    {
        auto resumed = xp::core::FileSink::open(path, true);
        check(static_cast<bool>(resumed), "kept part can be reopened");
        if (resumed) {
            check(static_cast<bool>(resumed.value()->restart()), "restart truncates part");
            const std::string fresh = "fresh";
            check(static_cast<bool>(resumed.value()->write(fresh.data(), fresh.size())),
                  "write after restart succeeds");
            check(static_cast<bool>(resumed.value()->commit()), "restarted download commits");
        }
    }
    check(read_raw(path) == "fresh", "restart replaces prior partial bytes");

    const std::string abandoned = root + "/abandoned.bin";
    {
        auto opened = xp::core::FileSink::open(abandoned);
        check(static_cast<bool>(opened), "abandon sink opens");
        if (opened) {
            const std::string bytes = "temporary";
            (void)opened.value()->write(bytes.data(), bytes.size());
        }
    }
    check(::access((abandoned + ".part").c_str(), F_OK) != 0,
          "uncommitted and unkept part is removed");
}

void test_invalid_inputs_and_symlinks(const std::string& root) {
    auto invalid = xp::core::read_file("", 10);
    check(!invalid && invalid.error().code == xp::core::ErrorCode::invalid_argument,
          "empty read path is rejected");

    auto opened = xp::core::FileSink::open(root + "/null-data");
    check(static_cast<bool>(opened), "null-data sink opens");
    if (opened) {
        check(!opened.value()->write(nullptr, 1), "null pointer with bytes is rejected");
    }

    const std::string target = root + "/target";
    const std::string link = root + "/link";
    const std::string part_target = root + "/part-target";
    const std::string part_link = root + "/symlink-download.part";
    const std::string hard_target = root + "/hard-target";
    const std::string hard_part = root + "/hard-download.part";
    write_raw(target, "do-not-follow");
    if (::symlink(target.c_str(), link.c_str()) == 0) {
        auto symlink_read = xp::core::read_file(link, 100);
#ifdef O_NOFOLLOW
        check(!symlink_read, "read refuses a symbolic link when supported");
#else
        (void)symlink_read;
#endif
    }
    write_raw(part_target, "external");
    if (::symlink(part_target.c_str(), part_link.c_str()) == 0) {
        auto symlink_sink = xp::core::FileSink::open(root + "/symlink-download");
#ifdef O_NOFOLLOW
        check(!symlink_sink, "download sink refuses a symlink part file");
#else
        (void)symlink_sink;
#endif
        check(read_raw(part_target) == "external",
              "opening a symlink part never truncates its target");
    }
    write_raw(hard_target, "hard-linked-data");
    if (::link(hard_target.c_str(), hard_part.c_str()) == 0) {
        auto hardlink_sink = xp::core::FileSink::open(root + "/hard-download");
        check(!hardlink_sink, "download sink refuses multiply-linked part files");
        check(read_raw(hard_target) == "hard-linked-data",
              "opening a hard-linked part never truncates its other name");
    }
}

} // namespace

int main() {
    TempDirectory temporary;
    check(!temporary.path.empty(), "temporary directory created");
    if (temporary.path.empty()) return 1;

    test_read_and_atomic_write(temporary.path);
    test_file_sink_commit_and_resume(temporary.path);
    test_keep_restart_and_abandon(temporary.path);
    test_invalid_inputs_and_symlinks(temporary.path);

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All file storage tests passed\n";
    return 0;
}
