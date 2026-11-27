#include "app/shell/video_pages.hpp"

#include <algorithm>
#include <utility>

#include "app/shell/screens.hpp"
#include "core/log.hpp"

namespace playtube::app {

namespace {

constexpr int kNearEnd = 8;

}  // namespace

VideoPages::VideoPages(ShellScreen& screen, ui::VirtualList& list, Fetch more, VideosSource::Action onPress,
                       VideosSource::Action onChannel)
    : screen_(screen),
      list_(list),
      more_(std::move(more)),
      onPress_(std::move(onPress)),
      onChannel_(std::move(onChannel)) {
    list_.onNearEnd(kNearEnd, [this] { loadMore(); });
}

void dropShorts(youtube::VideoPage& page) {
    auto& items = page.items;
    items.erase(std::remove_if(items.begin(), items.end(), [](const youtube::Video& video) { return video.isShort; }),
                items.end());
}

void VideoPages::show(youtube::VideoPage page) {
    if (hideShorts) {
        dropShorts(page);
    }
    ++generation_;
    loading_ = false;
    continuation_ = std::move(page.continuation);
    list_.setSource(std::make_unique<VideosSource>(std::move(page.items), onPress_, onChannel_));
}

int VideoPages::count() const { return list_.source() != nullptr ? list_.source()->count() : 0; }

const std::vector<youtube::Video>& VideoPages::videos() const {
    static const std::vector<youtube::Video> none;
    auto* source = static_cast<VideosSource*>(list_.source());
    return source != nullptr ? source->videos() : none;
}

void VideoPages::loadMore() {
    if (loading_ || continuation_.empty() || !more_) {
        return;
    }
    loading_ = true;
    const unsigned generation = generation_;
    const std::string token = continuation_;
    Fetch more = more_;
    const core::CancelToken cancel = screen_.cancelToken();
    screen_.run<core::Result<youtube::VideoPage>>(
        [more, token, cancel] { return more(token, cancel); },
        [this, generation](core::Result<youtube::VideoPage> page) {
            if (generation != generation_) {
                return;  // the list was replaced meanwhile
            }
            loading_ = false;
            if (!page.ok()) {
                PF_LOG_WARN("next page: %s", page.error().message.c_str());
                list_.rearmNearEnd();  // the next move near the end asks again
                return;
            }
            auto* source = static_cast<VideosSource*>(list_.source());
            if (hideShorts) {
                dropShorts(page.value());
            }
            continuation_ = std::move(page.value().continuation);
            std::vector<youtube::Video>& videos = source->videos();
            for (youtube::Video& video : page.value().items) {
                videos.push_back(std::move(video));
            }
            if (page.value().items.empty()) {
                list_.rearmNearEnd();  // nothing new (all filtered out): the next move asks the page after
            }
            list_.reload();
        });
}

First signedInFeed(AppSession& session, const char* browseId) {
    AppSession* shared = &session;
    return [shared, browseId](const core::CancelToken& cancel) {
        return shared->signedIn<youtube::VideoPage>(
            [&](const std::string& token) { return shared->youtube().feed(browseId, token, cancel); }, cancel);
    };
}

VideoPages::Fetch signedInFeedMore(AppSession& session) {
    AppSession* shared = &session;
    return [shared](const std::string& continuation, const core::CancelToken& cancel) {
        return shared->signedIn<youtube::VideoPage>(
            [&](const std::string& token) { return shared->youtube().feedMore(continuation, token, cancel); },
            cancel);
    };
}

VideosSource::Action playAction(AppSession& session) {
    AppSession* shared = &session;
    return [shared](const youtube::Video& video) {
        if (video.playlist) {
            openPlaylist(*shared, video);
        } else if (video.isChannel) {
            openChannel(*shared, video.id);
        } else {
            openVideo(*shared, video.id);
        }
    };
}

VideosSource::Action channelAction(ShellScreen& screen) {
    ShellScreen* owner = &screen;
    return [owner](const youtube::Video& video) {
        if (video.isChannel) {
            openChannel(owner->session(), video.id);
        } else if (!video.playlist) {  // a playlist card carries no channel to open
            openChannelOf(*owner, video);
        }
    };
}

}  // namespace playtube::app
