#include "xp/net/http.hpp"

#include <type_traits>

namespace xp::net {

// Compile-time integration guard: the PS4 and host application targets must
// consume the same XP-owned HTTP contract even while curl remains the backend.
static_assert(std::is_abstract<IHttpTransport>::value,
              "HTTP transport must remain an injectable backend boundary");
static_assert(HttpRequest{}.tls.mode == TlsMode::verify_system_ca,
              "TLS verification must remain the default policy");

void http_contract_compile_anchor() noexcept {}

} // namespace xp::net
