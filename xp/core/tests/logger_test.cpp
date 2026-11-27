#include "xp/core/logger.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {
std::string read(const std::string& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), {});
}
void check(bool ok) { if (!ok) std::exit(1); }
}
int main() {
    char dir[] = "/tmp/xp-logger-XXXXXX";
    check(mkdtemp(dir) != nullptr);
    const std::string path = std::string(dir) + "/app.log";
    xp::core::Logger logger;
    logger.set_stderr(false);
    check(logger.open_file(path, 180));
    logger.set_level(xp::core::LogLevel::debug);
    logger.write(xp::core::LogLevel::info, "request token=private-value");
    logger.close_file();
    auto content = read(path);
    check(content.find("token=***") != std::string::npos);
    check(content.find("private-value") == std::string::npos);
    check(xp::core::Logger::rotated_name("/data/app.log") == "/data/app.1.log");
    check(logger.open_file(path, 180));
    for (int i=0;i<20;++i) logger.write(xp::core::LogLevel::info, "line payload payload payload");
    logger.close_file();
    check(access((std::string(dir)+"/app.1.log").c_str(), F_OK) == 0);
    std::remove(path.c_str());
    std::remove((std::string(dir)+"/app.1.log").c_str());
    rmdir(dir);
    return 0;
}
