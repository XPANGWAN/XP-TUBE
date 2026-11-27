#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace playtube::ui {

// One line of text as it is drawn: right-to-left runs (Hebrew, Arabic…) in visual order, Arabic letters joined
// (their contextual presentation forms, FriBidi), the paragraph's direction from its first strong letter. Text
// with no right-to-left letter comes back unchanged (most text: no FriBidi call).
std::string visualOrder(std::string_view line);

// True when the text holds a letter of a right-to-left script.
bool hasRightToLeft(std::string_view text);

// A line ready to draw: visualOrder's text with the Arabic and Hebrew vowel marks taken out, and each mark with
// the letter it belongs to, so that it can be centred over (or under) that letter: without a shaper the fonts'
// zero-width marks would sit beside it.
struct DrawnLine {
    struct Mark {
        std::string text;      // the mark alone
        std::size_t base = 0;  // its letter: the index of that code point in `text`
        bool below = false;    // kasra, kasratan, Hebrew points under the letter…
        int stack = 0;         // the marks on the same side of the same letter before this one
    };
    std::string text;
    bool rightToLeft = false;  // a right-to-left paragraph: drawn against the right edge
    std::vector<Mark> marks;
};
DrawnLine drawnLine(std::string_view line);

}  // namespace playtube::ui
