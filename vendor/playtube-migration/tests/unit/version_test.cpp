#include <doctest/doctest.h>

#include <string>

#include "core/version.hpp"

TEST_CASE("version matches the CMake project version") {
    // one version: project(Playtube VERSION …) in CMakeLists.txt (the PS4 package's APP_VER is its "XX.YY")
    CHECK(std::string(playtube::core::version()) == PLAYTUBE_PROJECT_VERSION);
}

TEST_CASE("the development app never uses the release app's id or folder") {
#ifdef PLAYTUBE_DEV
    CHECK(std::string(playtube::core::titleId()) == "PLTB00001");
    CHECK(std::string(playtube::core::appFolder()) == "PlaytubeDev");
    CHECK(std::string(playtube::core::appTitle()) == "Playtube Dev");
#else
    CHECK(std::string(playtube::core::titleId()) == "PLTB00000");
    CHECK(std::string(playtube::core::appFolder()) == "Playtube");
    CHECK(std::string(playtube::core::appTitle()) == "Playtube");
#endif
}
