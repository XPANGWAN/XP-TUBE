#include "app/shell/screensaver.hpp"

#include <cstdint>
#include <string>

#include "app/shell/clock.hpp"
#include "ui/application.hpp"
#include "ui/theme.hpp"

namespace playtube::app {

namespace {

Screensaver* gShown = nullptr;

// Where the clock goes in turn, as the padding around it (left, top): spread over the screen, never twice the same
// place in a row.
constexpr float kPlaces[][2] = {{760, 400}, {200, 140}, {1180, 640}, {320, 700}, {1220, 180}, {640, 300},
                                {140, 460}, {980, 460}};
constexpr unsigned kPlaceCount = sizeof(kPlaces) / sizeof(kPlaces[0]);

}  // namespace

Screensaver::Screensaver(AppSession& session) : session_(session) {
    name = "screensaver";
    background = ui::Color{0, 0, 0, 255};
    focusable = true;
    drawsOwnFocus = true;
    hidesHints = true;
    column_ = add<ui::Box>(ui::Axis::Column);
    column_->gap = ui::theme().spacingS;
    time_ = column_->add<ui::Label>("", ui::FontStyle{ui::theme().fontDisplay * 1.6f, ui::FontWeight::SemiBold});
    time_->color = ui::Color{0xC8, 0xC8, 0xCE, 255};  // grey, not white: lit for a long time
    date_ = column_->add<ui::Label>("", ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::Regular});
    date_->color = ui::Color{0x8A, 0x8A, 0x92, 255};
    // any button leaves it (and does nothing else)
    for (int i = 0; i < platform::kActionCount; ++i) {
        bind(static_cast<platform::Action>(i), "", [] {
            if (ui::Application* app = ui::Application::current()) {
                app->closeModal();
            }
            return true;
        });
    }
    gShown = this;
    move();
    showClock();
}

Screensaver::~Screensaver() {
    if (gShown == this) {
        gShown = nullptr;
    }
}

bool Screensaver::shown() { return gShown != nullptr; }

void Screensaver::tick(float dtMs) {
    if (gShown != nullptr) {
        gShown->advance(dtMs);
    }
}

void Screensaver::advance(float dtMs) {
    sinceMs_ += dtMs;
    if (sinceMs_ >= kMoveMs) {
        sinceMs_ = 0;
        place_ = (place_ + 1) % kPlaceCount;
        move();
    }
    showClock();
}

void Screensaver::move() {
    padding = {kPlaces[place_][1], 0, 0, kPlaces[place_][0]};
    markDirty();
}

void Screensaver::showClock() {
    const std::int64_t now = session_.clockNow();
    const int offset = session_.utcOffsetMinutes(now);
    const std::string time = clock24(now, offset);
    if (time != time_->text()) {
        time_->setText(time);
        date_->setText(clockDate(now, offset));
    }
}

std::unique_ptr<ui::View> makeScreensaver(AppSession& session) { return std::make_unique<Screensaver>(session); }

}  // namespace playtube::app
