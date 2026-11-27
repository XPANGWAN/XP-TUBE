#pragma once

#include <functional>
#include <string>

#include "app/shell/session.hpp"
#include "ui/box.hpp"
#include "ui/label.hpp"

namespace playtube::app {

enum class Tab { Home, Search, Subscriptions, Library, Settings };

// A tab of the navigation bar: focused, it fills with the accent; the current tab is underlined with it.
class NavTab : public ui::Box {
public:
    NavTab(std::string text, bool current, std::function<void()> onPress);
    // a dot of the accent at the top right while it answers true (Paramètres: an update is available)
    std::function<bool()> badge;
    void onFocusChanged(bool focused) override;
    void onThemeChanged() override;
    void draw(NVGcontext* vg) const override;

private:
    void refresh();
    bool current_;
    bool focused_ = false;
    ui::Label* label_;
};

// Paramètres, as a gear beside the account (top right): filled with the accent when focused, underlined when it is
// the screen shown, a dot while an update is published.
class SettingsGear : public ui::View {
public:
    SettingsGear(bool current, std::function<void()> onPress);
    std::function<bool()> badge;
    void onFocusChanged(bool focused) override {
        focused_ = focused;
        markDirty();
    }
    void draw(NVGcontext* vg) const override;

private:
    bool current_;
    bool focused_ = false;
};

// The top line of the tab screens: the logo, then the four tabs, the clock, the settings' gear and the account. ✕ on a tab replaces the screen stack with that
// tab's screen.
class NavBar : public ui::Box {
public:
    NavBar(AppSession& session, Tab current);
    ui::View* currentTab() const { return current_; }
    // coming into the bar (Up from the content), the focus lands on the current tab
    ui::View* focusEntry() override { return current_ != nullptr ? current_ : ui::Box::focusEntry(); }

private:
    ui::View* current_ = nullptr;
};

// The header's date and time ("Vendredi 16 · 21:45"); every one shown is set again when the minute changes.
class ClockLabel : public ui::Label {
public:
    explicit ClockLabel(const AppSession& session);
    ~ClockLabel() override;
    void refresh();
    // once per frame from the app's loop: when the minute changed, every clock shown takes the new time
    static void tick(const AppSession& session);

private:
    const AppSession& session_;
};

// The tab's screen replaces the whole stack.
void openTab(AppSession& session, Tab tab);

}  // namespace playtube::app
