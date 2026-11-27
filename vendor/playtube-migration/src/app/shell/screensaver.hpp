#pragma once

#include <memory>

#include "app/shell/session.hpp"
#include "ui/box.hpp"
#include "ui/label.hpp"

namespace playtube::app {

// After a few idle minutes (never over a video): a black screen with the time, large, and the date under it. They
// move to another place every minute, so that nothing stays lit in one place on the TV. Any button closes it.
// Shown as a modal; it asks nothing of the network.
class Screensaver : public ui::Stack {
public:
    static constexpr float kIdleMs = 5 * 60 * 1000;
    static constexpr float kMoveMs = 60 * 1000;

    explicit Screensaver(AppSession& session);
    ~Screensaver() override;
    // from the app's loop: the clock, and its place every minute
    static void tick(float dtMs);
    static bool shown();

private:
    void advance(float dtMs);
    void showClock();
    void move();

    AppSession& session_;
    ui::Box* column_;
    ui::Label* time_;
    ui::Label* date_;
    float sinceMs_ = 0;
    unsigned place_ = 0;
};

std::unique_ptr<ui::View> makeScreensaver(AppSession& session);

}  // namespace playtube::app
