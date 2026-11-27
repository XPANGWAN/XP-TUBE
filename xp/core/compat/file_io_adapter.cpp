#include "core/file_io.hpp"

#include "xp/core/file_store.hpp"

#include <utility>

namespace playtube::core {

ReadStatus readSmallFile(const std::string& path, std::size_t maxBytes, std::string& out) {
    auto result = xp::core::read_file(path, maxBytes);
    if (!result) {
        return result.error().code == xp::core::ErrorCode::not_found
            ? ReadStatus::Missing
            : ReadStatus::Failed;
    }
    out = std::move(result.value());
    return ReadStatus::Ok;
}

bool writeFileAtomic(const std::string& path, std::string_view text) {
    return static_cast<bool>(xp::core::write_file_atomic(path, text));
}

FileSink::FileSink(std::string path, bool resume) {
    auto opened = xp::core::FileSink::open(path, resume);
    if (opened) {
        impl_ = std::move(opened.value());
        active_ = true;
    }
}

FileSink::~FileSink() = default;

bool FileSink::write(const void* data, std::size_t size) {
    if (!active_ || !impl_) return false;
    auto result = impl_->write(data, size);
    if (!result) active_ = false;
    return static_cast<bool>(result);
}

bool FileSink::commit() {
    if (!active_ || !impl_) return false;
    auto result = impl_->commit();
    active_ = false;
    return static_cast<bool>(result);
}

void FileSink::keep() {
    if (!active_ || !impl_) return;
    (void)impl_->keep();
    active_ = false;
}

bool FileSink::restart() {
    if (!active_ || !impl_) return false;
    auto result = impl_->restart();
    if (!result) active_ = false;
    return static_cast<bool>(result);
}

long long FileSink::written() const {
    return impl_ ? impl_->written() : 0;
}

}  // namespace playtube::core
