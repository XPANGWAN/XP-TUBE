#pragma once

#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>

namespace xp::core {

enum class LogLevel { debug = 0, info, warn, error };

class Logger {
public:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void set_level(LogLevel level);
    LogLevel level() const;
    void set_stderr(bool enabled);
    bool open_file(const std::string& path, std::size_t max_bytes = 1024 * 1024);
    void close_file();
    void write(LogLevel level, std::string_view message);
    static std::string rotated_name(const std::string& path);

private:
    void rotate_if_needed(std::size_t incoming);
    mutable std::mutex mutex_;
    LogLevel level_{LogLevel::info};
    bool stderr_enabled_{true};
    std::FILE* file_{nullptr};
    std::string path_;
    std::size_t max_bytes_{0};
    std::size_t written_{0};
};

} // namespace xp::core
