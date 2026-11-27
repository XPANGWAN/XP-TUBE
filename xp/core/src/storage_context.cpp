#include "xp/core/storage_context.hpp"

#include <utility>

namespace xp::core {

void StorageContext::configure(std::string data_root, std::string package_root) {
    paths_.emplace(std::move(data_root), std::move(package_root));
}

void StorageContext::reset() noexcept {
    paths_.reset();
}

const std::string& StorageContext::data_root() const noexcept {
    static const std::string empty;
    return paths_ ? paths_->data_root() : empty;
}

const std::string& StorageContext::package_root() const noexcept {
    static const std::string empty;
    return paths_ ? paths_->package_root() : empty;
}

std::optional<std::string> StorageContext::data_file(std::string_view relative) const {
    if (!paths_) return std::nullopt;
    return paths_->data_file(relative);
}

std::optional<std::string> StorageContext::package_file(std::string_view name) const {
    if (!paths_) return std::nullopt;
    return paths_->package_file(name);
}

} // namespace xp::core
