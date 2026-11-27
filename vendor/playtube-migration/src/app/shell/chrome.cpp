#include "app/shell/chrome.hpp"

#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "ui/box.hpp"
#include "ui/draw.hpp"
#include "ui/i18n.hpp"
#include "ui/label.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

// a label in the accent colour, following the accent setting
class AccentLabel : public ui::Label {
public:
    AccentLabel(std::string text, ui::FontStyle style) : ui::Label(std::move(text), style) {
        color = ui::theme().accent;
    }
    void onThemeChanged() override {
        color = ui::theme().accent;
        ui::Label::onThemeChanged();
    }
};

class Avatar : public ui::Stack {
public:
    explicit Avatar(const std::string& initial) {
        width = ui::SizeSpec::fixed(48);
        height = ui::SizeSpec::fixed(48);
        cornerRadius = 24;
        horizontal = ui::CrossAlign::Center;
        vertical = ui::CrossAlign::Center;
        label_ = add<ui::Label>(initial, ui::FontStyle{22, ui::FontWeight::Bold});
        onThemeChanged();
    }
    void onThemeChanged() override {
        background = ui::theme().accent;
        label_->color = ui::theme().onAccent;
        ui::Stack::onThemeChanged();
    }

private:
    ui::Label* label_;
};


}  // namespace

int progressPixel(double position, double duration, float width) {
    if (duration <= 0 || width <= 0) {
        return -1;
    }
    const double part = std::min(1.0, std::max(0.0, position / duration));
    return static_cast<int>(std::lround(part * static_cast<double>(width)));
}

std::string compactCount(long long count) {
    count = std::max(0LL, count);
    // by thousands, or by ten thousands (万, 만)
    const long long base = ui::tr("number.compact_base") == "10000" ? 10000 : 1000;
    if (count < base) {
        return std::to_string(count);
    }
    long long unit = base;
    int level = 1;
    while (level < 3 && count / unit >= base) {
        unit *= base;
        ++level;
    }
    const long long whole = count / unit;
    std::string number = std::to_string(whole);
    if (whole < 10) {
        if (const long long tenth = count % unit * 10 / unit; tenth > 0) {  // cut, as YouTube does
            number += ui::tr("number.decimal") + std::to_string(tenth);
        }
    }
    return ui::tr("number.compact_" + std::to_string(level), {number});
}

std::string speedText(double speed) {
    if (speed == 1) {
        return ui::tr("player.speed.normal");
    }
    char text[16];
    std::snprintf(text, sizeof text, "%g", speed);
    std::string out = text;
    if (const std::size_t dot = out.find('.'); dot != std::string::npos) {
        out.replace(dot, 1, ui::tr("number.decimal"));
    }
    return out + "\xC3\x97";
}

std::string clockText(double seconds) {
    const long total = seconds > 0 ? static_cast<long>(seconds) : 0;
    char text[32];
    if (total >= 3600) {
        std::snprintf(text, sizeof text, "%ld:%02ld:%02ld", total / 3600, total / 60 % 60, total % 60);
    } else {
        std::snprintf(text, sizeof text, "%ld:%02ld", total / 60, total % 60);
    }
    return text;
}

std::string errorText(const core::Error& error) {
    switch (error.kind) {
    case core::ErrorKind::Network:
    case core::ErrorKind::InvalidAddress:
        return ui::tr("shell.error.unreachable");
    case core::ErrorKind::Timeout:
        return ui::tr("shell.error.timeout");
    case core::ErrorKind::Tls:
    case core::ErrorKind::CertificateUntrusted:
        return ui::tr("shell.error.tls");
    case core::ErrorKind::Unauthorized:
        return ui::tr("shell.error.signed_out");
    default:
        return ui::tr("shell.error.generic");
    }
}

ui::Box* addMessage(ui::Box& column, const std::string& text, const std::string& buttonText,
                    std::function<void()> onPress) {
    auto* box = column.add<ui::Box>(ui::Axis::Column);
    box->gap = ui::theme().spacingM;
    box->padding.top = ui::theme().spacingL;
    auto* label = box->add<ui::Label>(text, bodyText());
    label->color = ui::theme().textSecondary;
    label->maxLines = 3;
    label->width = ui::SizeSpec::fixed(1200);
    if (!buttonText.empty()) {
        box->add<ui::Button>(buttonText, std::move(onPress));
    }
    return box;
}

ui::FontStyle bodyText() { return {ui::theme().fontBody, ui::FontWeight::Regular}; }
ui::FontStyle bodyStrong() { return {ui::theme().fontBody, ui::FontWeight::SemiBold}; }
ui::FontStyle headingText() { return {ui::theme().fontHeading, ui::FontWeight::SemiBold}; }
ui::FontStyle titleText(ui::FontWeight weight) { return {ui::theme().fontTitle, weight}; }

ui::Insets rowPadding() {
    return {ui::theme().spacingM, ui::theme().spacingL, ui::theme().spacingM, ui::theme().spacingL};
}

ui::Insets pagePadding() { return {ui::theme().spacingXxl, 96, 120, 96}; }

std::string initialOf(const std::string& name) {
    if (name.empty()) {
        return "?";
    }
    std::size_t length = 1;
    const auto lead = static_cast<unsigned char>(name[0]);
    if (lead >= 0xF0) {
        length = 4;
    } else if (lead >= 0xE0) {
        length = 3;
    } else if (lead >= 0xC0) {
        length = 2;
    }
    std::string initial = name.substr(0, length);
    if (initial.size() == 1 && initial[0] >= 'a' && initial[0] <= 'z') {
        initial[0] = static_cast<char>(initial[0] - 'a' + 'A');
    }
    return initial;
}

namespace {

std::shared_ptr<const ui::SvgPicture>& logoPicture() {
    static std::shared_ptr<const ui::SvgPicture> picture;
    return picture;
}

// "Playtube." in text, or the logo picture while there is one
class Logo : public ui::Box {
public:
    Logo() : ui::Box(ui::Axis::Row) {
        const ui::FontStyle style{ui::theme().fontLogo, ui::FontWeight::Bold};
        add<ui::Label>("Playtube", style);
        add<AccentLabel>(".", style);
        all().push_back(this);
    }
    ~Logo() override {
        auto& logos = all();
        logos.erase(std::remove(logos.begin(), logos.end(), this), logos.end());
    }
    static std::vector<Logo*>& all() {
        static std::vector<Logo*> logos;
        return logos;
    }
    ui::Size measure(ui::Size available, ui::TextMetrics& metrics) override {
        if (const auto& picture = logoPicture()) {
            return {static_cast<float>(picture->width), static_cast<float>(picture->height)};
        }
        return ui::Box::measure(available, metrics);
    }
    void draw(NVGcontext* vg) const override {
        const auto& picture = logoPicture();
        if (!picture) {
            ui::Box::draw(vg);
            return;
        }
        // one image per canvas, made again when the picture changes
        struct Cached {
            NVGcontext* vg = nullptr;
            const ui::SvgPicture* picture = nullptr;
            int image = 0;
        };
        static Cached cached;
        if (cached.vg != vg || cached.picture != picture.get()) {
            if (cached.vg == vg && cached.image > 0) {
                nvgDeleteImage(vg, cached.image);
            }
            cached = {vg, picture.get(),
                      nvgCreateImageRGBA(vg, picture->width, picture->height, NVG_IMAGE_PREMULTIPLIED,
                                         picture->pixels.data())};
        }
        if (cached.image <= 0) {
            ui::Box::draw(vg);
            return;
        }
        const float w = static_cast<float>(picture->width);
        const float h = static_cast<float>(picture->height);
        const float y = static_cast<float>(std::round(static_cast<double>(frame.height - h) / 2.0));
        nvgBeginPath(vg);
        nvgRect(vg, 0, y, w, h);
        nvgFillPaint(vg, nvgImagePattern(vg, 0, y, w, h, 0, cached.image, 1));
        nvgFill(vg);
    }
};

}  // namespace

std::unique_ptr<ui::View> makeLogo() { return std::make_unique<Logo>(); }

void setLogoPicture(std::shared_ptr<const ui::SvgPicture> picture) {
    logoPicture() = std::move(picture);
    for (Logo* logo : Logo::all()) {
        logo->markDirty();
    }
}

bool logoPictureShown() { return logoPicture() != nullptr; }

std::unique_ptr<ui::View> makeAvatar(const std::string& userName) { return std::make_unique<Avatar>(initialOf(userName)); }

BackdropShade::BackdropShade() {
    width = ui::SizeSpec::fill();
    height = ui::SizeSpec::fill();
}

void BackdropShade::draw(NVGcontext* vg) const {
    const ui::Color bg = ui::theme().background;
    const float split = frame.height * 0.7f;
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, frame.width, split);
    nvgFillPaint(vg, nvgLinearGradient(vg, 0, 0, 0, split, nvgRGBA(bg.r, bg.g, bg.b, 143), nvgRGBA(bg.r, bg.g, bg.b, 255)));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, 0, split, frame.width, frame.height - split);
    nvgFillColor(vg, nvgRGBA(bg.r, bg.g, bg.b, 255));
    nvgFill(vg);
}

}  // namespace playtube::app
