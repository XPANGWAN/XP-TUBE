#include "net/url.hpp"

#include "xp/core/url.hpp"

namespace playtube::net {

std::optional<Url> parseUrl(std::string_view text) {
    const auto parsed = xp::core::url::parse(text);
    if (!parsed) return std::nullopt;
    return Url{parsed->scheme, parsed->host, parsed->port, parsed->path};
}

std::string toString(const Url& url) {
    return xp::core::url::to_string({url.scheme, url.host, url.port, url.path});
}

std::string origin(const Url& url) {
    return xp::core::url::origin({url.scheme, url.host, url.port, url.path});
}

std::string urlEncode(std::string_view text) {
    return xp::core::url::encode(text);
}

Query& Query::add(std::string_view key, std::string_view value) {
    const std::string encoded = xp::core::url::Query().add(key, value).str();
    if (text_.empty()) {
        text_ = encoded;
    } else {
        text_ += "&";
        text_ += encoded.substr(1);
    }
    return *this;
}

Query& Query::add(std::string_view key, long long value) {
    return add(key, std::to_string(value));
}

}  // namespace playtube::net
