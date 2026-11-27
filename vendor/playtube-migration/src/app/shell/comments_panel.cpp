#include "app/shell/comments_panel.hpp"

#include <utility>

#include "app/shell/chrome.hpp"
#include "core/log.hpp"
#include "core/utf.hpp"
#include "ui/application.hpp"
#include "ui/i18n.hpp"
#include "ui/image_view.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

constexpr float kPanelWidth = 760;
constexpr float kAvatar = 56;
constexpr float kHintsRoom = 120;  // the button hints at the bottom right stay clear of the last comment
constexpr int kMaxLines = 8;
constexpr int kNearEnd = 3;
constexpr int kMaxComments = 300;

std::string repliesText(const std::string& count) {
    return count == "1" ? ui::tr("comments.reply_one") : ui::tr("comments.replies", {count});
}

}  // namespace

// The width of a comment's texts: the panel's, less its padding, the comment's padding, the picture and the gap
// (fixed: a wrapped text measured at another width would overflow its box), less the indent of a reply.
float textWidth(bool reply) {
    const ui::Theme& t = ui::theme();
    return kPanelWidth - 2 * t.spacingXl - 2 * t.spacingM - kAvatar - t.spacingM - (reply ? kAvatar + t.spacingM : 0);
}

// One comment: its author's picture, name and date, its text, its likes and replies. Its replies go in `replies`,
// a list placed right under it.
class CommentView : public ui::Box {
public:
    CommentView(const youtube::Comment& comment, bool reply) : ui::Box(ui::Axis::Row) {
        focusable = true;
        drawsOwnFocus = true;
        width = ui::SizeSpec::fill();
        gap = ui::theme().spacingM;
        padding = ui::Insets::all(ui::theme().spacingM);
        cornerRadius = ui::theme().radius;
        if (reply) {
            margin.left = kAvatar + ui::theme().spacingM;
        }
        const float wide = textWidth(reply);
        auto* avatar = add<ui::ImageView>();
        avatar->width = ui::SizeSpec::fixed(kAvatar);
        avatar->height = ui::SizeSpec::fixed(kAvatar);
        avatar->cornerRadius = kAvatar / 2;
        avatar->clip = true;
        avatar->background = ui::theme().surface;
        avatar->setUrl(comment.avatar);
        auto* texts = add<ui::Box>(ui::Axis::Column);
        texts->width = ui::SizeSpec::fixed(wide);
        texts->gap = ui::theme().spacingS;
        std::string heading = core::withoutEmoji(comment.author);
        if (!comment.publishedText.empty()) {
            heading += " \xC2\xB7 " + comment.publishedText;
        }
        auto* author = texts->add<ui::Label>(heading, ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::SemiBold});
        author->width = ui::SizeSpec::fixed(wide);
        author->color = ui::theme().textSecondary;
        auto* text = texts->add<ui::Label>(core::withoutEmoji(comment.text), bodyText());
        text->width = ui::SizeSpec::fixed(wide);
        text->maxLines = kMaxLines;
        std::string footer;
        if (!comment.likesText.empty()) {
            footer = ui::tr("comments.likes", {comment.likesText});
        }
        if (!comment.repliesContinuation.empty() && !comment.repliesText.empty()) {
            footer += (footer.empty() ? "" : " \xC2\xB7 ") + repliesText(comment.repliesText);
        }
        auto* details = texts->add<ui::Label>(footer, ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Regular});
        details->color = ui::theme().textSecondary;
        details->visible = !footer.empty();
        repliesContinuation = comment.repliesContinuation;
        if (!repliesContinuation.empty()) {
            bind(platform::Action::Confirm, ui::tr("comments.show_replies"), [this] {
                if (onPress) {
                    onPress();
                }
                return true;
            });
        }
    }

    void onFocusChanged(bool focused) override {
        background = focused ? ui::withAlpha(ui::theme().accent, 0x40) : ui::Color{0, 0, 0, 0};
        markDirty();
        if (focused && onFocused) {
            onFocused();
        }
    }

    ui::Box* replies = nullptr;  // set by the panel
    std::string repliesContinuation;
    bool repliesLoaded = false;
    bool repliesLoading = false;
    std::function<void()> onPress;
    std::function<void()> onFocused;
};

CommentsPanel::CommentsPanel(std::string continuation, Fetch fetch)
    : continuation_(std::move(continuation)), fetch_(std::move(fetch)) {
    name = "comments";
    horizontal = ui::CrossAlign::End;
    vertical = ui::CrossAlign::Start;
    card_ = add<ui::Box>(ui::Axis::Column);
    card_->width = ui::SizeSpec::fixed(kPanelWidth);
    card_->height = ui::SizeSpec::fill();
    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left
    card_->padding = ui::Insets::all(ui::theme().spacingXl);
    card_->padding.bottom = kHintsRoom;
    card_->gap = ui::theme().spacingL;
    // focused while there is no comment yet, so ◯ still closes the panel
    card_->focusable = true;
    card_->drawsOwnFocus = true;
    auto* heading = card_->add<ui::Label>(ui::tr("player.comments"), headingText());
    heading->width = ui::SizeSpec::fill();
    status_ = card_->add<ui::Label>("", bodyText());
    status_->color = ui::theme().textSecondary;
    status_->width = ui::SizeSpec::fill();
    scroll_ = card_->add<ui::VScroll>();
    scroll_->policy = ui::VScroll::Policy::Nearest;
    scroll_->nearestMargin = ui::theme().spacingL;
    scroll_->width = ui::SizeSpec::fill();
    scroll_->height = ui::SizeSpec::fill();
    scroll_->clip = true;
    auto list = std::make_unique<ui::Box>(ui::Axis::Column);
    list->width = ui::SizeSpec::fill();
    list->gap = ui::theme().spacingS;
    list_ = list.get();
    scroll_->setContent(std::move(list));
    bind(platform::Action::Back, ui::tr("ui.back"), [] {
        if (ui::Application* app = ui::Application::current()) {
            app->closeModal();
        }
        return true;
    });
    if (continuation_.empty()) {
        status_->setText(ui::tr("comments.empty"));
    } else {
        load();
    }
}

CommentsPanel::~CommentsPanel() { *alive_ = false; }

CommentsPanel::Done CommentsPanel::guarded(std::function<void(core::Result<youtube::CommentPage>)> then) {
    std::weak_ptr<bool> alive = alive_;
    return [alive, then = std::move(then)](core::Result<youtube::CommentPage> page) {
        const auto flag = alive.lock();
        if (flag && *flag) {
            then(std::move(page));
        }
    };
}

void CommentsPanel::load() {
    if (loading_ || continuation_.empty() || count() >= kMaxComments) {
        return;
    }
    loading_ = true;
    if (comments_.empty()) {
        status_->setText(ui::tr("ui.loading"));
    }
    fetch_(continuation_, guarded([this](core::Result<youtube::CommentPage> page) { show(std::move(page)); }));
}

void CommentsPanel::show(core::Result<youtube::CommentPage> page) {
    loading_ = false;
    if (!page.ok()) {
        PF_LOG_WARN("comments: %s", page.error().message.c_str());
        if (comments_.empty()) {
            status_->setText(errorText(page.error()));
        }
        return;  // a later move near the end asks again
    }
    continuation_ = std::move(page.value().continuation);
    for (const youtube::Comment& comment : page.value().items) {
        auto* view = list_->add<CommentView>(comment, false);
        view->replies = list_->add<ui::Box>(ui::Axis::Column);
        view->replies->width = ui::SizeSpec::fill();
        view->replies->gap = ui::theme().spacingS;
        view->replies->visible = false;
        const int index = count();
        view->onFocused = [this, index] {
            if (index >= count() - kNearEnd) {
                load();
            }
        };
        view->onPress = [this, view] { toggleReplies(*view); };
        comments_.push_back(view);
    }
    status_->setText(comments_.empty() ? ui::tr("comments.empty") : std::string());
    status_->visible = comments_.empty();
    markDirty();
    focusFirst();
}

void CommentsPanel::toggleReplies(CommentView& view) {
    if (view.repliesLoaded) {
        view.replies->visible = !view.replies->visible;
        markDirty();
        return;
    }
    if (view.repliesLoading) {
        return;
    }
    view.repliesLoading = true;
    CommentView* target = &view;
    fetch_(view.repliesContinuation, guarded([this, target](core::Result<youtube::CommentPage> page) {
               target->repliesLoading = false;
               if (!page.ok()) {
                   PF_LOG_WARN("replies: %s", page.error().message.c_str());
                   if (ui::Application* app = ui::Application::current()) {
                       app->toast(errorText(page.error()));
                   }
                   return;
               }
               target->repliesLoaded = true;
               for (const youtube::Comment& reply : page.value().items) {
                   target->replies->add<CommentView>(reply, true);
               }
               target->replies->visible = true;
               markDirty();
           }));
}

// once there are comments, the focus leaves the card for the first one
void CommentsPanel::focusFirst() {
    if (comments_.empty() || !card_->focusable) {
        return;
    }
    card_->focusable = false;
    if (ui::Application* app = ui::Application::current()) {
        app->focus().focus(comments_.front());
    }
}

}  // namespace playtube::app
