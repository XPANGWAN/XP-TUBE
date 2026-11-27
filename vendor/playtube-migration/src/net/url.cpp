#include "net/url.hpp"

#include <algorithm>
#include <cctype>

#include "core/strings.hpp"

namespace playtube::net {

namespace {

bool isHostChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '.' || c == '-' || c == '_'; }

bool isIpv6Char(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0 || c == ':' || c == '.'; }

// host[:port][/path] after the scheme
std::optional<Url> parseRest(const std::string& scheme, std::string_view rest) {
    if (rest.empty()) {
        return std::nullopt;
    }
    for (char c : rest) {
        if (static_cast<unsigned char>(c) < 0x21 || c == '@' || c == '?' || c == '#' || c == '\\' || c == 0x7f) {
            return std::nullopt;
        }
    }
    const std::size_t slash = rest.find('/');
    const std::string_view authority = rest.substr(0, slash);
    const std::string_view path = slash == std::string_view::npos ? std::string_view() : rest.substr(slash);
    std::string_view host = authority;
    std::string_view port;
    if (!authority.empty() && authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos || close < 2) {
            return std::nullopt;
        }
        for (char c : authority.substr(1, close - 1)) {
            if (!isIpv6Char(c)) {
                return std::nullopt;
            }
        }
        host = authority.substr(0, close + 1);
        const std::string_view after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after.front() != ':' || after.size() == 1) {
                return std::nullopt;
            }
            port = after.substr(1);
        }
    } else {
        const std::size_t colon = authority.find(':');
        if (colon != std::string_view::npos) {
            host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
            if (port.empty()) {
                return std::nullopt;
            }
        }
        if (host.empty() || !std::all_of(host.begin(), host.end(), isHostChar)) {
            return std::nullopt;
        }
    }
    Url url;
    url.scheme = scheme;
    url.host = core::asciiLower(host);
    if (!port.empty()) {
        if (port.size() > 5) {
            return std::nullopt;
        }
        int value = 0;
        for (char c : port) {
            if (c < '0' || c > '9') {
                return std::nullopt;
            }
            value = value * 10 + (c - '0');
        }
        if (value < 1 || value > 65535) {
            return std::nullopt;
        }
        url.port = value;
    }
    url.path = std::string(path);
    while (!url.path.empty() && url.path.back() == '/') {
        url.path.pop_back();
    }
    return url;
}

}  // namespace

std::optional<Url> parseUrl(std::string_view text) {
    text = core::trimmed(text);
    const std::size_t separator = text.find("://");
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string scheme = core::asciiLower(text.substr(0, separator));
    if (scheme != "http" && scheme != "https") {
        return std::nullopt;
    }
    return parseRest(scheme, text.substr(separator + 3));
}

std::string toString(const Url& url) { return origin(url) + url.path; }

std::string origin(const Url& url) {
    std::string out = url.scheme + "://" + url.host;
    if (url.port != 0) {
        out += ":" + std::to_string(url.port);
    }
    return out;
}

std::string urlEncode(std::string_view text) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (std::isalnum(byte) != 0 || c == '-' || c == '.' || c == '_' || c == '~') {
            out += c;
        } else {
            out += '%';
            out += kHex[byte >> 4];
            out += kHex[byte & 0x0F];
        }
    }
    return out;
}

Query& Query::add(std::string_view key, std::string_view value) {
    text_ += text_.empty() ? '?' : '&';
    text_ += urlEncode(key);
    text_ += '=';
    text_ += urlEncode(value);
    return *this;
}

Query& Query::add(std::string_view key, long long value) { return add(key, std::to_string(value)); }

}  // namespace playtube::net
