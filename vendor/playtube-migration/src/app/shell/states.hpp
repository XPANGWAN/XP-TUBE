#pragma once

#include <functional>
#include <string>

#include "app/shell/screen.hpp"
#include "core/result.hpp"
#include "ui/box.hpp"

namespace playtube::app {

// The message area of a screen: "Chargement…", a text, or a failure with "Réessayer". Each replaces what the area
// showed, moving a focus inside it to the screen's start first.
void showLoading(ShellScreen& screen, ui::Box& messages);
void showText(ShellScreen& screen, ui::Box& messages, const std::string& text);
// The button takes the focus when nothing else of the screen has it (the focus still at the start).
void showFailure(ShellScreen& screen, ui::Box& messages, const core::Error& error, std::function<void()> retry);
void clearMessages(ShellScreen& screen, ui::Box& messages);

}  // namespace playtube::app
