#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/box.hpp"
#include "ui/label.hpp"

namespace playtube::app {

// One line of the player's Paramètres panel: what it sets, its current value, and what ✕ opens (its chooser).
struct PlayerSettingRow {
    std::string name;
    std::string value;
    std::function<void()> open;
};

// A line of the panel: the name at the left, the value at the right in the secondary colour; focused, white with
// dark text.
class PlayerSettingLine : public ui::Box {
public:
    explicit PlayerSettingLine(PlayerSettingRow row);
    const std::string& title() const { return name_->text(); }
    const std::string& value() const { return value_->text(); }
    void setValue(std::string value);
    void onFocusChanged(bool focused) override;
    void onThemeChanged() override;

private:
    void refresh();
    bool focused_ = false;
    ui::Label* name_;
    ui::Label* value_;
};

// The player's Paramètres: a sheet at the right over the dimmed picture (shown as a modal), one line per row, the
// focus on the first. ✕ on a line runs its `open`; the panel comes back on that line when its chooser closes. ◯
// closes the panel.
class PlayerSettings : public ui::Stack {
public:
    explicit PlayerSettings(std::vector<PlayerSettingRow> rows);
    ~PlayerSettings() override;
    // `slot` points at this panel until it is destroyed, then holds nullptr (safe in either destruction order)
    void trackIn(std::shared_ptr<PlayerSettings*> slot);
    // the line named `title` shows `value` (once its chooser changed it)
    void setValue(const std::string& title, std::string value);
    const std::vector<PlayerSettingLine*>& lines() const { return lines_; }

private:
    std::vector<PlayerSettingLine*> lines_;
    std::shared_ptr<PlayerSettings*> slot_;
};

std::unique_ptr<PlayerSettings> makePlayerSettings(std::vector<PlayerSettingRow> rows);

}  // namespace playtube::app
