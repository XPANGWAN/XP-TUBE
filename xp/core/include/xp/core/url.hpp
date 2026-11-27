#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace xp::core::url {

struct Url {
    std::string scheme;
    std::string host;
    int port = 0;
    std::string path;
};

std::optional<Url> parse(std::string_view text);
std::string to_string(const Url& url);
std::string origin(const Url& url);
std::string encode(std::string_view text);

class Query {
public:
    Query& add(std::string_view key, std::string_view value);
    Query& add(std::string_view key, long long value);
    const std::string& str() const noexcept { return text_; }
private:
    std::string text_;
};

}  // namespace xp::core::url
