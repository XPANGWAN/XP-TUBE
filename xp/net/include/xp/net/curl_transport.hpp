#pragma once

#include "xp/net/http.hpp"
#include "xp/core/logger.hpp"

namespace xp::net {

// XP-owned libcurl transport. libcurl is a platform dependency; request policy,
// cancellation, response limits, redirects and streaming semantics belong to XP-TUBE.
class CurlHttpTransport final : public IHttpTransport {
public:
    explicit CurlHttpTransport(HttpConfig config = {}, core::Logger* logger = nullptr);

    core::Result<HttpResponse> send(const HttpRequest& request,
                                    const CancelToken& cancel = {}) const override;
    core::Result<PeerCertificate> fetch_peer_certificate(
        const std::string& https_url, const CancelToken& cancel = {}) const override;
    core::Result<HttpResponse> download(const HttpRequest& request, HttpSink sink,
                                        HttpProgress progress = {},
                                        const CancelToken& cancel = {},
                                        HttpStarted started = {}) const override;

private:
    core::Result<HttpResponse> perform(const HttpRequest& request, const CancelToken& cancel,
                                        const HttpSink* sink, const HttpProgress* progress,
                                        const HttpStarted* started) const;

    HttpConfig config_;
    core::Logger* logger_{nullptr}; // non-owning; optional
};

} // namespace xp::net
