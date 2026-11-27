#include "app/shell/states.hpp"

#include <utility>

#include "app/shell/chrome.hpp"
#include "ui/i18n.hpp"

namespace playtube::app {

void clearMessages(ShellScreen& screen, ui::Box& messages) {
    screen.moveFocusOff(&messages);
    messages.clearChildren();
}

void showLoading(ShellScreen& screen, ui::Box& messages) { showText(screen, messages, ui::tr("shell.loading")); }

void showText(ShellScreen& screen, ui::Box& messages, const std::string& text) {
    clearMessages(screen, messages);
    addMessage(messages, text);
}

void showFailure(ShellScreen& screen, ui::Box& messages, const core::Error& error, std::function<void()> retry) {
    clearMessages(screen, messages);
    ShellScreen* owner = &screen;
    ui::Box* area = &messages;
    auto* box = addMessage(messages, errorText(error), ui::tr("ui.retry"), [owner, area, retry = std::move(retry)] {
        // not from inside the button's own handler: retrying empties the area that holds it
        owner->later([owner, area, retry] {
            clearMessages(*owner, *area);
            retry();
        });
    });
    screen.focusContent(box->children().back().get());
}

}  // namespace playtube::app
