#include "net/xp_http_adapter.hpp"

#include <utility>

namespace playtube::net {
namespace {

using XpResponse = xp::core::Result<xp::net::HttpResponse>;

xp::core::Error mapError(const core::Error& error) {
    xp::core::ErrorCode code = xp::core::ErrorCode::network;
    switch (error.kind) {
    case core::ErrorKind::InvalidAddress: code = xp::core::ErrorCode::invalid_argument; break;
    case core::ErrorKind::Timeout: code = xp::core::ErrorCode::timeout; break;
    case core::ErrorKind::Tls: code = xp::core::ErrorCode::tls; break;
    case core::ErrorKind::CertificateUntrusted: code = xp::core::ErrorCode::certificate_untrusted; break;
    case core::ErrorKind::Cancelled: code = xp::core::ErrorCode::cancelled; break;
    case core::ErrorKind::Storage: code = xp::core::ErrorCode::storage; break;
    case core::ErrorKind::BadResponse:
    case core::ErrorKind::Unauthorized:
    case core::ErrorKind::NotFound:
    case core::ErrorKind::Server:
    case core::ErrorKind::Blocked:
    case core::ErrorKind::UnsupportedServer: code = xp::core::ErrorCode::bad_response; break;
    case core::ErrorKind::Network: code = xp::core::ErrorCode::network; break;
    }
    return xp::core::Error::make(code, error.message,
                                 error.httpStatus == 0 ? std::string() : std::to_string(error.httpStatus));
}

XpResponse failure(xp::core::ErrorCode code, const char* message) {
    return XpResponse::failure(xp::core::Error::make(code, message));
}

bool prepare(const xp::net::HttpRequest& request, HttpRequest& legacy,
             std::string& pin, xp::core::Error& error) {
    if (!request.tls.ca_file.empty()) {
        error = xp::core::Error::make(xp::core::ErrorCode::unsupported,
                                      "per-request CA files are not supported by the transitional backend");
        return false;
    }
    if (request.tls.mode == xp::net::TlsMode::inspect_peer_certificate) {
        error = xp::core::Error::make(xp::core::ErrorCode::invalid_argument,
                                      "use fetch_peer_certificate for informational certificate inspection");
        return false;
    }
    if (request.tls.mode == xp::net::TlsMode::verify_pinned_public_key) {
        if (request.tls.public_key_pin.empty()) {
            error = xp::core::Error::make(xp::core::ErrorCode::invalid_argument,
                                          "public-key pin is required");
            return false;
        }
        pin = request.tls.public_key_pin;
    } else if (!request.tls.public_key_pin.empty()) {
        error = xp::core::Error::make(xp::core::ErrorCode::invalid_argument,
                                      "public-key pin requires verify_pinned_public_key mode");
        return false;
    }

    legacy.method = request.method;
    legacy.url = request.url;
    legacy.headers = request.headers;
    legacy.body = request.body;
    legacy.timeoutMs = request.timeout_ms;
    legacy.connectTimeoutMs = request.connect_timeout_ms;
    legacy.maxBodyBytes = request.max_body_bytes;
    return true;
}

xp::net::HttpResponse convertResponse(core::HttpResponse response) {
    xp::net::HttpResponse converted;
    converted.status = response.status;
    converted.body = std::move(response.body);
    converted.content_type = std::move(response.contentType);
    return converted;
}

core::CancelToken bridgeCancel(const xp::net::CancelToken& cancel) {
    return core::CancelToken([cancel] { return cancel.cancelled(); });
}

std::string lowerHex(const core::Sha256& digest) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (const std::uint8_t byte : digest) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

}  // namespace

XpResponse XpHttpAdapter::send(const xp::net::HttpRequest& request,
                               const xp::net::CancelToken& cancel) const {
    if (cancel.cancelled()) {
        return failure(xp::core::ErrorCode::cancelled, "cancelled");
    }
    HttpRequest legacy;
    std::string pin;
    xp::core::Error error;
    if (!prepare(request, legacy, pin, error)) {
        return XpResponse::failure(std::move(error));
    }
    auto response = client_.send(legacy, pin, bridgeCancel(cancel));
    if (!response.ok()) {
        return XpResponse::failure(mapError(response.error()));
    }
    return XpResponse::success(convertResponse(std::move(response.value())));
}

xp::core::Result<xp::net::PeerCertificate> XpHttpAdapter::fetch_peer_certificate(
    const std::string& https_url, const xp::net::CancelToken& cancel) const {
    if (https_url.rfind("https://", 0) != 0) {
        return xp::core::Result<xp::net::PeerCertificate>::failure(
            xp::core::Error::make(xp::core::ErrorCode::invalid_argument,
                                  "certificate inspection requires an HTTPS URL"));
    }
    if (cancel.cancelled()) {
        return xp::core::Result<xp::net::PeerCertificate>::failure(
            xp::core::Error::make(xp::core::ErrorCode::cancelled, "cancelled"));
    }
    auto certificate = client_.fetchCertificate(https_url, bridgeCancel(cancel));
    if (!certificate.ok()) {
        return xp::core::Result<xp::net::PeerCertificate>::failure(mapError(certificate.error()));
    }
    xp::net::PeerCertificate converted;
    converted.subject = certificate.value().subject;
    converted.issuer = certificate.value().issuer;
    converted.not_after = certificate.value().notAfter;
    converted.spki_sha256_hex = lowerHex(certificate.value().spkiSha256);
    return xp::core::Result<xp::net::PeerCertificate>::success(std::move(converted));
}

XpResponse XpHttpAdapter::download(const xp::net::HttpRequest& request, xp::net::HttpSink sink,
                                   xp::net::HttpProgress progress,
                                   const xp::net::CancelToken& cancel,
                                   xp::net::HttpStarted started) const {
    if (!sink) {
        return failure(xp::core::ErrorCode::invalid_argument, "download sink is required");
    }
    if (cancel.cancelled()) {
        return failure(xp::core::ErrorCode::cancelled, "cancelled");
    }
    HttpRequest legacy;
    std::string pin;
    xp::core::Error error;
    if (!prepare(request, legacy, pin, error)) {
        return XpResponse::failure(std::move(error));
    }
    HttpClient::Progress legacyProgress;
    if (progress) {
        legacyProgress = [progress = std::move(progress)](long long received, long long total) {
            progress({received, total});
        };
    }
    auto response = client_.download(
        legacy, std::move(sink), std::move(legacyProgress), pin, bridgeCancel(cancel),
        [started = std::move(started)](long status) {
            if (started) started(status);
        });
    if (!response.ok()) {
        return XpResponse::failure(mapError(response.error()));
    }
    return XpResponse::success(convertResponse(std::move(response.value())));
}

}  // namespace playtube::net
