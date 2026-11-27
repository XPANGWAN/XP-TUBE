#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/shell/cards.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/states.hpp"
#include "app/shell/video_pages.hpp"
#include "core/log.hpp"
#include "core/utf.hpp"
#include "ui/i18n.hpp"
#include "ui/reveal.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

// A titled grid of cards with its next pages: a playlist's videos, a channel's playlists.
class GridScreen : public ShellScreen {
public:
    GridScreen(AppSession& session, const std::string& title, const std::string& details)
        : ShellScreen(session) {
        column_ = add<ui::Box>(ui::Axis::Column);
        column_->width = ui::SizeSpec::fill();
        column_->height = ui::SizeSpec::fill();
        column_->padding = pagePadding();
        column_->padding.bottom = 0;
        column_->gap = ui::theme().spacingL;
        auto* heading = column_->add<ui::Label>(core::withoutEmoji(title), ui::FontStyle{56, ui::FontWeight::Bold});
        heading->width = ui::SizeSpec::fill();
        heading->maxLines = 1;
        details_ = column_->add<ui::Label>(details, bodyText());
        details_->color = ui::theme().textSecondary;
        details_->visible = !details.empty();
        buttons_ = column_->add<ui::Box>(ui::Axis::Row);
        buttons_->gap = ui::theme().spacingM;
        buttons_->visible = false;
        messages_ = column_->add<ui::Box>(ui::Axis::Column);
        grid_ = addVideoGrid(*column_);
    }

protected:
    using Fetch = VideoPages::Fetch;

    void setPages(Fetch more, VideosSource::Action onPress) {
        pages_ = std::make_unique<VideoPages>(*this, *grid_, std::move(more), std::move(onPress), nullptr);
        pages_->hideShorts = !session_.settings().shorts;
    }

    // the first page: shown, else the reason and "Réessayer", or `emptyText`
    void load(First first, std::string emptyText) {
        showLoading(*this, *messages_);
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::VideoPage>>(
            [first, cancel] { return first(cancel); },
            [this, first, emptyText](core::Result<youtube::VideoPage> page) {
                if (!page.ok()) {
                    PF_LOG_WARN("grid: %s", page.error().message.c_str());
                    showFailure(*this, *messages_, page.error(), [this, first, emptyText] { load(first, emptyText); });
                    return;
                }
                clearMessages(*this, *messages_);
                const bool empty = page.value().items.empty();
                if (empty) {
                    addMessage(*messages_, emptyText);
                }
                pages_->show(std::move(page.value()));
                buttons_->visible = !empty && !buttons_->children().empty();
                ui::revealChildren(*column_, 0);
                focusIfEmpty(buttons_->visible ? static_cast<ui::View*>(buttons_) : grid_);
            });
    }

    ui::Box* column_;
    ui::Label* details_;
    ui::Box* buttons_;
    ui::Box* messages_;
    ui::Grid* grid_;
    std::unique_ptr<VideoPages> pages_;
};

class PlaylistScreen : public GridScreen {
public:
    PlaylistScreen(AppSession& session, const youtube::Video& playlist)
        : GridScreen(session, playlist.title, details(playlist)), playlistId_(playlist.id) {
        AppSession* shared = &session_;
        auto* play = buttons_->add<ui::Button>(ui::tr("playlist.play_all"), [this] { playFrom(0); });
        play->setPrimary(true);
        setPages(
            [shared](const std::string& token, const core::CancelToken& cancel) {
                if (!shared->account().signedIn()) {
                    return shared->youtube().feedMore(token, {}, cancel);
                }
                return shared->signedIn<youtube::VideoPage>(
                    [&](const std::string& access) { return shared->youtube().feedMore(token, access, cancel); },
                    cancel);
            },
            [this](const youtube::Video& video) { playFrom(indexOf(video.id)); });
        const std::string id = playlistId_;
        load(
            [shared, id](const core::CancelToken& cancel) {
                if (!shared->account().signedIn()) {
                    return shared->youtube().playlist(id, {}, cancel);  // a public playlist
                }
                return shared->signedIn<youtube::VideoPage>(
                    [&](const std::string& access) { return shared->youtube().playlist(id, access, cancel); },
                    cancel);
            },
            ui::tr("playlist.empty"));
    }

private:
    static std::string details(const youtube::Video& playlist) {
        std::string text = playlist.channelName;
        if (!playlist.videoCountText.empty()) {
            text += (text.empty() ? "" : " \xC2\xB7 ") + playlist.videoCountText;
        }
        return core::withoutEmoji(text);
    }

    std::size_t indexOf(const std::string& videoId) const {
        const std::vector<youtube::Video>& videos = pages_->videos();
        for (std::size_t i = 0; i < videos.size(); ++i) {
            if (videos[i].id == videoId) {
                return i;
            }
        }
        return 0;
    }

    // the videos loaded so far, in order, from `index`
    void playFrom(std::size_t index) {
        const std::vector<youtube::Video>& videos = pages_->videos();
        if (index < videos.size()) {
            openQueue(session_, std::make_shared<const std::vector<youtube::Video>>(videos), index);
        }
    }

    std::string playlistId_;
};

class ChannelPlaylistsScreen : public GridScreen {
public:
    ChannelPlaylistsScreen(AppSession& session, const std::string& channelId, const std::string& channelName)
        : GridScreen(session, ui::tr("channel.playlists"), core::withoutEmoji(channelName)) {
        AppSession* shared = &session_;
        setPages(
            [shared](const std::string& token, const core::CancelToken& cancel) {
                return shared->youtube().channelPlaylistsMore(token, cancel);
            },
            [shared, channelName](const youtube::Video& playlist) {
                youtube::Video withOwner = playlist;
                if (withOwner.channelName.empty()) {
                    withOwner.channelName = channelName;
                }
                openPlaylist(*shared, withOwner);
            });
        load([shared, channelId](const core::CancelToken& cancel) {
            return shared->youtube().channelPlaylists(channelId, cancel);
        },
             ui::tr("channel.no_playlists"));
    }
};

}  // namespace

void openPlaylist(AppSession& session, const youtube::Video& playlist) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && !playlist.id.empty()) {
        app->push(std::make_unique<PlaylistScreen>(session, playlist));
    }
}

void openChannelPlaylists(AppSession& session, const std::string& channelId, const std::string& channelName) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && !channelId.empty()) {
        app->push(std::make_unique<ChannelPlaylistsScreen>(session, channelId, channelName));
    }
}

}  // namespace playtube::app
