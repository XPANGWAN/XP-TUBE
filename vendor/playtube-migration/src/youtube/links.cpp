#include "youtube/links.hpp"

#include <cctype>

namespace playtube::youtube {

namespace {

bool idChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_'; }

bool isId(std::string_view text) {
    if (text.size() != 11) {
        return false;
    }
    for (char c : text) {
        if (!idChar(c)) {
            return false;
        }
    }
    return true;
}

// The id at the start of `text`, ended by the end, '/', '?', '&' or '#'.
std::optional<std::string> idAt(std::string_view text) {
    const std::size_t end = text.find_first_of("/?&#");
    const std::string_view id = text.substr(0, end);
    return isId(id) ? std::optional<std::string>(id) : std::nullopt;
}

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }

// A bare id rather than a search ("minecraft22", "Gta6Trailer" stay searches): a '-' or '_', or a digit with
// lower-case letters and an upper-case letter past the first character, as in YouTube's random ids.
bool looksRandom(std::string_view text) {
    if (text.find_first_of("-_") != std::string_view::npos) {
        return true;
    }
    bool digit = false;
    bool lower = false;
    bool upper = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        digit = digit || std::isdigit(c) != 0;
        lower = lower || std::islower(c) != 0;
        upper = upper || (i > 0 && std::isupper(c) != 0);
    }
    return digit && lower && upper;
}

}  // namespace

std::optional<std::string> videoIdFrom(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    if (isId(text) && looksRandom(text)) {
        return std::string(text);
    }
    std::string lowered(text.substr(0, 8));
    for (char& c : lowered) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (std::string_view scheme : {"https://", "http://"}) {
        if (startsWith(lowered, scheme)) {
            text.remove_prefix(scheme.size());
            break;
        }
    }
    const std::size_t hostEnd = text.find_first_of("/?#");
    std::string host(text.substr(0, hostEnd));
    for (char& c : host) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const char* prefix : {"www.", "m.", "music."}) {
        if (startsWith(host, prefix)) {
            host.erase(0, std::string_view(prefix).size());
            break;
        }
    }
    const std::string_view path = hostEnd == std::string_view::npos ? std::string_view() : text.substr(hostEnd);
    if (host == "youtu.be") {
        return startsWith(path, "/") ? idAt(path.substr(1)) : std::nullopt;
    }
    if (host != "youtube.com") {
        return std::nullopt;
    }
    for (std::string_view prefix : {"/shorts/", "/live/", "/embed/", "/v/"}) {
        if (startsWith(path, prefix)) {
            return idAt(path.substr(prefix.size()));
        }
    }
    if (!startsWith(path, "/watch?")) {
        return std::nullopt;
    }
    std::string_view query = path.substr(7);
    query = query.substr(0, query.find('#'));
    while (!query.empty()) {
        const std::size_t end = query.find('&');
        const std::string_view pair = query.substr(0, end);
        if (startsWith(pair, "v=")) {
            const std::string_view id = pair.substr(2);
            return isId(id) ? std::optional<std::string>(id) : std::nullopt;
        }
        if (end == std::string_view::npos) {
            break;
        }
        query.remove_prefix(end + 1);
    }
    return std::nullopt;
}

}  // namespace playtube::youtube
