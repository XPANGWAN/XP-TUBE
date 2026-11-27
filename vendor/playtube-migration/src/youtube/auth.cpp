#include "youtube/auth.hpp"

#include <cstdio>
#include <random>

#include "core/json.hpp"
#include "net/url.hpp"
#include "youtube/parse.hpp"

namespace playtube::youtube {

namespace {

// YouTube's TV app (public values, the same as the previous Playtube's so that its sign-in keeps working)
const char* const kClientId = "861556708454-d6dlm3lh05idd8npek18k6be8ba3oc68.apps.googleusercontent.com";
const char* const kClientSecret = "SboVhoG9s0rNafixCSGGKXAT";
const char* const kScope = "http://gdata.youtube.com https://www.googleapis.com/auth/youtube";
const char* const kTvAgent = "Mozilla/5.0 (ChromiumStylePlatform) Cobalt/Version";

std::string randomDeviceId() {
    std::random_device random;
    char id[37];
    std::snprintf(id, sizeof id, "%08x-%04x-4%03x-%04x-%04x%08x", random(), random() & 0xffff, random() & 0xfff,
                  (random() & 0x3fff) | 0x8000, random() & 0xffff, random());
    return id;
}

}  // namespace

Auth::Auth(const net::HttpClient& http, Endpoints endpoints) : http_(http), endpoints_(std::move(endpoints)) {}

// The token endpoint answers its errors ("authorization_pending", "invalid_grant") with 4xx and a JSON body.
core::Result<std::string> Auth::send(const std::string& url, const std::string& body,
                                     const core::CancelToken& cancel) const {
    net::HttpRequest request;
    request.method = "POST";
    request.url = url;
    request.body = body;
    request.maxBodyBytes = 64 * 1024;
    request.headers = {{"Content-Type", "application/json"}, {"User-Agent", kTvAgent}};
    auto response = http_.send(request, {}, cancel);
    if (!response.ok()) {
        return response.error();
    }
    if (response.value().status >= 500) {
        return core::makeError(core::ErrorKind::Server, "sign-in: HTTP " + std::to_string(response.value().status),
                               static_cast<int>(response.value().status));
    }
    return std::move(response.value().body);
}

core::Result<DeviceCode> Auth::start(const core::CancelToken& cancel) const {
    const auto body = send(endpoints_.youtube + "/o/oauth2/device/code",
                           core::json::writeObject({{"client_id", kClientId},
                                                    {"scope", kScope},
                                                    {"device_id", randomDeviceId()},
                                                    {"device_model", "ytlr::"}}),
                           cancel);
    if (!body.ok()) {
        return body.error();
    }
    return parseDeviceCode(body.value());
}

core::Result<Token> Auth::poll(const std::string& deviceCode, const core::CancelToken& cancel) const {
    const auto body = send(endpoints_.youtube + "/o/oauth2/token",
                           core::json::writeObject({{"client_id", kClientId},
                                                    {"client_secret", kClientSecret},
                                                    {"code", deviceCode},
                                                    {"grant_type", "http://oauth.net/grant_type/device/1.0"}}),
                           cancel);
    if (!body.ok()) {
        return body.error();
    }
    return parseToken(body.value());
}

core::Result<Token> Auth::refresh(const std::string& refreshToken, const core::CancelToken& cancel) const {
    const auto body = send(endpoints_.youtube + "/o/oauth2/token",
                           core::json::writeObject({{"client_id", kClientId},
                                                    {"client_secret", kClientSecret},
                                                    {"refresh_token", refreshToken},
                                                    {"grant_type", "refresh_token"}}),
                           cancel);
    if (!body.ok()) {
        return body.error();
    }
    auto token = parseToken(body.value());
    if (token.ok() && token.value().pending) {
        return core::makeError(core::ErrorKind::BadResponse, "refresh: pending");
    }
    return token;
}

void Auth::revoke(const std::string& token) const {
    net::HttpRequest request;
    request.method = "POST";
    request.url = endpoints_.oauthRevoke + "/revoke";
    request.body = "token=" + net::urlEncode(token);
    request.timeoutMs = 10000;
    request.maxBodyBytes = 64 * 1024;
    request.headers = {{"Content-Type", "application/x-www-form-urlencoded"}, {"User-Agent", kTvAgent}};
    (void)http_.send(request);
}

}  // namespace playtube::youtube
