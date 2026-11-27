#include "xp/core/url.hpp"

#include <algorithm>

#include "xp/core/text.hpp"

namespace xp::core::url {
namespace {

bool ascii_alpha_numeric(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool host_char(char c) noexcept {
    return ascii_alpha_numeric(c) || c == '.' || c == '-' || c == '_';
}

bool ipv6_char(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F') || c == ':' || c == '.';
}

std::optional<Url> parse_rest(const std::string& scheme, std::string_view rest) {
    if (rest.empty()) return std::nullopt;
    for (char c : rest) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x21 || byte == 0x7f || c == '@' || c == '?' || c == '#' || c == '\\') {
            return std::nullopt;
        }
    }

    const std::size_t slash = rest.find('/');
    const std::string_view authority = rest.substr(0, slash);
    const std::string_view path = slash == std::string_view::npos ? std::string_view{} : rest.substr(slash);
    std::string_view host = authority;
    std::string_view port;

    if (!authority.empty() && authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos || close < 2) return std::nullopt;
        for (char c : authority.substr(1, close - 1)) {
            if (!ipv6_char(c)) return std::nullopt;
        }
        host = authority.substr(0, close + 1);
        const std::string_view after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':' || after.size() == 1) return std::nullopt;
            port = after.substr(1);
        }
    } else {
        const std::size_t colon = authority.find(':');
        if (colon != std::string_view::npos) {
            host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
            if (port.empty()) return std::nullopt;
        }
        if (host.empty() || !std::all_of(host.begin(), host.end(), host_char)) return std::nullopt;
    }

    Url result;
    result.scheme = scheme;
    result.host = ascii_lower(host);
    if (!port.empty()) {
        if (port.size() > 5) return std::nullopt;
        int value = 0;
        for (char c : port) {
            if (c < '0' || c > '9') return std::nullopt;
            value = value * 10 + (c - '0');
        }
        if (value < 1 || value > 65535) return std::nullopt;
        result.port = value;
    }
    result.path.assign(path);
    while (!result.path.empty() && result.path.back() == '/') result.path.pop_back();
    return result;
}

bool unreserved(unsigned char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}

}  // namespace

std::optional<Url> parse(std::string_view text) {
    text = trim_ascii(text);
    const std::size_t separator = text.find("://");
    if (separator == std::string_view::npos) return std::nullopt;
    const std::string scheme = ascii_lower(text.substr(0, separator));
    if (scheme != "http" && scheme != "https") return std::nullopt;
    return parse_rest(scheme, text.substr(separator + 3));
}

std::string to_string(const Url& url) { return origin(url) + url.path; }

std::string origin(const Url& url) {
    std::string result = url.scheme + "://" + url.host;
    if (url.port != 0) result += ":" + std::to_string(url.port);
    return result;
}

std::string encode(std::string_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (unreserved(byte)) {
            result += c;
        } else {
            result += '%';
            result += hex[byte >> 4];
            result += hex[byte & 0x0F];
        }
    }
    return result;
}

Query& Query::add(std::string_view key, std::string_view value) {
    text_ += text_.empty() ? '?' : '&';
    text_ += encode(key);
    text_ += '=';
    text_ += encode(value);
    return *this;
}

Query& Query::add(std::string_view key, long long value) {
    return add(key, std::to_string(value));
}

}  // namespace xp::core::url
