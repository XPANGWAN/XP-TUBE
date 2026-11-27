#include "youtube/account.hpp"

#include <utility>

#include "core/file_io.hpp"
#include "core/json.hpp"
#include "core/log.hpp"

namespace playtube::youtube {

namespace json = core::json;

namespace {

constexpr std::size_t kMaxFileBytes = 1024 * 1024;
constexpr std::int64_t kRenewMargin = 60;  // seconds before the expiry

}  // namespace

Account::Account(std::string path) : path_(std::move(path)) {}

void Account::load() {
    std::string text;
    if (core::readSmallFile(path_, kMaxFileBytes, text) != core::ReadStatus::Ok) {
        return;
    }
    const auto doc = json::Document::parse(text);
    if (!doc) {
        PF_LOG_WARN("account: damaged file, signed out");
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    refreshToken_ = json::string(doc->root(), "refresh_token");
    name_ = json::string(doc->root(), "name");
    imported_ = json::boolean(doc->root(), "imported", false);
}

bool Account::signedIn() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !refreshToken_.empty();
}

std::string Account::name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_;
}

void Account::signIn(const Token& token, std::string name, std::int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!token.refreshToken.empty()) {
        refreshToken_ = token.refreshToken;
    }
    accessToken_ = token.accessToken;
    expiresAt_ = now + token.expiresIn - kRenewMargin;
    name_ = std::move(name);
    saveLocked();
}

void Account::setName(std::string name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (name_ != name) {
        name_ = std::move(name);
        saveLocked();
    }
}

core::Result<std::string> Account::accessToken(const Auth& auth, std::int64_t now, const core::CancelToken& cancel) {
    std::lock_guard<std::mutex> renewing(renewMutex_);
    std::string refreshToken;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (refreshToken_.empty()) {
            return core::makeError(core::ErrorKind::Unauthorized, "signed out");
        }
        if (!accessToken_.empty() && now < expiresAt_) {
            return accessToken_;
        }
        refreshToken = refreshToken_;
    }
    const auto renewed = auth.refresh(refreshToken, cancel);
    std::lock_guard<std::mutex> lock(mutex_);
    if (!renewed.ok()) {
        if (renewed.error().kind == core::ErrorKind::Unauthorized && refreshToken_ == refreshToken) {
            PF_LOG_WARN("account: sign-in refused, signed out");
            refreshToken_.clear();
            accessToken_.clear();
            name_.clear();
            saveLocked();
        }
        return renewed.error();
    }
    if (refreshToken_ != refreshToken) {  // signed out or in again meanwhile
        return core::makeError(core::ErrorKind::Unauthorized, "signed out");
    }
    accessToken_ = renewed.value().accessToken;
    expiresAt_ = now + renewed.value().expiresIn - kRenewMargin;
    if (!renewed.value().refreshToken.empty() && renewed.value().refreshToken != refreshToken_) {
        refreshToken_ = renewed.value().refreshToken;
        saveLocked();
    }
    return accessToken_;
}

void Account::invalidate(const std::string& accessToken) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (accessToken_ == accessToken) {
        accessToken_.clear();
    }
}

void Account::signOut(const Auth& auth) {
    const std::string refreshToken = forget();
    if (!refreshToken.empty()) {
        auth.revoke(refreshToken);
    }
}

std::string Account::forget() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string refreshToken = std::exchange(refreshToken_, {});
    accessToken_.clear();
    name_.clear();
    saveLocked();
    return refreshToken;
}

bool Account::lookedForOld() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return imported_;
}

bool Account::importFromOld(const std::string& oldConfigPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (imported_) {
        return false;
    }
    imported_ = true;
    std::string token;
    std::string text;
    if (refreshToken_.empty() && core::readSmallFile(oldConfigPath, kMaxFileBytes, text) == core::ReadStatus::Ok) {
        if (const auto doc = json::Document::parse(text)) {
            token = json::string(json::object(doc->root(), "setting"), "yt_refresh_token");
            if (token.empty()) {
                token = json::string(doc->root(), "yt_refresh_token");
            }
        }
    }
    if (!token.empty()) {
        refreshToken_ = token;
        PF_LOG_INFO("account: sign-in imported from the previous Playtube");
    }
    saveLocked();
    return !token.empty();
}

void Account::saveLocked() const {
    if (path_.empty()) {
        return;
    }
    json::Writer writer;
    writer.object();
    if (!refreshToken_.empty()) {
        writer.key("refresh_token").value(refreshToken_);
        writer.key("name").value(name_);
    }
    writer.key("imported").value(imported_);
    writer.end();
    if (!core::writeFileAtomic(path_, writer.str())) {
        PF_LOG_WARN("account: could not be saved");
    }
}

}  // namespace playtube::youtube
