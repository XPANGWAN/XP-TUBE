#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "core/result.hpp"
#include "core/task.hpp"
#include "youtube/auth.hpp"

namespace playtube::youtube {

// The signed-in account: the refresh token and the account name in account.json (private, written atomically);
// the access token stays in memory. Thread-safe.
class Account {
public:
    explicit Account(std::string path);  // empty: kept in memory only

    void load();
    bool signedIn() const;
    std::string name() const;

    // After poll() gave the tokens. `now` in seconds.
    void signIn(const Token& token, std::string name, std::int64_t now);
    void setName(std::string name);
    // A valid access token, renewed when it expired or was refused. A revoked refresh token signs out.
    core::Result<std::string> accessToken(const Auth& auth, std::int64_t now, const core::CancelToken& cancel = {});
    // A request was refused with this token: the next accessToken() renews it.
    void invalidate(const std::string& accessToken);
    void signOut(const Auth& auth);  // revokes over the network: worker threads only
    // Signs out at once (any thread) and returns the refresh token for Auth::revoke ("" when signed out).
    std::string forget();

    // Runs `call(accessToken)`; when the token is refused, renews it and runs the call once more.
    template <typename F>
    auto withToken(const Auth& auth, std::int64_t now, F call, const core::CancelToken& cancel = {})
        -> decltype(call(std::string())) {
        auto token = accessToken(auth, now, cancel);
        if (!token.ok()) {
            return token.error();
        }
        auto result = call(token.value());
        if (result.ok() || result.error().kind != core::ErrorKind::Unauthorized) {
            return result;
        }
        invalidate(token.value());
        token = accessToken(auth, now, cancel);
        if (!token.ok()) {
            return token.error();
        }
        return call(token.value());
    }

private:
    void saveLocked() const;

    std::string path_;
    mutable std::mutex mutex_;
    std::mutex renewMutex_;  // one renewal at a time
    std::string refreshToken_;
    std::string name_;
    std::string accessToken_;
    std::int64_t expiresAt_ = 0;
};

}  // namespace playtube::youtube
