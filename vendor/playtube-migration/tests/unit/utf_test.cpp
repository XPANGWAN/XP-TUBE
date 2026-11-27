#include <doctest/doctest.h>

#include <string>

#include "core/utf.hpp"

using playtube::core::utf16ToUtf8;
using playtube::core::utf8ToUtf16;

TEST_CASE("UTF-8 to UTF-16 and back") {
    CHECK(utf8ToUtf16("abc") == u"abc");
    CHECK(utf8ToUtf16("\xC3\xA9") == u"é");
    CHECK(utf8ToUtf16("\xE6\x97\xA5\xE6\x9C\xAC") == u"日本");
    CHECK(utf8ToUtf16("\xF0\x9F\x98\x80") == u"\U0001F600");
    for (const char* text : {"", "Playtube", "\xC3\xA9t\xC3\xA9", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "\xF0\x9F\x98\x80!"}) {
        CHECK(utf16ToUtf8(utf8ToUtf16(text)) == text);
    }
}

TEST_CASE("invalid sequences become U+FFFD") {
    CHECK(utf8ToUtf16("a\xFF" "b") == u"a�b");
    CHECK(utf8ToUtf16("\xC3") == u"�");
    CHECK(utf8ToUtf16("\xC0\xAF") == u"��");      // overlong
    CHECK(utf8ToUtf16("\xED\xA0\x80") == u"���");  // encoded surrogate
    std::u16string lone;
    lone += static_cast<char16_t>(0xD800);
    lone += u'x';
    CHECK(utf16ToUtf8(lone) == "\xEF\xBF\xBDx");
}

TEST_CASE("code point stepping") {
    const std::string text = "a\xC3\xA9\xE6\x97\xA5";
    std::size_t pos = 0;
    CHECK(playtube::core::nextCodePoint(text, pos) == U'a');
    CHECK(playtube::core::nextCodePoint(text, pos) == U'é');
    CHECK(playtube::core::nextCodePoint(text, pos) == U'日');
    CHECK(pos == text.size());
    CHECK(playtube::core::previousCodePointStart(text, text.size()) == 3);
    CHECK(playtube::core::previousCodePointStart(text, 3) == 1);
    CHECK(playtube::core::previousCodePointStart(text, 0) == 0);
}

TEST_CASE("utf8Prefix never cuts a character") {
    using playtube::core::utf8Prefix;
    CHECK(utf8Prefix("abc", 10) == "abc");
    CHECK(utf8Prefix("abc", 2) == "ab");
    CHECK(utf8Prefix("a\xC3\xA9", 2) == "a");           // é is 2 bytes
    CHECK(utf8Prefix("\xE6\x97\xA5\xE6\x9C\xAC", 4) == "\xE6\x97\xA5");
    CHECK(utf8Prefix("", 3).empty());
}

TEST_CASE("withoutEmoji drops pictographs the fonts do not have, and the spaces they leave") {
    using playtube::core::withoutEmoji;
    CHECK(withoutEmoji("lofi hip hop radio \xF0\x9F\x93\x9A beats to relax") == "lofi hip hop radio beats to relax");
    CHECK(withoutEmoji("\xF0\x9F\x8E\x83 spooky") == "spooky");
    CHECK(withoutEmoji("Coffee \xE2\x98\x95\xEF\xB8\x8F Morning") == "Coffee Morning");    // ☕️
    CHECK(withoutEmoji("Family \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9 day") == "Family day");  // ZWJ sequence
    CHECK(withoutEmoji("Fran\xC3\xA7" "ais \xE6\x97\xA5\xE6\x9C\xAC") == "Fran\xC3\xA7" "ais \xE6\x97\xA5\xE6\x9C\xAC");
    CHECK(withoutEmoji("end \xF0\x9F\x94\xA5") == "end");
    CHECK(withoutEmoji("").empty());
}

TEST_CASE("withoutEmoji also turns styled Latin letters and digits into plain ones") {
    using playtube::core::withoutEmoji;
    // bold 𝐋𝐨𝐟𝐢
    CHECK(withoutEmoji("\xF0\x9D\x90\x8B\xF0\x9D\x90\xA8\xF0\x9D\x90\x9F\xF0\x9D\x90\xA2") == "Lofi");
    CHECK(withoutEmoji("\xF0\x9D\x93\x9B\xF0\x9D\x93\xB8") == "Lo");                                    // script 𝓛𝓸
    CHECK(withoutEmoji("\xF0\x9D\x9F\x8F\xF0\x9D\x9F\x90") == "12");                                    // bold 𝟏𝟐
    CHECK(withoutEmoji("\xEF\xBD\x8C\xEF\xBD\x8F\xEF\xBC\x91") == "lo1");                               // fullwidth ｌｏ１
}

TEST_CASE("withoutEmoji folds the letterlike symbols that complete the styled alphabets, and circled letters") {
    using playtube::core::withoutEmoji;
    CHECK(withoutEmoji("\xE2\x84\xAC\xE2\x84\xB0") == "BE");          // ℬ ℰ (script capitals)
    CHECK(withoutEmoji("\xE2\x84\x9D\xE2\x84\xA4") == "RZ");          // ℝ ℤ (double-struck)
    CHECK(withoutEmoji("\xE2\x84\xB4\xE2\x84\xAF") == "oe");          // ℴ ℯ
    CHECK(withoutEmoji("\xE2\x93\x81\xE2\x93\x9E") == "Lo");          // Ⓛ ⓞ
}
