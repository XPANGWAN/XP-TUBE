#include "xp/net/http.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {

class FakeTransport final : public xp::net::IHttpTransport {
public:
    xp::core::Result<xp::net::HttpResponse> send(
        const xp::net::HttpRequest& request, const xp::net::CancelToken& cancel) const override {
        if (cancel.cancelled()) {
            return xp::core::Result<xp::net::HttpResponse>::failure(
                xp::core::Error::make(xp::core::ErrorCode::cancelled, "cancelled"));
        }
        xp::net::HttpResponse response;
        response.status = 200;
        response.content_type = "text/plain";
        response.body = request.url;
        return xp::core::Result<xp::net::HttpResponse>::success(std::move(response));
    }

    xp::core::Result<xp::net::PeerCertificate> fetch_peer_certificate(
        const std::string& url, const xp::net::CancelToken& cancel) const override {
        if (cancel.cancelled() || url.rfind("https://", 0) != 0) {
            return xp::core::Result<xp::net::PeerCertificate>::failure(
                xp::core::Error::make(xp::core::ErrorCode::invalid_argument, "invalid certificate request"));
        }
        return xp::core::Result<xp::net::PeerCertificate>::success({});
    }

    xp::core::Result<xp::net::HttpResponse> download(
        const xp::net::HttpRequest&, xp::net::HttpSink sink, xp::net::HttpProgress progress,
        const xp::net::CancelToken& cancel, xp::net::HttpStarted started) const override {
        if (cancel.cancelled()) {
            return xp::core::Result<xp::net::HttpResponse>::failure(
                xp::core::Error::make(xp::core::ErrorCode::cancelled, "cancelled"));
        }
        static constexpr char payload[] = "xp";
        if (started) started(206);
        if (progress) progress({2, 2});
        if (!sink(payload, 2)) {
            return xp::core::Result<xp::net::HttpResponse>::failure(
                xp::core::Error::make(xp::core::ErrorCode::storage, "sink refused data"));
        }
        xp::net::HttpResponse response;
        response.status = 206;
        return xp::core::Result<xp::net::HttpResponse>::success(std::move(response));
    }
};

} // namespace

int main() {
    xp::net::CancelToken first;
    xp::net::CancelToken shared = first;
    assert(!first.cancelled());
    shared.cancel();
    assert(first.cancelled());

    bool externalCancelled = false;
    xp::net::CancelToken bridged([&externalCancelled] { return externalCancelled; });
    assert(!bridged.cancelled());
    externalCancelled = true;
    assert(bridged.cancelled());

    xp::net::HttpRequest request;
    assert(request.method == "GET");
    assert(request.timeout_ms == 30000);
    assert(request.connect_timeout_ms == 5000);
    assert(request.tls.mode == xp::net::TlsMode::verify_system_ca);
    assert(request.max_body_bytes == 0);

    xp::net::HttpConfig config;
    assert(config.max_body_bytes == 64u * 1024u * 1024u);
    assert(config.low_speed_limit_bytes_per_second == 1);
    assert(config.max_redirects == 5);
    assert(config.reject_https_downgrade);

    xp::net::TransferProgress progress;
    assert(progress.received_bytes == 0);
    assert(progress.total_bytes == 0);

    FakeTransport transport;
    request.url = "https://example.invalid/resource";
    auto response = transport.send(request, {});
    assert(response.has_value());
    assert(response.value().status == 200);
    assert(response.value().body == request.url);

    auto certificate = transport.fetch_peer_certificate(request.url, {});
    assert(certificate.has_value());

    std::string received;
    long started_status = 0;
    bool progress_called = false;
    auto downloaded = transport.download(
        request,
        [&](const char* data, std::size_t size) {
            received.append(data, size);
            return true;
        },
        [&](const xp::net::TransferProgress& value) {
            progress_called = value.received_bytes == 2 && value.total_bytes == 2;
        },
        {},
        [&](long status) { started_status = status; });
    assert(downloaded.has_value());
    assert(downloaded.value().status == 206);
    assert(received == "xp");
    assert(started_status == 206);
    assert(progress_called);

    return 0;
}
