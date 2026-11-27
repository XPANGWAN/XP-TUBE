#include "xp/core/path.hpp"

namespace xp::core {

std::optional<std::string> resolve_inside(std::string_view root,
                                          std::string_view relative) {
    if (root.empty() || relative.empty() || relative.front() == '/') {
        return std::nullopt;
    }
    if (relative.find('\\\\') != std::string_view::npos ||
        relative.find('\\0') != std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t start = 0;
    while (start < relative.size()) {
        const auto slash = relative.find('/', start);
        const auto end = slash == std::string_view::npos ? relative.size() : slash;
        const auto segment = relative.substr(start, end - start);
        if (segment.empty() || segment == "." || segment == "..") {
            return std::nullopt;
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
        if (start == relative.size()) {
            return std::nullopt;
        }
    }

    std::string resolved(root);
    if (resolved.back() != '/') {
        resolved.push_back('/');
    }
    resolved.append(relative.data(), relative.size());
    return resolved;
}

} // namespace xp::core
