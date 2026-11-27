#include "core/log.hpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "core/boot_trace.hpp"
#include "xp/core/logger.hpp"

namespace playtube::log {
namespace {
xp::core::Logger gLogger;

xp::core::LogLevel toNative(Level level) {
    switch (level) {
    case Level::Debug: return xp::core::LogLevel::debug;
    case Level::Info: return xp::core::LogLevel::info;
    case Level::Warn: return xp::core::LogLevel::warn;
    case Level::Error: return xp::core::LogLevel::error;
    }
    return xp::core::LogLevel::info;
}
Level fromNative(xp::core::LogLevel level) {
    switch (level) {
    case xp::core::LogLevel::debug: return Level::Debug;
    case xp::core::LogLevel::info: return Level::Info;
    case xp::core::LogLevel::warn: return Level::Warn;
    case xp::core::LogLevel::error: return Level::Error;
    }
    return Level::Info;
}
std::string format(const char* fmt, va_list args) {
    std::vector<char> buffer(256);
    va_list copy;
    va_copy(copy, args);
    const int needed = std::vsnprintf(buffer.data(), buffer.size(), fmt, copy);
    va_end(copy);
    if (needed < 0) return "[log format error]";
    const auto length = static_cast<std::size_t>(needed);
    if (length >= buffer.size()) {
        buffer.resize(length + 1);
        std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    }
    std::string message(buffer.data(), length);
    if (message.size() > kMaxMessage) {
        message.resize(kMaxMessage);
        message += "[truncated]";
    }
    return message;
}
} // namespace

void setLevel(Level level) { gLogger.set_level(toNative(level)); }
Level level() { return fromNative(gLogger.level()); }
void setStderr(bool enabled) { gLogger.set_stderr(enabled); }

bool openFile(const std::string& path, std::size_t maxBytes) {
    const bool opened = gLogger.open_file(path, maxBytes);
    if (!opened) {
        core::bootMark("log: open failed");
        return false;
    }
    core::bootMark("log: open");
    return true;
}
void closeFile() { gLogger.close_file(); }
std::string rotatedName(const std::string& path) { return xp::core::Logger::rotated_name(path); }

void write(Level logLevel, const char* fmt, ...) {
    if (logLevel < level()) return;
    va_list args;
    va_start(args, fmt);
    const std::string message = format(fmt, args);
    va_end(args);
    gLogger.write(toNative(logLevel), message);
#ifdef PLAYTUBE_PS4
    static int mirrored = 0;
    if (mirrored < 200) {
        ++mirrored;
        const std::string mirror = message.substr(0, kMaxMessage);
        core::bootMark(mirror.c_str());
    }
#endif
}
} // namespace playtube::log
