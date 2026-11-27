#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xp::core {

// Resolve an untrusted relative path under root. Reject absolute paths,
// traversal segments, empty segments, backslashes and embedded NUL bytes.
std::optional<std::string> resolve_inside(std::string_view root,
                                          std::string_view relative);

} // namespace xp::core
