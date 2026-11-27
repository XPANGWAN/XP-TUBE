#include "xp/core/redaction.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace xp::core {
namespace {

enum class ValueEnd { Query, PathSegment, Line, Quoted, JsonString };
struct Rule { std::string_view key; ValueEnd end; };

constexpr Rule kRules[] = {
    {"api_key=", ValueEnd::Query}, {"apikey=", ValueEnd::Query},
    {"password=", ValueEnd::Query}, {"secret=", ValueEnd::Query},
    {"token=", ValueEnd::Query}, {"&ip=", ValueEnd::Query},
    {"?ip=", ValueEnd::Query}, {"/ip/", ValueEnd::PathSegment},
    {"bearer ", ValueEnd::Query}, {"x-emby-token:", ValueEnd::Line},
    {"x-mediabrowser-token:", ValueEnd::Line}, {"authorization:", ValueEnd::Line},
    {"token=\"", ValueEnd::Quoted}, {"\"pw\"", ValueEnd::JsonString},
    {"\"password\"", ValueEnd::JsonString}, {"\"accesstoken\"", ValueEnd::JsonString},
    {"\"currentpw\"", ValueEnd::JsonString}, {"\"newpw\"", ValueEnd::JsonString},
    {"\"secret\"", ValueEnd::JsonString}, {"\"access_token\"", ValueEnd::JsonString},
    {"\"refresh_token\"", ValueEnd::JsonString}, {"\"yt_refresh_token\"", ValueEnd::JsonString},
    {"\"device_code\"", ValueEnd::JsonString},
};
using Range = std::pair<std::size_t, std::size_t>;

std::string lower_ascii(std::string_view input) {
    std::string out(input);
    for (char& c : out) {
        const auto u = static_cast<unsigned char>(c);
        if (u >= 'A' && u <= 'Z') c = static_cast<char>(u - 'A' + 'a');
    }
    return out;
}
bool delimiter(char c) {
    return c == '&' || c == ' ' || c == '"' || c == '\'' ||
           c == '\n' || c == '\r' || c == '\t';
}
std::size_t spaces(std::string_view s, std::size_t p) {
    while (p < s.size() && s[p] == ' ') ++p;
    return p;
}
Range find_value(std::string_view s, std::size_t p, ValueEnd mode) {
    std::size_t end = p;
    switch (mode) {
    case ValueEnd::Query:
        while (end < s.size() && !delimiter(s[end])) ++end;
        return {p, end};
    case ValueEnd::PathSegment:
        while (end < s.size() && s[end] != '/' && !delimiter(s[end]) && s[end] != '?') ++end;
        return {p, end};
    case ValueEnd::Line: {
        const auto begin = spaces(s, p);
        end = begin;
        while (end < s.size() && s[end] != '\n' && s[end] != '\r') ++end;
        return {begin, end};
    }
    case ValueEnd::Quoted:
        while (end < s.size() && s[end] != '"') ++end;
        return {p, end};
    case ValueEnd::JsonString: {
        auto q = spaces(s, p);
        if (q >= s.size() || s[q] != ':') return {p, p};
        q = spaces(s, q + 1);
        if (q >= s.size() || s[q] != '"') return {p, p};
        const auto begin = q + 1;
        end = begin;
        while (end < s.size() && s[end] != '"') {
            if (s[end] == '\\' && end + 1 < s.size()) ++end;
            ++end;
        }
        return {begin, std::min(end, s.size())};
    }
    }
    return {p, p};
}
}  // namespace

// The matching rules are adapted from the GPL-3.0-licensed migration reference
// at docs/SOURCE-PROVENANCE.md; this XP-owned implementation is maintained here.
std::string redact_secrets(std::string_view text) {
    const auto lower = lower_ascii(text);
    std::vector<Range> ranges;
    for (const auto& rule : kRules) {
        std::size_t from = 0;
        while (from < lower.size()) {
            const auto at = lower.find(rule.key, from);
            if (at == std::string::npos) break;
            const auto key_end = at + rule.key.size();
            const auto range = find_value(text, key_end, rule.end);
            if (range.second > range.first) {
                ranges.push_back(range);
                from = range.second;
            } else {
                from = key_end;
            }
        }
    }
    if (ranges.empty()) return std::string(text);
    std::sort(ranges.begin(), ranges.end());
    std::string out;
    out.reserve(text.size());
    std::size_t cursor = 0;
    for (const auto& range : ranges) {
        if (range.first < cursor) {
            cursor = std::max(cursor, range.second);
            continue;
        }
        out.append(text.substr(cursor, range.first - cursor));
        out += "***";
        cursor = range.second;
    }
    out.append(text.substr(cursor));
    return out;
}
}  // namespace xp::core
