#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xp::net {

// SHA-256 of the DER SubjectPublicKeyInfo in a PEM X.509 certificate,
// returned as 64 lower-case hexadecimal characters.
std::optional<std::string> spki_sha256_hex_from_pem(std::string_view pem);

} // namespace xp::net
