#pragma once

#include <string>
#include <utility>

namespace xp::core {

enum class ErrorCode {
    invalid_argument,
    not_found,
    io,
    network,
    timeout,
    tls,
    certificate_untrusted,
    bad_response,
    storage,
    unsupported,
    cancelled,
    unavailable,
    internal
};

struct Error {
    ErrorCode code{ErrorCode::internal};
    std::string message;
    std::string context;

    static Error make(ErrorCode code, std::string message, std::string context = {}) {
        return Error{code, std::move(message), std::move(context)};
    }
};

} // namespace xp::core
