#include "app/shell/player_settings.hpp"

#include <utility>

#include "ui/application.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"

namespace playtube::app {

namespace {

// The approved canvas at 1280x720, times 1.5
constexpr float kWidth = 600;
constexpr float kMargin = 72;      // top, right and bottom
constexpr float kPaddingV = 42;
constexpr float kPaddingH = 30;
constexpr float kRadius = 24;
constexpr float kGap = 12;         // between the lines
constexpr float kTitleSize = 33;   // semibold
constexpr float kTitleInset = 18;  // the title's sides and the room under it
constexpr float kLineHeight = 84;
constexpr float kLinePadding = 24;
constexpr float kLineRadius = 18;
constexpr float kTextSize = 27;
const ui::Color kDim{0x07, 0x07, 0x08, 0x8C};      // 55 % over the picture
const ui::Color kFocused{0xFF, 0xFF, 0xFF, 0xFF};  // white, as the focused icon
const ui::Color kOnFocused{0x0A, 0x0A, 0x0C, 0xFF};

}  // namespace

PlayerSettingLine::PlayerSettingLine(PlayerSettingRow row) : ui::Box(ui::Axis::Row) {
    focusable = true;
    drawsOwnFocus = true;
    width = ui::SizeSpec::fill();
    height = ui::SizeSpec::fixed(kLineHeight);
    padding = {0, kLinePadding, 0, kLinePadding};
    cornerRadius = kLineRadius;
    crossAlign = ui::CrossAlign::Center;
    gap = kLinePadding;
    name_ = add<ui::Label>(std::move(row.name), ui::FontStyle{kTextSize, ui::FontWeight::Regular});
    name_->width = ui::SizeSpec::fill();  // what the value leaves: a long name is cut, the value stays
    value_ = add<ui::Label>(std::move(row.value), ui::FontStyle{kTextSize, ui::FontWeight::Regular});
    refresh();
    if (row.open) {
        bind(platform::Action::Confirm, ui::tr("ui.select"), [open = std::move(row.open)] {
            open();
            return true;
        });
    }
}

void PlayerSettingLine::setValue(std::string value) {
    if (value != value_->text()) {
        value_->setText(std::move(value));
    }
}

void PlayerSettingLine::onFocusChanged(bool focused) {
    focused_ = focused;
    refresh();
}

void PlayerSettingLine::onThemeChanged() {
    refresh();
    ui::Box::onThemeChanged();
}

void PlayerSettingLine::refresh() {
    background = focused_ ? kFocused : ui::Color{0, 0, 0, 0};
    const ui::FontWeight weight = focused_ ? ui::FontWeight::SemiBold : ui::FontWeight::Regular;
    name_->color = focused_ ? kOnFocused : ui::theme().text;
    value_->color = focused_ ? kOnFocused : ui::theme().textSecondary;
    name_->style.weight = weight;
    value_->style.weight = weight;
    name_->markDirty();
    value_->markDirty();
}

PlayerSettings::PlayerSettings(std::vector<PlayerSettingRow> rows) {
    name = "player settings";
    background = kDim;
    horizontal = ui::CrossAlign::End;
    vertical = ui::CrossAlign::Start;
    rememberFocus = true;    // back on the line whose chooser closed
    navigationHint = false;  // ✕ and ◯ only, as on the canvas
    auto* card = add<ui::Box>(ui::Axis::Column);
    card->width = ui::SizeSpec::fixed(kWidth);
    card->height = ui::SizeSpec::fill();
    card->margin = {kMargin, kMargin, kMargin, 0};
    card->padding = {kPaddingV, kPaddingH, kPaddingV, kPaddingH};
    card->gap = kGap;
    card->cornerRadius = kRadius;
    card->background = ui::theme().surfaceRaised;
    auto* title = card->add<ui::Label>(ui::tr("player.settings"), ui::FontStyle{kTitleSize, ui::FontWeight::SemiBold});
    title->width = ui::SizeSpec::fill();
    title->margin = {0, kTitleInset, kTitleInset, kTitleInset};
    for (PlayerSettingRow& row : rows) {
        lines_.push_back(card->add<PlayerSettingLine>(std::move(row)));
    }
    bind(platform::Action::Back, ui::tr("ui.back"), [] {
        if (ui::Application* app = ui::Application::current()) {
            app->closeModal();
        }
        return true;
    });
}

PlayerSettings::~PlayerSettings() {
    if (slot_) {
        *slot_ = nullptr;
    }
}

void PlayerSettings::trackIn(std::shared_ptr<PlayerSettings*> slot) {
    slot_ = std::move(slot);
    if (slot_) {
        *slot_ = this;
    }
}

void PlayerSettings::setValue(const std::string& title, std::string value) {
    for (PlayerSettingLine* line : lines_) {
        if (line->title() == title) {
            line->setValue(std::move(value));
            return;
        }
    }
}

std::unique_ptr<PlayerSettings> makePlayerSettings(std::vector<PlayerSettingRow> rows) {
    return std::make_unique<PlayerSettings>(std::move(rows));
}

}  // namespace playtube::app
