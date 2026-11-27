#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/shell/session.hpp"
#include "ui/box.hpp"
#include "ui/image_view.hpp"
#include "ui/label.hpp"
#include "ui/list.hpp"
#include "youtube/models.hpp"

namespace playtube::app {

// A video card: the 400x225 thumbnail with its duration (or "EN DIRECT") in the corner, the title on two lines,
// the channel, then the views and the date.
ui::Size videoCardSize();

// "1,2 M de vues · il y a 3 jours" (either part may be missing).
std::string videoMeta(const youtube::Video& video);

// How much of a video was watched here (0 to 100, -1 never): the session's progress store, set once by the app.
using ProgressLookup = std::function<int(const std::string& videoId)>;
void setProgressLookup(ProgressLookup lookup);

// What pressing a Short does: the Shorts player on that Short, with the other Shorts of the same list around it
// (set once by the app).
using ShortsOpener = std::function<void(std::vector<youtube::Video> shorts, std::size_t index)>;
void setShortsOpener(ShortsOpener opener);

// DeArrow on a card (set once by the app): `ready` gets the video's title and thumbnail address when DeArrow has
// them (on the main thread, at once or later); nothing when the setting is off.
using BrandingSource = std::function<void(const std::string& videoId,
                                          std::function<void(const std::string& title, const std::string& thumbnail)>)>;
void setBrandingSource(BrandingSource source);

// △ on a video card: adds the video to the queue (set once by the app; none: △ does nothing).
using QueueAdder = std::function<void(const youtube::Video& video)>;
void setQueueAdder(QueueAdder adder);

// The red bar along the bottom of a thumbnail: the part watched (YouTube's or Playtube's own, the larger).
class WatchedBar : public ui::View {
public:
    WatchedBar();
    void set(int percent);  // -1 hides it
    void draw(NVGcontext* vg) const override;

private:
    int percent_ = -1;
};

// Focused, the card steps into the light: it grows to 1.07 and rises 6 px around its picture's centre, with a
// glow of the accent under it and an outline.
class VideoCard : public ui::Box {
public:
    explicit VideoCard(bool channelButton = true);  // false: □ does nothing (on the channel's own page)
    ~VideoCard() override;
    void show(const youtube::Video& video);
    std::function<void()> onPress;
    std::function<void()> onChannel;  // □
    std::function<void()> onQueue;    // △
    void onFocusChanged(bool focused) override;
    void draw(NVGcontext* vg) const override;

private:
    float focus_ = 0;
    void refreshWatched() const;
    void askBranding(const youtube::Video& video);
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);  // DeArrow's answers come after a card may be gone
    bool queueable_ = false;  // a video (not a live, a playlist or a channel): △ queues it
    std::string videoId_;     // a watchable video's id: its bar follows Playtube's progress
    int youtubePercent_ = -1;
    mutable int watchedPercent_ = -2;
    ui::ImageView* image_;
    WatchedBar* watched_;
    ui::Label* badge_;
    ui::Label* title_;
    ui::Label* channel_;
    ui::Label* meta_;
};

class VideosSource : public ui::DataSource {
public:
    using Action = std::function<void(const youtube::Video&)>;
    VideosSource(std::vector<youtube::Video> videos, Action onPress, Action onChannel);
    int count() const override { return static_cast<int>(videos_.size()); }
    std::unique_ptr<ui::View> create() override;
    void bind(ui::View& view, int index) override;
    std::vector<youtube::Video>& videos() { return videos_; }

private:
    std::vector<youtube::Video> videos_;
    Action onPress_;
    Action onChannel_;
};

// A titled horizontal row of video cards appended to `column`; the row is returned.
ui::HList* addVideoRow(ui::Box& column, const std::string& title);
// A grid of video cards, four per line, appended to `column`.
ui::Grid* addVideoGrid(ui::Box& column);

}  // namespace playtube::app
