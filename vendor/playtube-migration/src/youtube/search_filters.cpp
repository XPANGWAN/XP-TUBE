#include "youtube/search_filters.hpp"

#include <cstdint>
#include <vector>

namespace playtube::youtube {

namespace {

// a field of varint type (wire type 0); values here stay below 128: one byte each
void varintField(std::vector<std::uint8_t>& out, int field, int value) {
    out.push_back(static_cast<std::uint8_t>(field << 3));
    out.push_back(static_cast<std::uint8_t>(value));
}

std::string base64(const std::vector<std::uint8_t>& bytes) {
    static const char* const kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned b0 = bytes[i];
        const unsigned b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const unsigned b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        out += kAlphabet[b0 >> 2];
        out += kAlphabet[((b0 & 3) << 4) | (b1 >> 4)];
        out += i + 1 < bytes.size() ? kAlphabet[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out += i + 2 < bytes.size() ? kAlphabet[b2 & 63] : '=';
    }
    return out;
}

}  // namespace

std::string searchParams(const SearchFilters& filters) {
    using F = SearchFilters;
    std::vector<std::uint8_t> inner;  // field 2 of the message: the filters
    if (filters.upload != F::Upload::Any) {
        varintField(inner, 1, static_cast<int>(filters.upload));  // hour 1, today 2, week 3, month 4, year 5
    }
    if (filters.type != F::Type::Any) {
        const int type = filters.type == F::Type::Video      ? 1
                         : filters.type == F::Type::Channel  ? 2
                         : filters.type == F::Type::Playlist ? 3
                                                             : 9;  // Shorts
        varintField(inner, 2, type);
    }
    if (filters.length != F::Length::Any) {
        const int length = filters.length == F::Length::Under4 ? 1 : filters.length == F::Length::Over20 ? 2 : 3;
        varintField(inner, 3, length);
    }
    if (filters.live) {
        varintField(inner, 8, 1);
    }
    std::vector<std::uint8_t> message;
    if (filters.sort != F::Sort::Relevance) {
        const int sort = filters.sort == F::Sort::Rating ? 1 : filters.sort == F::Sort::Date ? 2 : 3;
        varintField(message, 1, sort);
    }
    if (!inner.empty()) {
        message.push_back(static_cast<std::uint8_t>((2 << 3) | 2));  // field 2, length-delimited
        message.push_back(static_cast<std::uint8_t>(inner.size()));
        message.insert(message.end(), inner.begin(), inner.end());
    }
    return message.empty() ? std::string() : base64(message);
}

}  // namespace playtube::youtube
