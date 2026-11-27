#include "core/redact.hpp"

#include "xp/core/redaction.hpp"

namespace playtube::core {

std::string redact(std::string_view text) {
    return xp::core::redact_secrets(text);
}

}  // namespace playtube::core
