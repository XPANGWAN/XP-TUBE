#include <memory>
#include <string>

#include "app/shell/cards.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/states.hpp"
#include "app/shell/video_pages.hpp"
#include "core/log.hpp"
#include "core/utf.hpp"
#include "ui/i18n.hpp"
#include "ui/image_view.hpp"
#include "ui/reveal.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

constexpr float kAvatar = 160;

class ChannelScreen : public ShellScreen {
public:
    ChannelScreen(AppSession& session, std::string channelId)
        : ShellScreen(session), channelId_(std::move(channelId)) {
        column_ = add<ui::Box>(ui::Axis::Column);
        column_->width = ui::SizeSpec::fill();
        column_->height = ui::SizeSpec::fill();
        column_->padding = pagePadding();
        column_->padding.bottom = 0;
        column_->gap = ui::theme().spacingL;
        header_ = column_->add<ui::Box>(ui::Axis::Row);
        header_->gap = ui::theme().spacingXl;
        header_->crossAlign = ui::CrossAlign::Center;
        header_->visible = false;
        avatar_ = header_->add<ui::ImageView>();
        avatar_->width = ui::SizeSpec::fixed(kAvatar);
        avatar_->height = ui::SizeSpec::fixed(kAvatar);
        avatar_->cornerRadius = kAvatar / 2;
        avatar_->clip = true;
        avatar_->background = ui::theme().surface;
        auto* texts = header_->add<ui::Box>(ui::Axis::Column);
        texts->gap = ui::theme().spacingS;
        name_ = texts->add<ui::Label>("", ui::FontStyle{56, ui::FontWeight::Bold});
        name_->width = ui::SizeSpec::fixed(1300);
        details_ = texts->add<ui::Label>("", bodyText());
        details_->width = ui::SizeSpec::fixed(1300);
        details_->color = ui::theme().textSecondary;
        description_ = texts->add<ui::Label>("", bodyText());
        description_->color = ui::theme().textSecondary;
        description_->maxLines = 2;
        description_->width = ui::SizeSpec::fixed(1300);
        buttons_ = column_->add<ui::Box>(ui::Axis::Row);
        buttons_->gap = ui::theme().spacingM;
        buttons_->visible = false;
        play_ = buttons_->add<ui::Button>(ui::tr("channel.play_latest"), [this] {
            if (!latest_.empty()) {
                openVideo(session_, latest_);
            }
        });
        play_->setPrimary(true);
        play_->visible = false;
        // the channel's tabs: its videos (shown first), Shorts and lives in the grid below; its playlists pushed
        buttons_->add<ui::Button>(ui::tr("channel.videos"), [this] { later([this] { showTab(youtube::ChannelTab::Videos); }); });
        if (session_.settings().shorts) {
            buttons_->add<ui::Button>(ui::tr("channel.shorts"),
                                      [this] { later([this] { showTab(youtube::ChannelTab::Shorts); }); });
        }
        buttons_->add<ui::Button>(ui::tr("channel.lives"), [this] { later([this] { showTab(youtube::ChannelTab::Lives); }); });
        buttons_->add<ui::Button>(ui::tr("channel.playlists"),
                                  [this] { openChannelPlaylists(session_, channelId_, channelName_); });
        // signed in: shown once YouTube said whether the account is subscribed
        subscribe_ = buttons_->add<ui::Button>(ui::tr("channel.subscribe"), [this] { toggleSubscription(); });
        subscribe_->visible = false;
        messages_ = column_->add<ui::Box>(ui::Axis::Column);
        grid_ = addVideoGrid(*column_);
        AppSession* shared = &session_;
        pages_ = std::make_unique<VideoPages>(
            *this, *grid_,
            [shared](const std::string& token, const core::CancelToken& cancel) {
                return shared->youtube().channelMore(token, cancel);
            },
            [shared](const youtube::Video& video) { openVideo(*shared, video.id); }, nullptr);  // already here
        pages_->hideShorts = !session_.settings().shorts;
        load();
    }

private:
    void load() {
        showLoading(*this, *messages_);
        AppSession* shared = &session_;
        const std::string id = channelId_;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::ChannelPage>>(
            [shared, id, cancel] { return shared->youtube().channel(id, cancel); },
            [this](core::Result<youtube::ChannelPage> page) { show(std::move(page)); });
    }

    void show(core::Result<youtube::ChannelPage> page) {
        if (!page.ok()) {
            PF_LOG_WARN("channel: %s", page.error().message.c_str());
            showFailure(*this, *messages_, page.error(), [this] { load(); });
            return;
        }
        clearMessages(*this, *messages_);
        const youtube::Channel& channel = page.value().channel;
        avatar_->setUrl(channel.avatar);
        channelName_ = channel.name;
        name_->setText(core::withoutEmoji(channel.name));
        std::string details = channel.handle;
        if (!channel.subscribersText.empty()) {
            details += (details.empty() ? "" : " \xC2\xB7 ") + channel.subscribersText;
        }
        details_->setText(details);
        description_->setText(core::withoutEmoji(channel.description));
        description_->visible = !channel.description.empty();
        header_->visible = true;
        buttons_->visible = true;
        if (!page.value().videos.items.empty()) {
            latest_ = page.value().videos.items.front().id;
            play_->visible = true;
            loadSubscription();
        } else {
            addMessage(*messages_, ui::tr("channel.empty"));
        }
        pages_->show(std::move(page.value().videos));
        ui::revealChildren(*column_, 0);
        focusIfEmpty(buttons_);
    }

    // another tab's videos in the grid (its next pages through channelMore, as the videos')
    void showTab(youtube::ChannelTab tab, bool again = false) {
        if (tab == tab_ && !again) {
            focusContent(grid_);
            return;
        }
        tab_ = tab;
        ++tabGeneration_;
        moveFocusOff(grid_);
        pages_->show({});
        clearMessages(*this, *messages_);
        showLoading(*this, *messages_);
        AppSession* shared = &session_;
        const std::string id = channelId_;
        const core::CancelToken cancel = token_;
        const unsigned generation = tabGeneration_;
        run<core::Result<youtube::VideoPage>>(
            [shared, id, tab, cancel] { return shared->youtube().channelTab(id, tab, cancel); },
            [this, generation, tab](core::Result<youtube::VideoPage> page) {
                if (generation != tabGeneration_) {
                    return;  // another tab was asked for since
                }
                if (!page.ok()) {
                    PF_LOG_WARN("channel tab: %s", page.error().message.c_str());
                    showFailure(*this, *messages_, page.error(), [this, tab] { showTab(tab, true); });
                    return;
                }
                clearMessages(*this, *messages_);
                if (page.value().items.empty()) {
                    addMessage(*messages_, ui::tr("channel.tab_empty"));
                }
                pages_->show(std::move(page.value()));
                focusContent(grid_);
            });
    }

    // the subscription is told by the account's view of the channel's latest video
    void loadSubscription() {
        if (!session_.account().signedIn()) {
            return;
        }
        AppSession* shared = &session_;
        const std::string video = latest_;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::VideoState>>(
            [shared, video, cancel] {
                return shared->signedIn<youtube::VideoState>(
                    [&](const std::string& token) { return shared->youtube().videoState(video, token, cancel); },
                    cancel);
            },
            [this](core::Result<youtube::VideoState> state) {
                if (!state.ok()) {
                    PF_LOG_WARN("channel subscription: %s", state.error().message.c_str());
                    return;
                }
                if (!state.value().channelId.empty() && state.value().channelId != channelId_) {
                    return;  // the latest video is another channel's (a collaboration)
                }
                subscribed_ = state.value().subscribed;
                showSubscription();
            });
    }

    void showSubscription() {
        subscribe_->setText(ui::tr(subscribed_ ? "channel.subscribed" : "channel.subscribe"));
        subscribe_->visible = true;
        markDirty();
    }

    void toggleSubscription() {
        if (changing_) {
            return;  // one change at a time: the button shows what YouTube confirmed
        }
        changing_ = true;
        AppSession* shared = &session_;
        const std::string id = channelId_;
        const bool wanted = !subscribed_;
        run<core::Result<core::Unit>>(
            [shared, id, wanted] {
                return shared->signedIn<core::Unit>(
                    [&](const std::string& token) { return shared->youtube().setSubscribed(id, wanted, token); });
            },
            [this, wanted](core::Result<core::Unit> done) {
                changing_ = false;
                if (!done.ok()) {
                    PF_LOG_WARN("subscription: %s", done.error().message.c_str());
                    if (ui::Application* app = ui::Application::current()) {
                        app->toast(errorText(done.error()));
                    }
                    return;
                }
                subscribed_ = wanted;
                showSubscription();
            });
    }

    std::string channelId_;
    youtube::ChannelTab tab_ = youtube::ChannelTab::Videos;
    unsigned tabGeneration_ = 0;
    std::string channelName_;
    std::string latest_;
    bool subscribed_ = false;
    bool changing_ = false;
    ui::Box* column_;
    ui::Box* header_;
    ui::ImageView* avatar_;
    ui::Label* name_;
    ui::Label* details_;
    ui::Label* description_;
    ui::Box* buttons_;
    ui::Button* play_;
    ui::Button* subscribe_;
    ui::Box* messages_;
    ui::Grid* grid_;
    std::unique_ptr<VideoPages> pages_;
};

}  // namespace

void openChannel(AppSession& session, const std::string& channelId) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && !channelId.empty()) {
        app->push(std::make_unique<ChannelScreen>(session, channelId));
    }
}

void openChannelOf(ShellScreen& screen, const youtube::Video& video) {
    if (!video.channelId.empty()) {
        openChannel(screen.session(), video.channelId);
        return;
    }
    // TV tiles and related videos do not carry the channel: the video page tells it
    AppSession* session = &screen.session();
    const std::string id = video.id;
    const core::CancelToken cancel = screen.cancelToken();
    screen.run<core::Result<youtube::NextReply>>(
        [session, id, cancel] { return session->youtube().next(id, cancel); },
        [session, owner = &screen](core::Result<youtube::NextReply> reply) {
            ui::Application* app = ui::Application::current();
            if (app == nullptr || app->top() != owner) {
                return;  // the user went elsewhere meanwhile, or a second □ already opened it
            }
            if (reply.ok()) {
                openChannel(*session, reply.value().owner.id);
            } else {
                PF_LOG_WARN("channel of a video: %s", reply.error().message.c_str());
            }
        });
}

}  // namespace playtube::app
