#include "core/utf.hpp"

#include <utility>

namespace playtube::core {

namespace {

constexpr char32_t kReplacement = 0xFFFD;

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

}  // namespace

void appendCodePoint(std::string& out, char32_t codePoint) {
    appendUtf8(out, (codePoint >= 0xD800 && codePoint <= 0xDFFF) || codePoint > 0x10FFFF ? 0xFFFD : codePoint);
}

char32_t nextCodePoint(std::string_view text, std::size_t& pos) {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char first = byte(pos);
    std::size_t length = 0;
    char32_t cp = 0;
    char32_t minimum = 0;
    if (first < 0x80) {
        ++pos;
        return first;
    } else if ((first & 0xE0) == 0xC0) {
        length = 2;
        cp = first & 0x1F;
        minimum = 0x80;
    } else if ((first & 0xF0) == 0xE0) {
        length = 3;
        cp = first & 0x0F;
        minimum = 0x800;
    } else if ((first & 0xF8) == 0xF0) {
        length = 4;
        cp = first & 0x07;
        minimum = 0x10000;
    } else {
        ++pos;
        return kReplacement;
    }
    if (pos + length > text.size()) {
        ++pos;
        return kReplacement;
    }
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char next = byte(pos + i);
        if ((next & 0xC0) != 0x80) {
            ++pos;
            return kReplacement;
        }
        cp = (cp << 6) | (next & 0x3F);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        ++pos;
        return kReplacement;
    }
    pos += length;
    return cp;
}

std::size_t previousCodePointStart(std::string_view text, std::size_t end) {
    if (end == 0) {
        return 0;
    }
    std::size_t start = end - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80 && end - start < 4) {
        --start;
    }
    return start;
}

std::u16string utf8ToUtf16(std::string_view utf8) {
    std::u16string out;
    out.reserve(utf8.size());
    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const char32_t cp = nextCodePoint(utf8, pos);
        if (cp >= 0x10000) {
            const char32_t v = cp - 0x10000;
            out += static_cast<char16_t>(0xD800 + (v >> 10));
            out += static_cast<char16_t>(0xDC00 + (v & 0x3FF));
        } else {
            out += static_cast<char16_t>(cp);
        }
    }
    return out;
}

std::string utf16ToUtf8(std::u16string_view utf16) {
    std::string out;
    out.reserve(utf16.size());
    for (std::size_t i = 0; i < utf16.size(); ++i) {
        const char16_t unit = utf16[i];
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < utf16.size() && utf16[i + 1] >= 0xDC00 &&
            utf16[i + 1] <= 0xDFFF) {
            const char32_t cp = 0x10000 + ((static_cast<char32_t>(unit) - 0xD800) << 10) +
                                (static_cast<char32_t>(utf16[i + 1]) - 0xDC00);
            appendUtf8(out, cp);
            ++i;
        } else if (unit >= 0xD800 && unit <= 0xDFFF) {
            appendUtf8(out, kReplacement);
        } else {
            appendUtf8(out, unit);
        }
    }
    return out;
}

std::string_view utf8Prefix(std::string_view text, std::size_t maxBytes) {
    if (text.size() <= maxBytes) {
        return text;
    }
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
        --end;
    }
    return text.substr(0, end);
}

namespace {

bool pictograph(char32_t c) {
    return (c >= 0x1F000 && c <= 0x1FAFF) || (c >= 0x2300 && c <= 0x23FF) || (c >= 0x2600 && c <= 0x27BF) ||
           (c >= 0x2B00 && c <= 0x2BFF) || (c >= 0xFE00 && c <= 0xFE0F) || c == 0x200D || c == 0x20E3 ||
           (c >= 0xE0000 && c <= 0xE007F);
}

// The letter or digit behind a styled one (mathematical bold, italic, script…: U+1D400–1D6A3, U+1D7CE–1D7FF;
// fullwidth), else 0.
char plainLatin(char32_t c) {
    if (c >= 0x1D400 && c <= 0x1D6A3) {
        const auto index = static_cast<int>((c - 0x1D400) % 52);
        return static_cast<char>(index < 26 ? 'A' + index : 'a' + index - 26);
    }
    if (c >= 0x1D7CE && c <= 0x1D7FF) {
        return static_cast<char>('0' + static_cast<int>((c - 0x1D7CE) % 10));
    }
    // the letterlike symbols that fill the holes of the styled alphabets above, and circled letters
    static const std::pair<char32_t, char> letterlike[] = {
        {0x2102, 'C'}, {0x210A, 'g'}, {0x210B, 'H'}, {0x210C, 'H'}, {0x210D, 'H'}, {0x210E, 'h'}, {0x2110, 'I'},
        {0x2111, 'I'}, {0x2112, 'L'}, {0x2113, 'l'}, {0x2115, 'N'}, {0x2119, 'P'}, {0x211A, 'Q'}, {0x211B, 'R'},
        {0x211C, 'R'}, {0x211D, 'R'}, {0x2124, 'Z'}, {0x2128, 'Z'}, {0x212C, 'B'}, {0x212D, 'C'}, {0x212F, 'e'},
        {0x2130, 'E'}, {0x2131, 'F'}, {0x2133, 'M'}, {0x2134, 'o'},
    };
    for (const auto& entry : letterlike) {
        if (c == entry.first) {
            return entry.second;
        }
    }
    if (c >= 0x24B6 && c <= 0x24CF) {
        return static_cast<char>('A' + static_cast<int>(c - 0x24B6));
    }
    if (c >= 0x24D0 && c <= 0x24E9) {
        return static_cast<char>('a' + static_cast<int>(c - 0x24D0));
    }
    if (c >= 0xFF10 && c <= 0xFF19) {
        return static_cast<char>('0' + static_cast<int>(c - 0xFF10));
    }
    if (c >= 0xFF21 && c <= 0xFF3A) {
        return static_cast<char>('A' + static_cast<int>(c - 0xFF21));
    }
    if (c >= 0xFF41 && c <= 0xFF5A) {
        return static_cast<char>('a' + static_cast<int>(c - 0xFF41));
    }
    return 0;
}

}  // namespace

std::string withoutEmoji(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t start = pos;
        const char32_t c = nextCodePoint(text, pos);
        if (pictograph(c)) {
            continue;
        }
        if (const char plain = plainLatin(c)) {
            out.push_back(plain);
            continue;
        }
        if (c == U' ' && (out.empty() || out.back() == ' ')) {
            continue;
        }
        out.append(text.substr(start, pos - start));
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

}  // namespace playtube::core
