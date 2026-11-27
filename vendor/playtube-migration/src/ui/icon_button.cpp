#include "ui/icon_button.hpp"

#include <nanovg.h>

#include <algorithm>
#include <cmath>

#include "ui/draw.hpp"
#include "ui/i18n.hpp"
#include "ui/text.hpp"

namespace playtube::ui {

namespace {

constexpr float kSize = 84;
constexpr float kIconPx = 39;
constexpr float kStrokePx = 3;
constexpr float kInitialDiameter = 51;
constexpr float kInitialText = 22;
constexpr float kPillText = 22;
constexpr float kPillPadX = 18;
constexpr float kPillPadY = 9;
constexpr float kPillRadius = 12;
constexpr float kPillGap = 15;
constexpr float kPi = 3.14159265f;

void line(NVGcontext* vg, float x0, float y0, float x1, float y1) {
    nvgMoveTo(vg, x0, y0);
    nvgLineTo(vg, x1, y1);
}

void paint(NVGcontext* vg, bool filled, Color color) {
    if (filled) {
        nvgFillColor(vg, nvgColor(color));
        nvgFill(vg);
    }
    nvgStrokeColor(vg, nvgColor(color));
    nvgStroke(vg);
}

}  // namespace

IconButton::IconButton(Icon icon, std::string label, std::function<void()> onPress)
    : icon_(icon), label_(std::move(label)), pill_(label_), letter_({}, FontStyle{kInitialText, FontWeight::Bold}) {
    width = SizeSpec::fixed(kSize);
    height = SizeSpec::fixed(kSize);
    focusable = true;
    drawsOwnFocus = true;
    bind(platform::Action::Confirm, tr("ui.select"), [onPress = std::move(onPress)] {
        if (onPress) {
            onPress();
        }
        return true;
    });
}

void IconButton::setIcon(Icon icon) {
    icon_ = icon;
    markDirty();
}

void IconButton::setLabel(std::string label) {
    label_ = std::move(label);
    pill_.setText(label_);
    markDirty();
}

void IconButton::setPicture(std::string url) {
    if (url == picture_.url()) {
        return;
    }
    picture_.setUrl(std::move(url));
    markDirty();
}

void IconButton::setInitial(std::string letter) {
    if (letter == initial_) {
        return;
    }
    initial_ = std::move(letter);
    letter_.setText(initial_);
    markDirty();
}

void IconButton::setLabelAlign(CrossAlign align) {
    labelAlign_ = align;
    markDirty();
}

void IconButton::onFocusChanged(bool focused) {
    focused_ = focused;
    markDirty();
}

Color IconButton::fillColor() const { return focused_ ? Color{0xFF, 0xFF, 0xFF, 0xFF} : Color{0xFF, 0xFF, 0xFF, 0x24}; }

Color IconButton::iconColor() const { return focused_ ? Color{0x0A, 0x0A, 0x0C, 0xFF} : theme().text; }

void IconButton::draw(NVGcontext* vg) const {
    nvgBeginPath(vg);
    nvgCircle(vg, frame.width / 2, frame.height / 2, std::min(frame.width, frame.height) / 2);
    nvgFillColor(vg, nvgColor(fillColor()));
    nvgFill(vg);

    if (!initial_.empty()) {
        const float cx = frame.width / 2;
        const float cy = frame.height / 2;
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, kInitialDiameter / 2);
        nvgFillColor(vg, nvgRGBA(0x5B, 0x61, 0x70, 0xFF));
        nvgFill(vg);
        nvgSave(vg);  // as every label (and the channel line's avatar): capitals in the middle, vowel marks
        nvgTranslate(vg, letter_.frame.x, letter_.frame.y);
        letter_.draw(vg);
        nvgRestore(vg);
        if (!picture_.url().empty()) {
            nvgSave(vg);
            nvgTranslate(vg, picture_.frame.x, picture_.frame.y);
            picture_.draw(vg);
            nvgRestore(vg);
        }
    } else {
        drawIcon(vg, iconColor());
    }
    if (focused_ && !label_.empty()) {
        drawLabel(vg);
    }
}

void IconButton::layout(TextMetrics& metrics) {
    View::layout(metrics);
    pill_.style = FontStyle{kPillText, FontWeight::SemiBold};
    pill_.color = theme().text;
    pill_.maxLines = 1;
    const Size text = pill_.measure({1000, 100}, metrics);
    pill_.place({0, 0, text.width, text.height}, metrics);
    pillW_ = text.width + 2 * kPillPadX;
    pillH_ = text.height + 2 * kPillPadY;
    // the initial: its line's box centred on the button, as a centred label in the channel line's avatar
    letter_.color = Color{0xFF, 0xFF, 0xFF, 0xFF};
    const Size letter = letter_.measure({kInitialDiameter, kInitialDiameter}, metrics);
    letter_.place({(frame.width - letter.width) / 2, (frame.height - letter.height) / 2, letter.width, letter.height},
                  metrics);
    picture_.cornerRadius = kInitialDiameter / 2;
    picture_.placeholder = Color{0, 0, 0, 0};
    picture_.place({(frame.width - kInitialDiameter) / 2, (frame.height - kInitialDiameter) / 2, kInitialDiameter,
                    kInitialDiameter},
                   metrics);
}

void IconButton::onThemeChanged() {
    markDirty();
    View::onThemeChanged();
}

void IconButton::drawLabel(NVGcontext* vg) const {
    const float x = labelAlign_ == CrossAlign::End ? frame.width - pillW_ : (frame.width - pillW_) / 2;
    const float y = -kPillGap - pillH_;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, pillW_, pillH_, kPillRadius);
    nvgFillColor(vg, nvgRGBA(0x07, 0x07, 0x08, 0xE0));  // 88 %
    nvgFill(vg);
    nvgSave(vg);
    nvgTranslate(vg, x + kPillPadX, y + kPillPadY);
    pill_.draw(vg);  // as every label: right-to-left, vowel marks, vertical placement
    nvgRestore(vg);
}

void IconButton::drawIcon(NVGcontext* vg, Color color) const {
    const float scale = kIconPx / 24;
    nvgSave(vg);
    nvgTranslate(vg, (frame.width - kIconPx) / 2, (frame.height - kIconPx) / 2);
    nvgScale(vg, scale, scale);
    nvgStrokeWidth(vg, kStrokePx / scale);
    nvgLineCap(vg, NVG_ROUND);
    nvgLineJoin(vg, NVG_ROUND);
    const bool filled = icon_ == Icon::LikeFilled || icon_ == Icon::DislikeFilled || icon_ == Icon::BellTicked;
    switch (icon_) {
    case Icon::Pause:
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 6, 5, 4, 14, 1);
        nvgRoundedRect(vg, 14, 5, 4, 14, 1);
        nvgFillColor(vg, nvgColor(color));
        nvgFill(vg);
        break;
    case Icon::Play:
        nvgBeginPath(vg);
        nvgMoveTo(vg, 8, 5);
        nvgLineTo(vg, 8, 19);
        nvgLineTo(vg, 19, 12);
        nvgClosePath(vg);
        paint(vg, false, color);
        break;
    case Icon::Restart:
        nvgBeginPath(vg);
        nvgArc(vg, 12, 12, 8, -kPi * 0.2f, kPi * 1.5f, NVG_CW);  // from the upper right round to the top
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        nvgBeginPath(vg);  // the arrow head at the arc's end (the top, heading right)
        nvgMoveTo(vg, 9, 1);
        nvgLineTo(vg, 12, 4);
        nvgLineTo(vg, 9, 7);
        nvgStroke(vg);
        break;
    case Icon::Like:
    case Icon::LikeFilled:
    case Icon::Dislike:
    case Icon::DislikeFilled: {
        const bool down = icon_ == Icon::Dislike || icon_ == Icon::DislikeFilled;
        if (down) {  // the like turned half a turn about the box's centre
            nvgTranslate(vg, 12, 12);
            nvgRotate(vg, kPi);
            nvgTranslate(vg, -12, -12);
        }
        nvgBeginPath(vg);
        nvgMoveTo(vg, 7, 10);
        nvgLineTo(vg, 7, 21);
        nvgLineTo(vg, 4, 21);
        nvgLineTo(vg, 4, 10);
        nvgClosePath(vg);
        paint(vg, filled, color);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 7, 10);
        nvgLineTo(vg, 11, 3);
        nvgBezierTo(vg, 12.5f, 3, 13.5f, 4, 13.5f, 5.5f);
        nvgLineTo(vg, 13.5f, 9);
        nvgLineTo(vg, 18.7f, 9);
        nvgQuadTo(vg, 20.7f, 9, 20.7f, 11.3f);
        nvgLineTo(vg, 19.4f, 18.8f);
        nvgQuadTo(vg, 19.4f, 20.5f, 17.4f, 20.5f);
        nvgLineTo(vg, 7, 20.5f);
        paint(vg, filled, color);
        break;
    }
    case Icon::Comments:
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4, 5);
        nvgLineTo(vg, 20, 5);
        nvgLineTo(vg, 20, 16);
        nvgLineTo(vg, 9, 16);
        nvgLineTo(vg, 4, 20);
        nvgClosePath(vg);
        paint(vg, false, color);
        break;
    case Icon::LiveChat:
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4, 5);
        nvgLineTo(vg, 20, 5);
        nvgLineTo(vg, 20, 16);
        nvgLineTo(vg, 9, 16);
        nvgLineTo(vg, 4, 20);
        nvgClosePath(vg);
        paint(vg, false, color);
        nvgBeginPath(vg);
        line(vg, 8, 9, 16, 9);
        line(vg, 8, 12.5f, 13, 12.5f);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        break;
    case Icon::Save:
        nvgBeginPath(vg);
        nvgMoveTo(vg, 6, 3);
        nvgLineTo(vg, 18, 3);
        nvgLineTo(vg, 18, 21);
        nvgLineTo(vg, 12, 17);
        nvgLineTo(vg, 6, 21);
        nvgClosePath(vg);
        paint(vg, false, color);
        break;
    case Icon::Bell:
    case Icon::BellTicked:
        nvgBeginPath(vg);
        nvgMoveTo(vg, 6, 17);
        nvgLineTo(vg, 6, 11);
        nvgArc(vg, 12, 11, 6, kPi, 2 * kPi, NVG_CW);
        nvgLineTo(vg, 18, 17);
        nvgLineTo(vg, 20, 19);
        nvgLineTo(vg, 4, 19);
        nvgLineTo(vg, 6, 17);
        nvgClosePath(vg);
        paint(vg, filled, color);
        nvgBeginPath(vg);
        line(vg, 10, 21, 14, 21);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        if (icon_ == Icon::BellTicked) {  // a tick cut out of the filled bell
            nvgBeginPath(vg);
            nvgMoveTo(vg, 9, 12.5f);
            nvgLineTo(vg, 11, 14.5f);
            nvgLineTo(vg, 15, 10);
            nvgStrokeColor(vg, nvgColor(focused_ ? Color{0xFF, 0xFF, 0xFF, 0xFF} : theme().background));
            nvgStroke(vg);
        }
        break;
    case Icon::Queue:
        nvgBeginPath(vg);
        line(vg, 4, 6, 20, 6);
        line(vg, 4, 11, 20, 11);
        line(vg, 4, 16, 10, 16);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 14, 14);
        nvgLineTo(vg, 14, 20);
        nvgLineTo(vg, 19, 17);
        nvgClosePath(vg);
        paint(vg, false, color);
        break;
    case Icon::Captions:
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 3, 5, 18, 14, 2);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        nvgBeginPath(vg);  // the two "C": arcs opening to the right
        nvgArc(vg, 11.32f, 12, 2, -2.294f, -3.989f, NVG_CCW);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgArc(vg, 17.82f, 12, 2, -2.294f, -3.989f, NVG_CCW);
        nvgStroke(vg);
        break;
    case Icon::Channel:  // a person: head and shoulders (until the channel's initial is known)
        nvgBeginPath(vg);
        nvgCircle(vg, 12, 8, 4);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, 4.5f, 20.5f);
        nvgBezierTo(vg, 4.5f, 16, 8, 14, 12, 14);
        nvgBezierTo(vg, 16, 14, 19.5f, 16, 19.5f, 20.5f);
        nvgStroke(vg);
        break;
    case Icon::Settings: {
        nvgBeginPath(vg);
        nvgCircle(vg, 12, 12, 3);
        nvgStrokeColor(vg, nvgColor(color));
        nvgStroke(vg);
        nvgBeginPath(vg);  // a toothed wheel: 8 teeth between radius 7.3 and 9.8
        constexpr int kTeeth = 8;
        for (int i = 0; i < kTeeth * 4; ++i) {
            const float angle = (static_cast<float>(i) - 0.5f) * 2 * kPi / (kTeeth * 4);
            const float radius = ((i + 3) % 4 < 2) ? 9.8f : 7.3f;  // two points out, two in
            const float x = 12 + radius * static_cast<float>(std::cos(angle));
            const float y = 12 + radius * static_cast<float>(std::sin(angle));
            if (i == 0) {
                nvgMoveTo(vg, x, y);
            } else {
                nvgLineTo(vg, x, y);
            }
        }
        nvgClosePath(vg);
        nvgStroke(vg);
        break;
    }
    }
    nvgRestore(vg);
}

}  // namespace playtube::ui
