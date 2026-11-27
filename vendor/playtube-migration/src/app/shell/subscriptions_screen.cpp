#include <memory>
#include <string>

#include "app/shell/cards.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/nav_bar.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/states.hpp"
#include "app/shell/video_pages.hpp"
#include "core/log.hpp"
#include "ui/i18n.hpp"
#include "ui/reveal.hpp"

namespace playtube::app {

namespace {

// Abonnements: the latest videos of the subscribed channels, with more pages; an invitation when signed out.
class SubscriptionsScreen : public ShellScreen {
public:
    explicit SubscriptionsScreen(AppSession& session) : ShellScreen(session) {
        column_ = add<ui::Box>(ui::Axis::Column);
        column_->width = ui::SizeSpec::fill();
        column_->height = ui::SizeSpec::fill();
        column_->padding = pagePadding();
        column_->padding.bottom = 0;
        column_->gap = ui::theme().spacingL;
        auto* bar = column_->add<NavBar>(session_, Tab::Subscriptions);
        setStart(bar->currentTab());
        messages_ = column_->add<ui::Box>(ui::Axis::Column);
        if (!session_.account().signedIn()) {
            addSignInInvitation(*this, *messages_, ui::tr("subscriptions.signed_out"), Tab::Subscriptions);
            return;
        }
        grid_ = addVideoGrid(*column_);
        pages_ = std::make_unique<VideoPages>(*this, *grid_, signedInFeedMore(session_), playAction(session_),
                                              channelAction(*this));
        pages_->hideShorts = !session_.settings().shorts;
        load();
    }

private:
    void load() {
        showLoading(*this, *messages_);
        const First first = signedInFeed(session_, "FEsubscriptions");
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::VideoPage>>([first, cancel] { return first(cancel); },
                                              [this](core::Result<youtube::VideoPage> page) { show(std::move(page)); });
    }

    void show(core::Result<youtube::VideoPage> page) {
        if (!page.ok()) {
            PF_LOG_WARN("subscriptions: %s", page.error().message.c_str());
            if (page.error().kind == core::ErrorKind::Unauthorized && !session_.account().signedIn()) {
                AppSession* session = &session_;
                later([session] { openTab(*session, Tab::Subscriptions); });  // revoked: the tab, signed out
                return;
            }
            showFailure(*this, *messages_, page.error(), [this] { load(); });
            return;
        }
        if (page.value().items.empty()) {
            showText(*this, *messages_, ui::tr("subscriptions.empty"));
            return;
        }
        clearMessages(*this, *messages_);
        pages_->show(std::move(page.value()));
        ui::revealChildren(*column_, 2);
        focusContent(grid_);
    }

    ui::Box* column_;
    ui::Box* messages_;
    ui::Grid* grid_ = nullptr;
    std::unique_ptr<VideoPages> pages_;
};

}  // namespace

std::unique_ptr<ui::Screen> makeSubscriptions(AppSession& session) {
    return std::make_unique<SubscriptionsScreen>(session);
}

}  // namespace playtube::app
