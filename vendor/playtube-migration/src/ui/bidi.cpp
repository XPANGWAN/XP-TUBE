#include "ui/bidi.hpp"

#include <fribidi.h>

#include <vector>

#include "core/utf.hpp"

namespace playtube::ui {

namespace {

constexpr std::size_t kMaxLine = 4096;  // code points: longer lines are drawn as they are

bool rightToLeft(char32_t c) {
    return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF) ||
           (c >= 0x10800 && c <= 0x10FFF) || (c >= 0x1E800 && c <= 0x1EFFF);
}

// Arabic and Hebrew combining marks (vowels, shadda, sukun, Hebrew points and accents)
bool isMark(char32_t c) {
    return (c >= 0x0591 && c <= 0x05BD) || c == 0x05BF || c == 0x05C1 || c == 0x05C2 || c == 0x05C4 ||
           c == 0x05C5 || c == 0x05C7 || (c >= 0x0610 && c <= 0x061A) || (c >= 0x064B && c <= 0x065F) ||
           c == 0x0670 || (c >= 0x06D6 && c <= 0x06DC) || (c >= 0x06DF && c <= 0x06E4) || c == 0x06E7 ||
           c == 0x06E8 || (c >= 0x06EA && c <= 0x06ED) || (c >= 0x08D3 && c <= 0x08FF && c != 0x08E2);
}

bool isBelow(char32_t c) {
    // Arabic: kasratan, kasra, hamza below, subscript alef, small kasra, the low marks of Quranic text
    if (c == 0x064D || c == 0x0650 || c == 0x0655 || c == 0x0656 || c == 0x061A || c == 0x065C || c == 0x065F ||
        c == 0x06E3 || c == 0x06ED || c == 0x06EA) {
        return true;
    }
    // Hebrew points sit under the letter but holam, shin and sin dots, dagesh and the accents above
    return (c >= 0x05B0 && c <= 0x05BB && c != 0x05B9 && c != 0x05BA) || c == 0x05BD || c == 0x05C5 || c == 0x05C7;
}

struct Visual {
    std::vector<char32_t> chars;
    bool rightToLeft = false;
    bool ok = false;
};

Visual visual(std::string_view line) {
    Visual out;
    std::vector<FriBidiChar> logical;
    logical.reserve(line.size());
    std::size_t pos = 0;
    while (pos < line.size() && logical.size() < kMaxLine) {
        logical.push_back(core::nextCodePoint(line, pos));
    }
    if (pos < line.size()) {
        return out;
    }
    std::vector<FriBidiChar> drawn(logical.size());
    FriBidiParType direction = FRIBIDI_PAR_ON;  // from the first strong letter
    if (fribidi_log2vis(logical.data(), static_cast<FriBidiStrIndex>(logical.size()), &direction, drawn.data(),
                        nullptr, nullptr, nullptr) == 0) {
        return out;
    }
    for (FriBidiChar c : drawn) {
        if (c != 0xFEFF) {  // the place a joined lam-alef left (no width, not drawn)
            out.chars.push_back(static_cast<char32_t>(c));
        }
    }
    out.rightToLeft = FRIBIDI_IS_RTL(direction);
    out.ok = true;
    return out;
}

}  // namespace

bool hasRightToLeft(std::string_view text) {
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (rightToLeft(core::nextCodePoint(text, pos))) {
            return true;
        }
    }
    return false;
}

std::string visualOrder(std::string_view line) {
    if (!hasRightToLeft(line)) {
        return std::string(line);
    }
    const Visual drawn = visual(line);
    if (!drawn.ok) {
        return std::string(line);
    }
    std::string out;
    out.reserve(line.size());
    for (char32_t c : drawn.chars) {
        core::appendCodePoint(out, c);
    }
    return out;
}

DrawnLine drawnLine(std::string_view line) {
    DrawnLine out;
    if (!hasRightToLeft(line)) {
        out.text = std::string(line);
        return out;
    }
    const Visual drawn = visual(line);
    if (!drawn.ok) {
        out.text = std::string(line);
        return out;
    }
    out.rightToLeft = drawn.rightToLeft;
    std::size_t letters = 0;  // code points kept in out.text
    int above = 0;
    int below = 0;
    for (char32_t c : drawn.chars) {
        if (isMark(c) && letters > 0) {  // after its letter (FriBidi keeps marks after their base)
            DrawnLine::Mark mark;
            core::appendCodePoint(mark.text, c);
            mark.base = letters - 1;
            mark.below = isBelow(c);
            mark.stack = mark.below ? below++ : above++;
            out.marks.push_back(std::move(mark));
            continue;
        }
        core::appendCodePoint(out.text, c);
        ++letters;
        above = below = 0;
    }
    return out;
}

}  // namespace playtube::ui
