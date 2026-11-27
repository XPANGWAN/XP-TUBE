#pragma once

#include "core/result.hpp"
#include "core/task.hpp"
#include "xp/net/http.hpp"

namespace playtube::youtube::detail {

inline core::Error fromXpError(const xp::core::Error& error) {
    core::ErrorKind kind = core::ErrorKind::Network;
    switch (error.code) {
    case xp::core::ErrorCode::invalid_argument: kind = core::ErrorKind::InvalidAddress; break;
    case xp::core::ErrorCode::not_found: kind = core::ErrorKind::NotFound; break;
    case xp::core::ErrorCode::io:
    case xp::core::ErrorCode::storage: kind = core::ErrorKind::Storage; break;
    case xp::core::ErrorCode::timeout: kind = core::ErrorKind::Timeout; break;
    case xp::core::ErrorCode::tls: kind = core::ErrorKind::Tls; break;
    case xp::core::ErrorCode::certificate_untrusted: kind = core::ErrorKind::CertificateUntrusted; break;
    case xp::core::ErrorCode::bad_response: kind = core::ErrorKind::BadResponse; break;
    case xp::core::ErrorCode::unsupported: kind = core::ErrorKind::UnsupportedServer; break;
    case xp::core::ErrorCode::cancelled: kind = core::ErrorKind::Cancelled; break;
    case xp::core::ErrorCode::unavailable: kind = core::ErrorKind::Server; break;
    case xp::core::ErrorCode::network:
    case xp::core::ErrorCode::internal: kind = core::ErrorKind::Network; break;
    }
    return core::makeError(kind, error.message);
}

inline xp::net::CancelToken xpCancel(const core::CancelToken& cancel) {
    return xp::net::CancelToken([cancel] { return cancel.cancelled(); });
}

inline core::Result<xp::net::HttpResponse> sendHttp(
    const xp::net::IHttpTransport& transport, const xp::net::HttpRequest& request,
    const core::CancelToken& cancel = {}) {
    auto response = transport.send(request, xpCancel(cancel));
    if (!response.has_value()) return fromXpError(response.error());
    return std::move(response.value());
}

} // namespace playtube::youtube::detail
