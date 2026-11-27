#include "app/shell/nav_bar.hpp"

#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/shell/chrome.hpp"
#include "app/shell/clock.hpp"
#include "app/shell/screens.hpp"
#include "core/utf.hpp"
#include "ui/application.hpp"
#include "ui/draw.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"

namespace playtube::app {

namespace {

constexpr float kUnderline = 3;

}  // namespace

NavTab::NavTab(std::string text, bool current, std::function<void()> onPress)
    : ui::Box(ui::Axis::Row), current_(current) {
    focusable = true;
    drawsOwnFocus = true;
    padding = {ui::theme().spacingS, ui::theme().spacingL, ui::theme().spacingS, ui::theme().spacingL};
    cornerRadius = ui::theme().radius;
    crossAlign = ui::CrossAlign::Center;
    label_ = add<ui::Label>(std::move(text), ui::FontStyle{ui::theme().fontBody, ui::FontWeight::SemiBold});
    bind(platform::Action::Confirm, ui::tr("ui.select"), [onPress = std::move(onPress)] {
        if (onPress) {
            onPress();
        }
        return true;
    });
    refresh();
}

void NavTab::onFocusChanged(bool focused) {
    focused_ = focused;
    refresh();
}

void NavTab::onThemeChanged() {
    refresh();
    ui::Box::onThemeChanged();
}

void NavTab::refresh() {
    background = focused_ ? ui::theme().controlFocused : ui::Color{0, 0, 0, 0};
    label_->color = focused_ ? ui::theme().onAccent : current_ ? ui::theme().text : ui::theme().textSecondary;
    label_->markDirty();
}

void NavTab::draw(NVGcontext* vg) const {
    ui::Box::draw(vg);
    if (badge && badge()) {
        nvgBeginPath(vg);
        nvgCircle(vg, frame.width - 8, 8, 6);
        nvgFillColor(vg, ui::nvgColor(focused_ ? ui::theme().onAccent : ui::theme().accent));
        nvgFill(vg);
    }
    if (current_ && !focused_) {
        const float inset = ui::theme().spacingL;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, inset, frame.height - kUnderline, frame.width - 2 * inset, kUnderline, kUnderline / 2);
        nvgFillColor(vg, ui::nvgColor(ui::theme().accent));
        nvgFill(vg);
    }
}

NavBar::NavBar(AppSession& session, Tab current) : ui::Box(ui::Axis::Row) {
    width = ui::SizeSpec::fill();
    crossAlign = ui::CrossAlign::Center;
    gap = ui::theme().spacingS;
    addChild(makeLogo());
    auto* spacer = add<ui::View>();
    spacer->width = ui::SizeSpec::fixed(ui::theme().spacingXl);
    AppSession* shared = &session;
    const std::pair<Tab, const char*> tabs[] = {{Tab::Home, "shell.tab.home"},
                                                {Tab::Search, "shell.tab.search"},
                                                {Tab::Subscriptions, "shell.tab.subscriptions"},
                                                {Tab::Library, "shell.tab.library"}};
    for (const auto& tab : tabs) {
        const Tab which = tab.first;
        std::function<void()> press;
        if (which != current) {  // the current tab is already shown
            press = [shared, which] { openTab(*shared, which); };
        }
        auto* view = add<NavTab>(ui::tr(tab.second), which == current, std::move(press));
        if (which == current) {
            current_ = view;
        }
    }
    rememberFocus = true;  // the focus search enters the bar as a whole: focusEntry() gives the current tab
    // on the right: the account's initial, or "Se connecter"
    auto* rest = add<ui::View>();
    rest->width = ui::SizeSpec::fill();
    auto* clock = add<ClockLabel>(session);
    clock->margin = {0, ui::theme().spacingL, 0, 0};
    std::function<void()> settings;
    if (current != Tab::Settings) {
        settings = [shared] { openTab(*shared, Tab::Settings); };
    }
    auto* gear = add<SettingsGear>(current == Tab::Settings, std::move(settings));
    gear->badge = [shared] { return shared->update.has_value(); };
    gear->margin = {0, ui::theme().spacingS, 0, 0};
    if (current == Tab::Settings) {
        current_ = gear;
    }
    if (session.account().signedIn()) {
        addChild(makeAvatar(core::withoutEmoji(session.account().name())));
    } else {
        add<NavTab>(ui::tr("shell.signin"), false, [shared, current] {
            openSignIn(*shared, [shared, current] { openTab(*shared, current); });
        });
    }
}

SettingsGear::SettingsGear(bool current, std::function<void()> onPress) : current_(current) {
    name = "settings";
    focusable = true;
    drawsOwnFocus = true;
    width = ui::SizeSpec::fixed(52);
    height = ui::SizeSpec::fixed(52);
    bind(platform::Action::Confirm, ui::tr("shell.tab.settings"), [onPress = std::move(onPress)] {
        if (onPress) {
            onPress();
        }
        return true;
    });
}

void SettingsGear::draw(NVGcontext* vg) const {
    const ui::Theme& t = ui::theme();
    const float cx = frame.width / 2;
    const float cy = frame.height / 2;
    if (focused_) {
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, frame.width / 2);
        nvgFillColor(vg, ui::nvgColor(t.controlFocused));
        nvgFill(vg);
    }
    const ui::Color ink = focused_ ? t.onAccent : current_ ? t.text : t.textSecondary;
    // eight teeth around a ring; the hole in the colour behind it
    constexpr int kTeeth = 8;
    constexpr float kPi = 3.14159265f;
    const float outer = 15;
    const float root = 11;
    nvgBeginPath(vg);
    for (int i = 0; i < kTeeth * 4; ++i) {
        const float a = (static_cast<float>(i) - 0.5f) * 2 * kPi / (kTeeth * 4);
        const float r = (i % 4 == 1 || i % 4 == 2) ? outer : root;
        const float x = cx + r * static_cast<float>(std::cos(static_cast<double>(a)));
        const float y = cy + r * static_cast<float>(std::sin(static_cast<double>(a)));
        if (i == 0) {
            nvgMoveTo(vg, x, y);
        } else {
            nvgLineTo(vg, x, y);
        }
    }
    nvgClosePath(vg);
    nvgFillColor(vg, ui::nvgColor(ink));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, 5);
    nvgFillColor(vg, ui::nvgColor(focused_ ? t.controlFocused : t.background));
    nvgFill(vg);
    if (badge && badge()) {
        nvgBeginPath(vg);
        nvgCircle(vg, frame.width - 7, 7, 6);
        nvgFillColor(vg, ui::nvgColor(focused_ ? t.onAccent : t.accent));
        nvgFill(vg);
    }
    if (current_ && !focused_) {  // as the current tab: a line of the accent under it
        nvgBeginPath(vg);
        nvgRoundedRect(vg, cx - 12, frame.height - 3, 24, 3, 1.5f);
        nvgFillColor(vg, ui::nvgColor(t.accent));
        nvgFill(vg);
    }
}

void openTab(AppSession& session, Tab tab) {
    if (ui::Application* app = ui::Application::current()) {
        app->replace(makeTab(session, tab));
    }
}

namespace {
std::vector<ClockLabel*>& clocks() {
    static std::vector<ClockLabel*> all;
    return all;
}
std::int64_t gMinute = -1;
}  // namespace

ClockLabel::ClockLabel(const AppSession& session)
    : ui::Label("", ui::FontStyle{ui::theme().fontBody, ui::FontWeight::SemiBold}), session_(session) {
    clocks().push_back(this);
    refresh();
}

ClockLabel::~ClockLabel() {
    auto& all = clocks();
    all.erase(std::remove(all.begin(), all.end(), this), all.end());
}

void ClockLabel::refresh() {
    const std::int64_t now = session_.clockNow();
    const std::string text = headerClock(now, session_.utcOffsetMinutes(now));
    if (text != this->text()) {
        setText(text);
    }
}

void ClockLabel::tick(const AppSession& session) {
    const std::int64_t minute = session.clockNow() / 60;
    if (minute == gMinute) {
        return;
    }
    gMinute = minute;
    for (ClockLabel* clock : clocks()) {
        clock->refresh();
    }
}

}  // namespace playtube::app
