#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/shell/cards.hpp"
#include "app/shell/screen.hpp"
#include "core/result.hpp"
#include "ui/list.hpp"
#include "youtube/models.hpp"

namespace playtube::app {

// The videos of a row or grid and their next pages, asked for once when the focus comes within 8 cards of the
// end. A page that arrives after show() replaced the list, or after the screen closed, is dropped.
class VideoPages {
public:
    using Fetch = std::function<core::Result<youtube::VideoPage>(const std::string& continuation,
                                                                 const core::CancelToken& cancel)>;
    VideoPages(ShellScreen& screen, ui::VirtualList& list, Fetch more, VideosSource::Action onPress,
               VideosSource::Action onChannel);

    void show(youtube::VideoPage page);
    int count() const;
    // the videos loaded so far, in order
    const std::vector<youtube::Video>& videos() const;
    // Shorts left out of every page (the setting off; never in search)
    bool hideShorts = false;

private:
    void loadMore();

    ShellScreen& screen_;
    ui::VirtualList& list_;
    Fetch more_;
    VideosSource::Action onPress_;
    VideosSource::Action onChannel_;
    std::string continuation_;
    bool loading_ = false;
    unsigned generation_ = 0;
};

// Removes the Shorts of a page (the setting "Shorts" off).
void dropShorts(youtube::VideoPage& page);

// The first page of a list.
using First = std::function<core::Result<youtube::VideoPage>(const core::CancelToken& cancel)>;

// A signed-in feed ("FEsubscriptions"…) and its next pages, with the account's token.
First signedInFeed(AppSession& session, const char* browseId);
VideoPages::Fetch signedInFeedMore(AppSession& session);
// What ✕ and □ do on a card.
VideosSource::Action playAction(AppSession& session);
VideosSource::Action channelAction(ShellScreen& screen);

}  // namespace playtube::app
