#pragma once

#include "core/result.hpp"
#include "net/http.hpp"
#include "xp/net/http.hpp"

namespace playtube::net {

// Converts legacy YouTube request types at the boundary; the transport implementation
// is supplied by XP-TUBE and may be replaced without changing YouTube protocol code.
inline core::Result<HttpResponse> sendViaXp(const xp::net::IHttpTransport& transport,
                                            const HttpRequest& request,
                                            const core::CancelToken& cancel = {}) {
    xp::net::HttpRequest native;
    native.method = request.method;
    native.url = request.url;
    native.headers = request.headers;
    native.body = request.body;
    native.timeout_ms = request.timeoutMs;
    native.connect_timeout_ms = request.connectTimeoutMs;
    native.max_body_bytes = request.maxBodyBytes;
    xp::net::CancelToken token([cancel] { return cancel.cancelled(); });
    auto response = transport.send(native, token);
    if (!response.has_value()) {
        core::ErrorKind kind = core::ErrorKind::Network;
        switch (response.error().code) {
        case xp::core::ErrorCode::invalid_argument: kind = core::ErrorKind::InvalidAddress; break;
        case xp::core::ErrorCode::timeout: kind = core::ErrorKind::Timeout; break;
        case xp::core::ErrorCode::tls: kind = core::ErrorKind::Tls; break;
        case xp::core::ErrorCode::certificate_untrusted: kind = core::ErrorKind::CertificateUntrusted; break;
        case xp::core::ErrorCode::cancelled: kind = core::ErrorKind::Cancelled; break;
        case xp::core::ErrorCode::storage: kind = core::ErrorKind::Storage; break;
        case xp::core::ErrorCode::bad_response: kind = core::ErrorKind::BadResponse; break;
        case xp::core::ErrorCode::not_found: kind = core::ErrorKind::NotFound; break;
        case xp::core::ErrorCode::unsupported: kind = core::ErrorKind::UnsupportedServer; break;
        case xp::core::ErrorCode::io:
        case xp::core::ErrorCode::network:
        case xp::core::ErrorCode::unavailable:
        case xp::core::ErrorCode::internal: kind = core::ErrorKind::Network; break;
        }
        return core::makeError(kind, response.error().message);
    }
    HttpResponse converted;
    converted.status = response.value().status;
    converted.body = std::move(response.value().body);
    converted.contentType = std::move(response.value().content_type);
    return converted;
}

} // namespace playtube::net
