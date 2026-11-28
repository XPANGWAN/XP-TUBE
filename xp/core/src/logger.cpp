#include "xp/core/logger.hpp"
#include "xp/core/redaction.hpp"

#include <cerrno>
#include <cstdio>
#include <ctime>
#include <string>
#include <time.h>

namespace xp::core {
namespace {
const char* tag(LogLevel level) {
    switch (level) {
    case LogLevel::debug: return "D";
    case LogLevel::info: return "I";
    case LogLevel::warn: return "W";
    case LogLevel::error: return "E";
    }
    return "?";
}
std::string timestamp() {
    timespec now{};
    clock_gettime(CLOCK_REALTIME, &now);
    std::tm local{};
#ifdef XP_TUBE_PLATFORM_PS4
    gmtime_r(&now.tv_sec, &local);
#else
    localtime_r(&now.tv_sec, &local);
#endif
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03ld",
                  local.tm_hour, local.tm_min, local.tm_sec, now.tv_nsec / 1000000);
    return buffer;
}
} // namespace

Logger::~Logger() { close_file(); }
void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}
LogLevel Logger::level() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return level_;
}
void Logger::set_stderr(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    stderr_enabled_ = enabled;
}
bool Logger::open_file(const std::string& path, std::size_t max_bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_) { std::fclose(file_); file_ = nullptr; }
    file_ = std::fopen(path.c_str(), "r+");
    if (!file_) file_ = std::fopen(path.c_str(), "w");
    if (!file_) return false;
    path_ = path;
    max_bytes_ = max_bytes;
    std::fseek(file_, 0, SEEK_END);
    const long size = std::ftell(file_);
    written_ = size > 0 ? static_cast<std::size_t>(size) : 0;
    return true;
}
void Logger::close_file() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_) { std::fclose(file_); file_ = nullptr; }
}
std::string Logger::rotated_name(const std::string& path) {
    const auto slash = path.rfind('/');
    const auto dot = path.rfind('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return path + ".1";
    return path.substr(0, dot) + ".1" + path.substr(dot);
}
void Logger::rotate_if_needed(std::size_t incoming) {
    if (!file_ || written_ == 0 || max_bytes_ == 0 || written_ + incoming <= max_bytes_) return;
    std::fclose(file_);
    file_ = nullptr;
    std::remove(rotated_name(path_).c_str());
    std::rename(path_.c_str(), rotated_name(path_).c_str());
    file_ = std::fopen(path_.c_str(), "w");
    written_ = 0;
}
void Logger::write(LogLevel level, std::string_view message) {
    const std::string safe = redact_secrets(message);
    const std::string line = timestamp() + " " + tag(level) + " " + safe + "\n";
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_) return;
    if (stderr_enabled_) std::fputs(line.c_str(), stderr);
    rotate_if_needed(line.size());
    if (file_) {
        std::fputs(line.c_str(), file_);
        std::fflush(file_);
        written_ += line.size();
    }
}
} // namespace xp::core
