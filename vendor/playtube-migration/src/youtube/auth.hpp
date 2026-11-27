#pragma once

#include <string>

#include "core/result.hpp"
#include "core/task.hpp"
#include "net/http.hpp"
#include "youtube/client.hpp"
#include "youtube/models.hpp"

namespace playtube::youtube {

// Sign-in by code with YouTube's TV app identity (OAuth device flow): start() gives a code the user types on
// another device, poll() waits for it, refresh() renews the access token. Blocking: worker threads only.
class Auth {
public:
    Auth(const net::HttpClient& http, Endpoints endpoints);

    core::Result<DeviceCode> start(const core::CancelToken& cancel = {}) const;
    // Token.pending while the user has not typed the code yet; ErrorKind::Unauthorized once it expired or was
    // refused.
    core::Result<Token> poll(const std::string& deviceCode, const core::CancelToken& cancel = {}) const;
    // ErrorKind::Unauthorized when the refresh token was revoked.
    core::Result<Token> refresh(const std::string& refreshToken, const core::CancelToken& cancel = {}) const;
    void revoke(const std::string& token) const;  // best effort

private:
    core::Result<std::string> send(const std::string& url, const std::string& body,
                                   const core::CancelToken& cancel) const;

    const net::HttpClient& http_;
    Endpoints endpoints_;
};

}  // namespace playtube::youtube
