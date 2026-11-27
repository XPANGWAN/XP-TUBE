#pragma once

#include <string>
#include <vector>

#include "ui/bidi.hpp"
#include "ui/view.hpp"

namespace playtube::ui {

enum class TextAlign { Left, Center, Right };

// Text with wrapping, a line limit and an ellipsis. maxLines 0 = unlimited.
class Label : public View {
public:
    explicit Label(std::string text = {}, FontStyle styleValue = {});

    void setText(std::string text);
    const std::string& text() const { return text_; }
    const std::vector<std::string>& lines() const { return lines_; }

    FontStyle style;
    Color color;
    int maxLines = 1;
    TextAlign align = TextAlign::Left;

    Size measure(Size available, TextMetrics& metrics) override;
    void draw(NVGcontext* vg) const override;

protected:
    void layout(TextMetrics& metrics) override;

private:
    void drawMarks(NVGcontext* vg, const DrawnLine& line, float x, float y) const;

private:
    std::string text_;
    std::vector<std::string> lines_;
    std::vector<DrawnLine> drawn_;  // lines_ as drawn (right-to-left runs, vowel marks over their letters)
    float lineHeight_ = 0;
    float shift_ = 0;  // capitals in the middle of each line (topAlignedShift)
    float linesWidth_ = -1;  // width the lines were broken for (cache)
    // the last measure() and what it depended on: a page's layout asks again for every label on each change, and
    // breaking text into lines is the dearest part of an update on the console
    struct Measured {
        bool valid = false;
        float availableWidth = 0;
        float size = 0;
        FontWeight weight = FontWeight::Regular;
        int maxLines = 0;
        Insets padding;
        const TextMetrics* metrics = nullptr;
        unsigned generation = 0;
        Size result;
    };
    Measured measured_;
};

}  // namespace playtube::ui
