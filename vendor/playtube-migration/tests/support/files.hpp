#pragma once

#include <fstream>
#include <sstream>
#include <string>

namespace playtube::test {

inline std::string fixturePath(const std::string& relative) { return std::string(PLAYTUBE_TEST_FIXTURES) + "/" + relative; }

inline std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

inline std::string readFixture(const std::string& relative) { return readFile(fixturePath(relative)); }

}  // namespace playtube::test
