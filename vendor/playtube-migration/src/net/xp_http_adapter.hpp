#pragma once

#include "net/http.hpp"
#include "xp/net/http.hpp"

namespace playtube::net {

// Transitional bridge: XP owns the transport contract while the proven libcurl
// implementation remains the active PS4 backend during caller migration.
class XpHttpAdapter final : public xp::net::IHttpTransport {
public:
    explicit XpHttpAdapter(const HttpClient& client) : client_(client) {}

    xp::core::Result<xp::net::HttpResponse> send(
        const xp::net::HttpRequest& request,
        const xp::net::CancelToken& cancel = {}) const override;

    xp::core::Result<xp::net::PeerCertificate> fetch_peer_certificate(
        const std::string& https_url,
        const xp::net::CancelToken& cancel = {}) const override;

    xp::core::Result<xp::net::HttpResponse> download(
        const xp::net::HttpRequest& request,
        xp::net::HttpSink sink,
        xp::net::HttpProgress progress = {},
        const xp::net::CancelToken& cancel = {},
        xp::net::HttpStarted started = {}) const override;

private:
    const HttpClient& client_;
};

}  // namespace playtube::net
