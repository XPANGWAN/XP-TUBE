#include "ui/label.hpp"

#include <nanovg.h>

#include <algorithm>

#include "ui/bidi.hpp"
#include "ui/draw.hpp"
#include "ui/theme.hpp"

namespace playtube::ui {

Label::Label(std::string text, FontStyle styleValue) : style(styleValue), color(theme().text), text_(std::move(text)) {}

void Label::setText(std::string text) {
    if (text == text_) {
        return;
    }
    text_ = std::move(text);
    linesWidth_ = -1;
    measured_.valid = false;
    markDirty();
}

Size Label::measure(Size available, TextMetrics& metrics) {
    const Measured& m = measured_;
    if (m.valid && m.availableWidth == available.width && m.size == style.size && m.weight == style.weight &&
        m.maxLines == maxLines && m.padding == padding && m.metrics == &metrics && m.generation == themeGeneration()) {
        return m.result;
    }
    const float maxWidth = std::max(0.0f, available.width - padding.horizontal());
    const auto lines = metrics.breakLines(text_, style, maxWidth, maxLines);
    float widest = 0;
    for (const auto& line : lines) {
        widest = std::max(widest, metrics.measure(line, style));
    }
    const Size result{widest + padding.horizontal(),
                      metrics.lineHeight(style) * static_cast<float>(lines.size()) + padding.vertical()};
    measured_ = Measured{true,      available.width, style.size,         style.weight, maxLines,
                         padding,   &metrics,        themeGeneration(), result};
    return result;
}

void Label::layout(TextMetrics& metrics) {
    const float width = std::max(0.0f, frame.width - padding.horizontal());
    if (width != linesWidth_ || needsLayout()) {
        lines_ = metrics.breakLines(text_, style, width, maxLines);
        drawn_.clear();
        for (const std::string& line : lines_) {
            drawn_.push_back(drawnLine(line));  // right-to-left runs as they are drawn
        }
        linesWidth_ = width;
    }
    lineHeight_ = metrics.lineHeight(style);
    shift_ = topAlignedShift(style.size, lineHeight_);
}

void Label::draw(NVGcontext* vg) const {
    drawBackground(vg);
    nvgFontFace(vg, fontFace(style.weight));
    nvgFontSize(vg, style.size);
    nvgFillColor(vg, nvgColor(color));
    int alignFlags = NVG_ALIGN_TOP;
    float x = padding.left;
    if (align == TextAlign::Center) {
        alignFlags |= NVG_ALIGN_CENTER;
        x = padding.left + (frame.width - padding.horizontal()) / 2;
    } else if (align == TextAlign::Right) {
        alignFlags |= NVG_ALIGN_RIGHT;
        x = frame.width - padding.right;
    } else {
        alignFlags |= NVG_ALIGN_LEFT;
    }
    float y = padding.top + shift_;
    for (const DrawnLine& line : drawn_) {
        // a right-to-left paragraph against the right edge where the label reads from the left
        const bool right = line.rightToLeft && align == TextAlign::Left;
        const int flags = right ? NVG_ALIGN_TOP | NVG_ALIGN_RIGHT : alignFlags;
        const float lineX = right ? frame.width - padding.right : x;
        nvgTextAlign(vg, flags);
        const char* end = line.text.c_str() + line.text.size();
        nvgText(vg, lineX, y, line.text.c_str(), end);
        if (!line.marks.empty()) {
            drawMarks(vg, line, lineX, y);
        }
        y += lineHeight_;
    }
}

// Each vowel mark centred over (or under) its letter, the second one on the same side a step further.
void Label::drawMarks(NVGcontext* vg, const DrawnLine& line, float x, float y) const {
    constexpr int kMaxGlyphs = 512;
    NVGglyphPosition glyphs[kMaxGlyphs];
    const int count =
        nvgTextGlyphPositions(vg, x, y, line.text.c_str(), line.text.c_str() + line.text.size(), glyphs, kMaxGlyphs);
    nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
    for (const DrawnLine::Mark& mark : line.marks) {
        if (mark.base >= static_cast<std::size_t>(count)) {
            continue;
        }
        const NVGglyphPosition& letter = glyphs[mark.base];
        NVGglyphPosition ink;
        if (nvgTextGlyphPositions(vg, 0, 0, mark.text.c_str(), nullptr, &ink, 1) != 1) {
            continue;
        }
        const float centre = (letter.minx + letter.maxx) / 2 - (ink.minx + ink.maxx) / 2;
        const float step = static_cast<float>(mark.stack) * style.size * 0.3f * (mark.below ? 1.0f : -1.0f);
        nvgText(vg, centre, y + step, mark.text.c_str(), nullptr);
    }
}

}  // namespace playtube::ui
