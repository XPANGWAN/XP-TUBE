#include "xp/net/http.hpp"

#include <type_traits>

namespace xp::net {

static_assert(std::is_abstract<IHttpTransport>::value,
              "HTTP transport must remain an injectable backend boundary");
static_assert(static_cast<int>(TlsMode::verify_system_ca) == 0,
              "the default TLS policy enum value must remain stable");

void http_contract_compile_anchor() noexcept {
    const HttpRequest request{};
    (void)request;
}

} // namespace xp::net
