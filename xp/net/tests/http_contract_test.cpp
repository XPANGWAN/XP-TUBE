#include "xp/net/http.hpp"

#include <cassert>
#include <string>

int main() {
    xp::net::CancelToken first;
    xp::net::CancelToken shared = first;
    assert(!first.cancelled());
    shared.cancel();
    assert(first.cancelled());

    xp::net::HttpRequest request;
    assert(request.method == "GET");
    assert(request.timeout_ms == 30000);
    assert(request.connect_timeout_ms == 5000);
    assert(request.tls.mode == xp::net::TlsMode::verify_system_ca);
    assert(request.max_body_bytes == 0);

    xp::net::HttpConfig config;
    assert(config.max_body_bytes == 64u * 1024u * 1024u);

    xp::net::TransferProgress progress;
    assert(progress.received_bytes == 0);
    assert(progress.total_bytes == 0);

    return 0;
}
