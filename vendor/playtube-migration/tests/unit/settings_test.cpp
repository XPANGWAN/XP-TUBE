#include <doctest/doctest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/settings.hpp"
#include "core/file_io.hpp"
#include "ui/theme.hpp"

using namespace playtube;

namespace {

struct TempDir {
    std::string path;
    TempDir() {
        char tmpl[] = "/tmp/playtube-settings-XXXXXX";
        REQUIRE(mkdtemp(tmpl) != nullptr);
        path = tmpl;
    }
    ~TempDir() { std::filesystem::remove_all(path); }
};

}  // namespace

TEST_CASE("writeFileAtomic writes a private file that readSmallFile reads back") {
    TempDir dir;
    const std::string path = dir.path + "/a.json";
    REQUIRE(core::writeFileAtomic(path, "hello"));
    std::string text;
    CHECK(core::readSmallFile(path, 1024, text) == core::ReadStatus::Ok);
    CHECK(text == "hello");
    struct stat info {};
    REQUIRE(::stat(path.c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
    CHECK(core::readSmallFile(dir.path + "/missing", 1024, text) == core::ReadStatus::Missing);
    CHECK(core::readSmallFile(path, 2, text) == core::ReadStatus::Failed);  // larger than allowed
}

TEST_CASE("settings round trip") {
    TempDir dir;
    const std::string path = dir.path + "/settings.json";
    app::Settings settings;
    settings.accent = "violet";
    settings.language = "de";
    settings.recentSearches = {"dune", "alien"};
    REQUIRE(app::saveSettings(path, settings));
    const app::Settings loaded = app::loadSettings(path);
    CHECK(loaded.accent == "violet");
    CHECK(loaded.language == "de");
    CHECK(loaded.recentSearches == std::vector<std::string>{"dune", "alien"});
}

TEST_CASE("missing settings give the defaults") {
    TempDir dir;
    const app::Settings settings = app::loadSettings(dir.path + "/settings.json");
    CHECK(settings.accent == "white");
    CHECK(settings.language.empty());
    CHECK(settings.recentSearches.empty());
}

TEST_CASE("damaged settings are kept aside and the defaults used") {
    TempDir dir;
    const std::string path = dir.path + "/settings.json";
    {
        std::ofstream out(path);
        out << "{not json";
    }
    const app::Settings settings = app::loadSettings(path);
    CHECK(settings.accent == "white");
    CHECK(std::filesystem::exists(path + ".bad"));
    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("out of range values fall back to the defaults") {
    TempDir dir;
    const std::string path = dir.path + "/settings.json";
    {
        std::ofstream out(path);
        out << R"({"version":1,"accent":7,"maxBitrate":-5,"language":"xx-unknown","recentSearches":["a",3,"b"]})";
    }
    const app::Settings settings = app::loadSettings(path);
    CHECK(settings.accent == "white");
    CHECK(settings.recentSearches == std::vector<std::string>{"a", "b"});
}

TEST_CASE("an unknown accent id gives YouTube red; an older choice still loads") {
    CHECK(std::string(app::accentById("violet").id) == "violet");
    CHECK(std::string(app::accentById("playstation").id) == "playstation");
    CHECK(std::string(app::accentById("nope").id) == "white");
    CHECK(std::string(app::accentById("").id) == "white");
}

TEST_CASE("recent searches: trimmed, newest first, no duplicates, at most 8") {
    app::Settings settings;
    app::addRecentSearch(settings, "   ");
    CHECK(settings.recentSearches.empty());
    app::addRecentSearch(settings, " a ");
    app::addRecentSearch(settings, "b");
    app::addRecentSearch(settings, "a");
    CHECK(settings.recentSearches == std::vector<std::string>{"a", "b"});
    for (int i = 0; i < 10; ++i) {
        app::addRecentSearch(settings, "t" + std::to_string(i));
    }
    REQUIRE(settings.recentSearches.size() == 8);
    CHECK(settings.recentSearches.front() == "t9");
    CHECK(settings.recentSearches.back() == "t2");
}

TEST_CASE("YouTube settings: defaults, round trip, and values out of range replaced") {
    TempDir dir;
    const std::string path = dir.path + "/settings.json";
    const app::Settings defaults;
    CHECK(defaults.maxHeight == 1080);
    CHECK(defaults.sponsorBlock);
    CHECK(defaults.sponsorCategories == std::vector<std::string>{"sponsor", "selfpromo", "interaction"});
    CHECK_FALSE(defaults.customTopics);
    CHECK(defaults.invidious.empty());

    app::Settings settings;
    settings.maxHeight = 720;
    settings.sponsorBlock = false;
    settings.sponsorCategories = {"intro", "outro"};
    settings.customTopics = true;
    settings.homeTopics = {"Cuisine", "Jardinage"};
    settings.invidious = "https://invidious.example";
    REQUIRE(app::saveSettings(path, settings));
    const app::Settings loaded = app::loadSettings(path);
    CHECK(loaded.maxHeight == 720);
    CHECK_FALSE(loaded.sponsorBlock);
    CHECK(loaded.sponsorCategories == std::vector<std::string>{"intro", "outro"});
    CHECK(loaded.customTopics);
    CHECK(loaded.homeTopics == std::vector<std::string>{"Cuisine", "Jardinage"});
    CHECK(loaded.invidious == "https://invidious.example");

    REQUIRE(core::writeFileAtomic(path, R"({"maxHeight":4320,"sponsorCategories":["sponsor","made-up"],)"
                                        R"("homeTopics":["", "  ", "Ok"],"invidious":"http://plain.example"})"));
    const app::Settings odd = app::loadSettings(path);
    CHECK(odd.maxHeight == 1080);
    CHECK(odd.sponsorCategories == std::vector<std::string>{"sponsor"});
    CHECK(odd.customTopics);
    CHECK(odd.homeTopics == std::vector<std::string>{"Ok"});
    CHECK(odd.invidious.empty());
}

TEST_CASE("an Invidious address is kept only over https, without its trailing slash") {
    CHECK(app::cleanInvidious("https://yewtu.be/") == "https://yewtu.be");
    CHECK(app::cleanInvidious("  https://inv.example/  ") == "https://inv.example");
    CHECK(app::cleanInvidious("http://inv.example").empty());
    CHECK(app::cleanInvidious("inv.example").empty());
    CHECK(app::cleanInvidious("https://").empty());
    CHECK(app::cleanInvidious("").empty());
}

TEST_CASE("home topics: trimmed, no duplicates, at most 12") {
    app::Settings settings;
    CHECK(app::addTopic(settings, "  Cuisine "));
    CHECK_FALSE(app::addTopic(settings, "Cuisine"));
    CHECK_FALSE(app::addTopic(settings, "   "));
    for (int i = 0; i < 20; ++i) {
        app::addTopic(settings, "Topic " + std::to_string(i));
    }
    CHECK(settings.homeTopics.size() == 12);
    CHECK(settings.homeTopics.front() == "Cuisine");
    CHECK(settings.customTopics);
}

TEST_CASE("Invidious addresses: no query, fragment or user part; the scheme in any case; a host") {
    CHECK(app::cleanInvidious("HTTPS://Inv.example") == "https://Inv.example");
    CHECK(app::cleanInvidious("https://inv.example/path") == "https://inv.example/path");
    CHECK(app::cleanInvidious("https://inv.example/?a=1").empty());
    CHECK(app::cleanInvidious("https://inv.example#x").empty());
    CHECK(app::cleanInvidious("https://user@inv.example").empty());
    CHECK(app::cleanInvidious("https:///path").empty());
}
