#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/log.hpp"

namespace {

std::string makeTempDir() {
    char tmpl[] = "/tmp/playtube-log-XXXXXX";
    REQUIRE(mkdtemp(tmpl) != nullptr);
    return tmpl;
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Leaves the global logger as the other tests expect it
struct LogReset {
    LogReset() { playtube::log::setStderr(false); }
    ~LogReset() {
        playtube::log::closeFile();
        playtube::log::setLevel(playtube::log::Level::Info);
        playtube::log::setStderr(true);
    }
};

}  // namespace

TEST_CASE("rotatedName inserts .1 before the extension") {
    CHECK(playtube::log::rotatedName("/data/Playtube/playtube.log") == "/data/Playtube/playtube.1.log");
    CHECK(playtube::log::rotatedName("/a.b/log") == "/a.b/log.1");
}

TEST_CASE("log lines are written to the file with secrets masked") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/t.log"));
    playtube::log::setLevel(playtube::log::Level::Debug);
    PF_LOG_INFO("request %s", "/Items?api_key=abc123");
    playtube::log::closeFile();
    const std::string content = readFile(dir + "/t.log");
    CHECK(content.find(" I request /Items?api_key=***") != std::string::npos);
    CHECK(content.find("abc123") == std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("messages below the level are dropped") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/t.log"));
    playtube::log::setLevel(playtube::log::Level::Warn);
    PF_LOG_INFO("hidden line");
    PF_LOG_WARN("shown line");
    playtube::log::closeFile();
    const std::string content = readFile(dir + "/t.log");
    CHECK(content.find("hidden line") == std::string::npos);
    CHECK(content.find(" W shown line") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("the file rotates at the size limit, twice in a row") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/p.log", 300));
    for (int i = 0; i < 40; ++i) {
        PF_LOG_INFO("line %02d %s", i, "................................");
    }
    playtube::log::closeFile();
    CHECK(std::filesystem::exists(dir + "/p.1.log"));
    CHECK(std::filesystem::file_size(dir + "/p.log") <= 300);
    CHECK(std::filesystem::file_size(dir + "/p.1.log") <= 300);
    CHECK(readFile(dir + "/p.log").find("line 39") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("reopening an existing log keeps counting its size") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/r.log", 300));
    for (int i = 0; i < 4; ++i) {
        PF_LOG_INFO("first run %02d %s", i, "................................");
    }
    playtube::log::closeFile();
    REQUIRE(playtube::log::openFile(dir + "/r.log", 300));
    for (int i = 0; i < 4; ++i) {
        PF_LOG_INFO("second run %02d %s", i, "................................");
    }
    playtube::log::closeFile();
    CHECK(std::filesystem::file_size(dir + "/r.log") <= 300);
    std::filesystem::remove_all(dir);
}

TEST_CASE("an unwritable log file is reported and logging keeps working") {
    LogReset reset;
    CHECK_FALSE(playtube::log::openFile("/nonexistent-dir/playtube.log"));
    PF_LOG_ERROR("still fine %d", 1);
}

TEST_CASE("huge messages are truncated") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/big.log", 1024 * 1024));
    const std::string huge(100000, 'x');
    PF_LOG_INFO("%s", huge.c_str());
    playtube::log::closeFile();
    const std::string content = readFile(dir + "/big.log");
    CHECK(content.size() < playtube::log::kMaxMessage + 64);
    CHECK(content.find("[truncated]") != std::string::npos);
    std::filesystem::remove_all(dir);
}

TEST_CASE("reopening a log adds to the end and keeps the earlier lines") {
    LogReset reset;
    const std::string dir = makeTempDir();
    REQUIRE(playtube::log::openFile(dir + "/k.log"));
    PF_LOG_INFO("from the first run");
    playtube::log::closeFile();
    REQUIRE(playtube::log::openFile(dir + "/k.log"));
    PF_LOG_INFO("from the second run");
    playtube::log::closeFile();
    const std::string text = readFile(dir + "/k.log");
    const auto first = text.find("from the first run");
    const auto second = text.find("from the second run");
    REQUIRE(first != std::string::npos);
    REQUIRE(second != std::string::npos);
    CHECK(first < second);
    std::filesystem::remove_all(dir);
}
