#pragma once

#include "xp/core/result.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace xp::core {

// Reads only regular files. Symlink targets are not followed.
// A file larger than max_bytes is rejected rather than truncated.
Result<std::string> read_file(const std::string& path, std::size_t max_bytes);

// Replaces path atomically using a unique private temporary file in the same
// directory. The temporary file is mode 0600; directory fsync is best-effort.
Result<void> write_file_atomic(const std::string& path, std::string_view bytes);

class FileSink {
public:
    static constexpr std::size_t kBlockSize = 1024 * 1024;

    // Opens "<path>.part". Resume preserves existing regular-file bytes and
    // appends after them; otherwise the part file is truncated.
    static Result<std::unique_ptr<FileSink>> open(const std::string& path,
                                                   bool resume = false);

    ~FileSink();
    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    Result<void> write(const void* data, std::size_t size);
    Result<void> restart();
    Result<void> keep();
    Result<void> commit();

    long long written() const noexcept { return written_; }

private:
    FileSink(std::string final_path, std::string part_path, int fd,
             long long initial_size);

    Result<void> flush();
    Result<void> write_out(const char* data, std::size_t size);
    Result<void> fail(Error error);

    std::string final_path_;
    std::string part_path_;
    int fd_{-1};
    long long written_{0};
    std::vector<char> buffer_;
    bool keep_part_{false};
    bool committed_{false};
};

} // namespace xp::core
