#include "xp/core/file_store.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

namespace xp::core {
namespace {

Error io_error(const char* operation, const std::string& path, int code = errno) {
    return Error::make(ErrorCode::io,
                       std::string(operation) + ": " + std::strerror(code),
                       path);
}

bool contains_nul(const std::string& value) {
    return value.find('\0') != std::string::npos;
}

void best_effort_sync_parent(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    std::string parent = slash == std::string::npos
        ? "."
        : (slash == 0 ? "/" : path.substr(0, slash));
#ifdef O_DIRECTORY
    const int directory_fd = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
#else
    const int directory_fd = ::open(parent.c_str(), O_RDONLY);
#endif
    if (directory_fd >= 0) {
        (void)::fsync(directory_fd);
        (void)::close(directory_fd);
    }
}

} // namespace

Result<std::string> read_file(const std::string& path, std::size_t max_bytes) {
    if (path.empty() || contains_nul(path)) {
        return Result<std::string>::failure(
            Error::make(ErrorCode::invalid_argument, "Invalid file path", path));
    }

    int flags = O_RDONLY | O_NONBLOCK;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int fd = ::open(path.c_str(), flags);
    if (fd < 0) {
        const int code = errno;
        if (code == ENOENT) {
            return Result<std::string>::failure(
                Error::make(ErrorCode::not_found, "File does not exist", path));
        }
        return Result<std::string>::failure(io_error("open", path, code));
    }

    struct stat info {};
    if (::fstat(fd, &info) != 0) {
        const int code = errno;
        (void)::close(fd);
        return Result<std::string>::failure(io_error("fstat", path, code));
    }
    if (!S_ISREG(info.st_mode) || info.st_size < 0) {
        (void)::close(fd);
        return Result<std::string>::failure(
            Error::make(ErrorCode::invalid_argument,
                        "Only regular files can be read", path));
    }

    std::string output;
    char block[16 * 1024];
    for (;;) {
        const ssize_t count = ::read(fd, block, sizeof(block));
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) {
            const int code = errno;
            (void)::close(fd);
            return Result<std::string>::failure(io_error("read", path, code));
        }
        if (count == 0) break;
        const std::size_t amount = static_cast<std::size_t>(count);
        if (amount > max_bytes - std::min(max_bytes, output.size())) {
            (void)::close(fd);
            return Result<std::string>::failure(
                Error::make(ErrorCode::invalid_argument,
                            "File exceeds the configured size limit", path));
        }
        output.append(block, amount);
    }

    if (::close(fd) != 0) {
        return Result<std::string>::failure(io_error("close", path));
    }
    return Result<std::string>::success(std::move(output));
}

Result<void> write_file_atomic(const std::string& path, std::string_view bytes) {
    if (path.empty() || contains_nul(path)) {
        return Result<void>::failure(
            Error::make(ErrorCode::invalid_argument, "Invalid file path", path));
    }
    if (bytes.size() > static_cast<std::size_t>(SSIZE_MAX)) {
        return Result<void>::failure(
            Error::make(ErrorCode::invalid_argument, "File is too large to write", path));
    }

    std::string temporary = path + ".tmp.XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back('\0');
    const int fd = ::mkstemp(name.data());
    if (fd < 0) {
        return Result<void>::failure(io_error("mkstemp", path));
    }
    temporary.assign(name.data());

    int failure_code = 0;
    bool ok = (::fchmod(fd, 0600) == 0);
    if (!ok) failure_code = errno;
    std::size_t offset = 0;
    while (ok && offset < bytes.size()) {
        const ssize_t count = ::write(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            failure_code = count == 0 ? EIO : errno;
            ok = false;
            break;
        }
        offset += static_cast<std::size_t>(count);
    }
    if (ok && ::fsync(fd) != 0) { failure_code = errno; ok = false; }
    const int close_result = ::close(fd);
    if (close_result != 0 && ok) { failure_code = errno; ok = false; }

    if (!ok) {
        const int code = failure_code == 0 ? EIO : failure_code;
        (void)::unlink(temporary.c_str());
        return Result<void>::failure(io_error("write temporary file", path, code));
    }
    if (::rename(temporary.c_str(), path.c_str()) != 0) {
        const int code = errno;
        (void)::unlink(temporary.c_str());
        return Result<void>::failure(io_error("rename temporary file", path, code));
    }
    best_effort_sync_parent(path);
    return Result<void>::success();
}

FileSink::FileSink(std::string final_path, std::string part_path, int fd,
                   long long initial_size)
    : final_path_(std::move(final_path)),
      part_path_(std::move(part_path)),
      fd_(fd),
      written_(initial_size) {
    buffer_.reserve(kBlockSize);
}

Result<std::unique_ptr<FileSink>> FileSink::open(const std::string& path,
                                                  bool resume) {
    if (path.empty() || contains_nul(path)) {
        return Result<std::unique_ptr<FileSink>>::failure(
            Error::make(ErrorCode::invalid_argument, "Invalid download path", path));
    }
    const std::string part = path + ".part";
    int flags = O_WRONLY | O_CREAT | O_NONBLOCK;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int fd = ::open(part.c_str(), flags, 0600);
    if (fd < 0) {
        return Result<std::unique_ptr<FileSink>>::failure(io_error("open part file", part));
    }

    struct stat info {};
    if (::fstat(fd, &info) != 0) {
        const int code = errno;
        (void)::close(fd);
        if (!resume) (void)::unlink(part.c_str());
        return Result<std::unique_ptr<FileSink>>::failure(io_error("fstat part file", part, code));
    }
    if (!S_ISREG(info.st_mode) || info.st_nlink != 1 || info.st_size < 0 ||
        static_cast<unsigned long long>(info.st_size) >
            static_cast<unsigned long long>(LLONG_MAX)) {
        (void)::close(fd);
        return Result<std::unique_ptr<FileSink>>::failure(
            Error::make(ErrorCode::invalid_argument,
                        "Part path must be a single-link regular file with a valid size",
                        part));
    }
    if (::fchmod(fd, 0600) != 0) {
        const int code = errno;
        (void)::close(fd);
        return Result<std::unique_ptr<FileSink>>::failure(io_error("fchmod", part, code));
    }
    if (!resume && ::ftruncate(fd, 0) != 0) {
        const int code = errno;
        (void)::close(fd);
        return Result<std::unique_ptr<FileSink>>::failure(io_error("truncate part file", part, code));
    }
    if (resume) {
        if (::lseek(fd, info.st_size, SEEK_SET) < 0) {
            const int code = errno;
            (void)::close(fd);
            return Result<std::unique_ptr<FileSink>>::failure(io_error("seek", part, code));
        }
    } else if (::lseek(fd, 0, SEEK_SET) < 0) {
        const int code = errno;
        (void)::close(fd);
        return Result<std::unique_ptr<FileSink>>::failure(io_error("seek", part, code));
    }

    auto sink = std::unique_ptr<FileSink>(
        new FileSink(path, part, fd, resume ? static_cast<long long>(info.st_size) : 0));
    return Result<std::unique_ptr<FileSink>>::success(std::move(sink));
}

FileSink::~FileSink() {
    if (fd_ >= 0) (void)::close(fd_);
    if (!keep_part_ && !committed_) (void)::unlink(part_path_.c_str());
}

Result<void> FileSink::fail(Error error) {
    if (fd_ >= 0) {
        (void)::close(fd_);
        fd_ = -1;
    }
    buffer_.clear();
    (void)::unlink(part_path_.c_str());
    keep_part_ = false;
    committed_ = false;
    return Result<void>::failure(std::move(error));
}

Result<void> FileSink::write_out(const char* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        const std::size_t remaining = size - offset;
        const std::size_t chunk = std::min(
            remaining, static_cast<std::size_t>(SSIZE_MAX));
        const ssize_t count = ::write(fd_, data + offset, chunk);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            const int code = count == 0 ? EIO : errno;
            return fail(io_error("write part file", part_path_, code));
        }
        offset += static_cast<std::size_t>(count);
    }
    return Result<void>::success();
}

Result<void> FileSink::flush() {
    if (fd_ < 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Download sink is closed", part_path_));
    }
    if (buffer_.empty()) return Result<void>::success();
    auto result = write_out(buffer_.data(), buffer_.size());
    if (result) buffer_.clear();
    return result;
}

Result<void> FileSink::write(const void* data, std::size_t size) {
    if (fd_ < 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Download sink is closed", part_path_));
    }
    if (size != 0 && data == nullptr) {
        return Result<void>::failure(
            Error::make(ErrorCode::invalid_argument, "Null data with non-zero size",
                        part_path_));
    }
    if (size > static_cast<std::size_t>(LLONG_MAX - written_)) {
        return fail(Error::make(ErrorCode::invalid_argument,
                                "Download size overflow", part_path_));
    }

    const char* bytes = static_cast<const char*>(data);
    std::size_t offset = 0;
    while (offset < size) {
        if (buffer_.size() == kBlockSize) {
            auto result = flush();
            if (!result) return result;
        }
        if (buffer_.empty() && size - offset >= kBlockSize) {
            const std::size_t direct = size - offset;
            auto result = write_out(bytes + offset, direct);
            if (!result) return result;
            offset += direct;
            written_ += static_cast<long long>(direct);
            continue;
        }
        const std::size_t room = kBlockSize - buffer_.size();
        const std::size_t amount = std::min(room, size - offset);
        buffer_.insert(buffer_.end(), bytes + offset, bytes + offset + amount);
        offset += amount;
        written_ += static_cast<long long>(amount);
    }
    return Result<void>::success();
}

Result<void> FileSink::restart() {
    if (fd_ < 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Download sink is closed", part_path_));
    }
    buffer_.clear();
    if (::ftruncate(fd_, 0) != 0 || ::lseek(fd_, 0, SEEK_SET) < 0) {
        return fail(io_error("restart part file", part_path_));
    }
    written_ = 0;
    return Result<void>::success();
}

Result<void> FileSink::keep() {
    if (fd_ < 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Download sink is closed", part_path_));
    }
    auto flushed = flush();
    if (!flushed) return flushed;
    if (::fsync(fd_) != 0) return fail(io_error("fsync part file", part_path_));
    if (::close(fd_) != 0) {
        const int code = errno;
        fd_ = -1;
        (void)::unlink(part_path_.c_str());
        return Result<void>::failure(io_error("close part file", part_path_, code));
    }
    fd_ = -1;
    keep_part_ = true;
    return Result<void>::success();
}

Result<void> FileSink::commit() {
    if (fd_ < 0) {
        return Result<void>::failure(
            Error::make(ErrorCode::io, "Download sink is closed", part_path_));
    }
    auto flushed = flush();
    if (!flushed) return flushed;
    if (::fsync(fd_) != 0) return fail(io_error("fsync part file", part_path_));
    if (::close(fd_) != 0) {
        const int code = errno;
        fd_ = -1;
        (void)::unlink(part_path_.c_str());
        return Result<void>::failure(io_error("close part file", part_path_, code));
    }
    fd_ = -1;
    if (::rename(part_path_.c_str(), final_path_.c_str()) != 0) {
        const int code = errno;
        (void)::unlink(part_path_.c_str());
        return Result<void>::failure(io_error("commit download", final_path_, code));
    }
    best_effort_sync_parent(final_path_);
    committed_ = true;
    keep_part_ = true;
    return Result<void>::success();
}

} // namespace xp::core
