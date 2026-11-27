#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "xp/core/result.hpp"

namespace xp::net {

// Copies share cancellation state, so UI and worker code can coordinate safely.
class CancelToken {
public:
    CancelToken() : state_(std::make_shared<std::atomic<bool>>(false)) {}
    void cancel() const noexcept { state_->store(true); }
    bool cancelled() const noexcept { return state_->load(); }

private:
    std::shared_ptr<std::atomic<bool>> state_;
};

struct PeerCertificate {
    std::string subject;
    std::string issuer;
    std::string not_after;
    std::string spki_sha256_hex;
};

enum class TlsMode {
    verify_system_ca,
    verify_pinned_public_key,
    inspect_peer_certificate
};

struct TlsPolicy {
    TlsMode mode{TlsMode::verify_system_ca};
    std::string ca_file;
    // curl-compatible sha256// base64 pin. Required only for verify_pinned_public_key.
    std::string public_key_pin;
};

struct HttpRequest {
    std::string method{"GET"};
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    long timeout_ms{30000};          // 0 means no overall timeout
    long connect_timeout_ms{5000};
    std::size_t max_body_bytes{0};   // 0 means use HttpConfig::max_body_bytes
    TlsPolicy tls;
};

struct HttpResponse {
    long status{0};
    std::string body;
    std::string content_type;
};

struct HttpConfig {
    std::string ca_file;
    std::size_t max_body_bytes{64u * 1024u * 1024u};
    long low_speed_seconds{15};
    long low_speed_limit_bytes_per_second{1};
    long max_redirects{5};
    bool reject_https_downgrade{true};
};

struct TransferProgress {
    long long received_bytes{0};
    long long total_bytes{0}; // 0 when the server does not provide a total
};

using HttpSink = std::function<bool(const char*, std::size_t)>;
using HttpProgress = std::function<void(const TransferProgress&)>;
using HttpStarted = std::function<void(long)>;

// Backend boundary. Implementations must preserve TLS verification by default,
// enforce body limits for buffered responses, and stop promptly on cancellation.
// HTTP 4xx/5xx remain ordinary responses; callers interpret status codes.
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual core::Result<HttpResponse> send(const HttpRequest& request,
                                            const CancelToken& cancel = {}) const = 0;

    // Informational only: callers must obtain explicit approval before pinning this key.
    virtual core::Result<PeerCertificate> fetch_peer_certificate(
        const std::string& https_url, const CancelToken& cancel = {}) const = 0;

    virtual core::Result<HttpResponse> download(const HttpRequest& request,
                                                HttpSink sink,
                                                HttpProgress progress = {},
                                                const CancelToken& cancel = {},
                                                HttpStarted started = {}) const = 0;
};

} // namespace xp::net
