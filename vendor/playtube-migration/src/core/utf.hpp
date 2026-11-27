#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace playtube::core {

// Invalid input (bad bytes, overlong forms, encoded surrogates, unpaired surrogates) becomes U+FFFD.
std::u16string utf8ToUtf16(std::string_view utf8);
std::string utf16ToUtf8(std::u16string_view utf16);

// Decodes the code point at `pos` and advances `pos` past it (one byte on invalid input, giving U+FFFD).
char32_t nextCodePoint(std::string_view text, std::size_t& pos);
// Appends one code point in UTF-8 (a surrogate or a value past U+10FFFF: U+FFFD).
void appendCodePoint(std::string& out, char32_t codePoint);
// Start of the code point that ends at `end` (0 when end is 0).
std::size_t previousCodePointStart(std::string_view text, std::size_t end);

// `text` without emoji and pictographic symbols (no font of the app has them), spaces left around them merged;
// styled Latin letters and digits (mathematical bold, script…, fullwidth) as plain ones.
std::string withoutEmoji(std::string_view text);

// At most `maxBytes` bytes of `text`, ended on a character boundary.
std::string_view utf8Prefix(std::string_view text, std::size_t maxBytes);

}  // namespace playtube::core
