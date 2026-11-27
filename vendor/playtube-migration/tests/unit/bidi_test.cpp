#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/utf.hpp"
#include "ui/bidi.hpp"

using namespace playtube;

namespace {

std::vector<char32_t> codePoints(const std::string& text) {
    std::vector<char32_t> out;
    std::size_t pos = 0;
    while (pos < text.size()) {
        out.push_back(core::nextCodePoint(text, pos));
    }
    return out;
}

}  // namespace

TEST_CASE("bidi: text with no right-to-left letter is drawn as it is") {
    for (const char* text : {"", "Lofi Girl", "Ça va ? 2026", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",
                             "\xE0\xB8\xAA\xE0\xB8\xA7\xE0\xB8\xB1\xE0\xB8\xAA\xE0\xB8\x94\xE0\xB8\xB5"}) {  // Thai
        CHECK(ui::visualOrder(text) == text);
        CHECK_FALSE(ui::hasRightToLeft(text));
    }
}

TEST_CASE("bidi: Hebrew in visual order; Arabic joined (presentation forms) then in visual order") {
    // שלום
    CHECK(codePoints(ui::visualOrder("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D")) ==
          std::vector<char32_t>{0x05DD, 0x05D5, 0x05DC, 0x05E9});
    // سلام: seen initial, lam-alef ligature (final), meem isolated; right to left
    CHECK(codePoints(ui::visualOrder("\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85")) ==
          std::vector<char32_t>{0xFEE1, 0xFEFC, 0xFEB3});
    CHECK(ui::hasRightToLeft("Live \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D"));
}

TEST_CASE("bidi: a left-to-right title keeps its order around a right-to-left word; numbers stay readable") {
    // "Live שלום 2026": the Hebrew word and the number after it form the right-to-left run
    const auto line = codePoints(ui::visualOrder("Live \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D 2026"));
    const std::vector<char32_t> expected{'L', 'i', 'v', 'e', ' ', '2', '0', '2', '6', ' ', 0x05DD, 0x05D5, 0x05DC, 0x05E9};
    CHECK(line == expected);
    // "سلام 2026 TV": an Arabic title (right to left), its number left to right inside it
    const auto arabic = codePoints(ui::visualOrder("\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85 2026 TV"));
    const std::vector<char32_t> expectedArabic{'T', 'V', ' ', '2', '0', '2', '6', ' ', 0xFEE1, 0xFEFC, 0xFEB3};
    CHECK(arabic == expectedArabic);
}

TEST_CASE("bidi: a drawn line takes the vowel marks out, each with its letter; right-to-left paragraphs say so") {
    // مُحَمَّد: meem damma, hah fatha, meem shadda fatha, dal
    const auto line = ui::drawnLine("\xD9\x85\xD9\x8F\xD8\xAD\xD9\x8E\xD9\x85\xD9\x91\xD9\x8E\xD8\xAF");
    CHECK(line.rightToLeft);
    CHECK(codePoints(line.text) == std::vector<char32_t>{0xFEAA, 0xFEE4, 0xFEA4, 0xFEE3});  // the letters, joined
    REQUIRE(line.marks.size() == 4);
    CHECK(codePoints(line.marks[0].text) == std::vector<char32_t>{0x0651});  // shadda on the middle meem
    CHECK(line.marks[0].base == 1);
    CHECK(line.marks[0].stack == 0);
    CHECK(codePoints(line.marks[1].text) == std::vector<char32_t>{0x064E});  // its fatha, above the shadda
    CHECK(line.marks[1].base == 1);
    CHECK(line.marks[1].stack == 1);
    CHECK(line.marks[2].base == 2);  // hah's fatha
    CHECK(line.marks[3].base == 3);  // the first meem's damma
    CHECK_FALSE(line.marks[3].below);
    // بِ: a kasra goes under its letter
    const auto kasra = ui::drawnLine("\xD8\xA8\xD9\x90");
    REQUIRE(kasra.marks.size() == 1);
    CHECK(kasra.marks[0].below);
    // a left-to-right paragraph stays so, even with a right-to-left word in it
    CHECK_FALSE(ui::drawnLine("Live \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D").rightToLeft);
    CHECK(ui::drawnLine("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D 2026").rightToLeft);
    CHECK(ui::drawnLine("Lofi Girl").text == "Lofi Girl");
    CHECK(ui::drawnLine("Lofi Girl").marks.empty());
}
