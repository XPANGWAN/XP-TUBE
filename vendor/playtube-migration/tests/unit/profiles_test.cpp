#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/profiles.hpp"
#include "core/data_dir.hpp"
#include "support/shell_harness.hpp"

using namespace playtube;

TEST_CASE("profiles: a folder per PS4 account; the sign-in made before them goes to the first one") {
    test::ShellDir dir;
    core::setDataRoot(dir.path);
    CHECK(app::profileFolder("") == "");
    CHECK(app::profileFolder("268435457") == "users/268435457");
    CHECK(app::profileFolder("../x") == "");  // digits only
    CHECK(*app::profileFile("", "account.json") == dir.path + "/account.json");  // Linux: as before
    std::ofstream(dir.path + "/account.json") << "{\"refresh\":\"r\"}";
    CHECK(app::onlyProfile("1"));
    const auto first = app::profileFile("1", "account.json");
    REQUIRE(first);
    CHECK(*first == dir.path + "/users/1/account.json");
    CHECK(std::filesystem::exists(*first));  // moved
    CHECK_FALSE(std::filesystem::exists(dir.path + "/account.json"));
    const auto second = app::profileFile("2", "account.json");
    REQUIRE(second);
    CHECK_FALSE(std::filesystem::exists(*second));  // a new account starts signed out
    CHECK(app::onlyProfile("1") == false);
    CHECK(app::profileFile("1", "account.json") == first);  // its own stays
    core::setDataRoot({});
}
