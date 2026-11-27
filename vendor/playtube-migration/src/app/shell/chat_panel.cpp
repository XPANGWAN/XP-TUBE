#include "app/shell/chat_panel.hpp"

#include <algorithm>
#include <utility>

#include "app/shell/chrome.hpp"
#include "core/log.hpp"
#include "core/utf.hpp"
#include "ui/application.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"

namespace playtube::app {

namespace {

constexpr float kPanelWidth = 640;
constexpr float kHintsRoom = 120;  // the button hints at the bottom right stay clear of the last message
constexpr int kMaxLines = 4;

}  // namespace

ChatPanel::ChatPanel(std::string continuation, Fetch fetch)
    : continuation_(std::move(continuation)), fetch_(std::move(fetch)) {
    name = "live chat";
    horizontal = ui::CrossAlign::End;
    vertical = ui::CrossAlign::Start;
    card_ = ui::Stack::add<ui::Box>(ui::Axis::Column);
    card_->width = ui::SizeSpec::fixed(kPanelWidth);
    card_->height = ui::SizeSpec::fill();
    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left
    card_->padding = ui::Insets::all(ui::theme().spacingXl);
    card_->padding.bottom = kHintsRoom;
    card_->gap = ui::theme().spacingL;
    card_->focusable = true;  // so that ◯ closes it (nothing in it takes the focus)
    card_->drawsOwnFocus = true;
    auto* heading = card_->add<ui::Label>(ui::tr("player.live_chat"), headingText());
    heading->width = ui::SizeSpec::fill();
    status_ = card_->add<ui::Label>(ui::tr("ui.loading"), bodyText());
    status_->color = ui::theme().textSecondary;
    status_->width = ui::SizeSpec::fill();
    // the newest at the bottom: older ones leave at the top, clipped
    list_ = card_->add<ui::Box>(ui::Axis::Column);
    list_->width = ui::SizeSpec::fill();
    list_->height = ui::SizeSpec::fill();
    list_->align = ui::Align::End;
    list_->gap = ui::theme().spacingL;
    list_->clip = true;
    bind(platform::Action::Back, ui::tr("ui.back"), [] {
        if (ui::Application* app = ui::Application::current()) {
            app->closeModal();
        }
        return true;
    });
    if (continuation_.empty()) {
        status_->setText(ui::tr("player.live_chat_off"));
    } else {
        load();
    }
}

ChatPanel::~ChatPanel() {
    *alive_ = false;
    token_.cancel();
}

void ChatPanel::load() {
    std::weak_ptr<bool> alive = alive_;
    fetch_(continuation_, [this, alive](core::Result<youtube::ChatPage> page) {
        const auto flag = alive.lock();
        if (flag && *flag) {
            show(std::move(page));
        }
    });
}

void ChatPanel::show(core::Result<youtube::ChatPage> page) {
    if (!page.ok()) {
        PF_LOG_WARN("live chat: %s", page.error().message.c_str());
        if (shown_.empty()) {
            status_->setText(ui::tr("player.live_chat_failed"));
            status_->visible = true;
        }
        later(kRetryMs);
        return;
    }
    for (const std::string& id : page.value().removed) {
        remove(id);
    }
    for (const youtube::ChatMessage& message : page.value().messages) {
        add(message);
    }
    status_->visible = shown_.empty();
    if (shown_.empty()) {
        status_->setText(ui::tr("player.live_chat_quiet"));
    }
    markDirty();
    if (page.value().continuation.empty()) {
        return;  // the stream ended
    }
    continuation_ = page.value().continuation;
    later(std::clamp(page.value().waitMs, 1000, kPollMs));
}

void ChatPanel::later(int delayMs) {
    std::weak_ptr<bool> alive = alive_;
    if (ui::Application* app = ui::Application::current()) {
        app->after(static_cast<float>(delayMs), [this, alive] {
            const auto flag = alive.lock();
            if (flag && *flag) {
                load();
            }
        }, token_);
    }
}

void ChatPanel::add(const youtube::ChatMessage& message) {
    const std::string text = core::withoutEmoji(message.text);
    if ((text.find_first_not_of(" \t\n") == std::string::npos && message.amount.empty()) ||
        std::any_of(shown_.begin(), shown_.end(), [&](const auto& entry) { return entry.first == message.id; })) {
        return;  // only emoji, or already here
    }
    const float wide = kPanelWidth - 2 * ui::theme().spacingXl;
    auto* entry = list_->add<ui::Box>(ui::Axis::Column);
    entry->width = ui::SizeSpec::fixed(wide);
    entry->gap = ui::theme().spacingS / 2;
    std::string heading = core::withoutEmoji(message.author);
    if (!message.amount.empty()) {
        heading += " \xC2\xB7 " + message.amount;
    }
    auto* author = entry->add<ui::Label>(heading, ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::SemiBold});
    author->width = ui::SizeSpec::fixed(wide);
    author->color = message.amount.empty() ? ui::theme().textSecondary : ui::theme().accent;
    auto* body = entry->add<ui::Label>(text, bodyText());
    body->width = ui::SizeSpec::fixed(wide);
    body->maxLines = kMaxLines;
    body->visible = !text.empty();
    shown_.emplace_back(message.id, entry);
    while (static_cast<int>(shown_.size()) > kMaxShown) {
        list_->takeChild(shown_.front().second);
        shown_.pop_front();
    }
}

void ChatPanel::remove(const std::string& id) {
    const auto it = std::find_if(shown_.begin(), shown_.end(), [&](const auto& entry) { return entry.first == id; });
    if (it != shown_.end()) {
        list_->takeChild(it->second);
        shown_.erase(it);
    }
}

}  // namespace playtube::app
