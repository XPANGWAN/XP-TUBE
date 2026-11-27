#include <nanovg.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/shell/autoplay.hpp"
#include "app/shell/chrome.hpp"
#include "app/shell/clock.hpp"
#include "app/shell/player_settings.hpp"
#include "app/shell/trickplay_preview.hpp"
#include "app/shell/watch_history.hpp"
#include "youtube/storyboard.hpp"
#include "app/shell/chat_panel.hpp"
#include "app/shell/comments_panel.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "core/log.hpp"
#include "core/utf.hpp"
#include "platform/platform.hpp"
#include "player/engine.hpp"
#include "player/seek.hpp"
#include "ui/context.hpp"
#include "ui/draw.hpp"
#include "ui/i18n.hpp"
#include "ui/icon_button.hpp"
#include "ui/image_view.hpp"
#include "ui/label.hpp"
#include "ui/sprites.hpp"
#include "ui/theme.hpp"
#include "ui/video_source.hpp"
#include "ui/widgets.hpp"
#include "youtube/playback.hpp"

namespace playtube::app {

namespace {

constexpr std::int64_t kBarMs = 4000;        // the bar hides after 4 s without input
constexpr std::int64_t kHoldGapMs = 300;     // presses closer than this are one held button
constexpr double kNextAheadSeconds = 10;     // "À suivre" shows 10 s before the end
constexpr double kMinDurationForNext = 30;   // not for very short videos
constexpr int kPageTries = 3;                // the video page: asked this many times in all when it fails
constexpr std::int64_t kPageRetryMs = 5000;  // 5 s after the first failure, 10 s after the second
const int kHeights[] = {360, 480, 720, 1080};

// The bar at 1080p (the approved canvas at 1280x720, times 1.5)
constexpr float kSide = 96;          // left and right margins
constexpr float kBottom = 60;        // under the icon row
constexpr float kGap = 21;           // between the bar's lines
constexpr float kHeadingGap = 9;     // between the title and the channel line
constexpr float kTitleSize = 42;     // semibold
constexpr float kChannelSize = 27;   // the channel line
constexpr float kAvatar = 42;        // the channel's initial in a circle
constexpr float kTimesSize = 26;     // the times line
constexpr float kRowTop = 51;        // above the icons: room for the focused one's name
constexpr float kIconGap = 21;
constexpr float kTrack = 6;          // the progress track's thickness
constexpr float kHead = 21;          // the round head at the position
constexpr float kProgressHeight = 24;
// The gradient, 560 px in all: transparent to 65 % before the title (white text must read over a white picture),
// to 85 % under the channel line, then to 94 % at the bottom
constexpr int kShadeTop = 140;
constexpr int kShadeMiddle = 120;
constexpr int kShadeBottom = 300;
constexpr std::uint8_t kShadeAtTitle = 0xA6;
constexpr std::uint8_t kShadeAtChannel = 0xD9;
constexpr std::uint8_t kShadeAtBottom = 0xF0;
const ui::Color kPlayedColor{0xFF, 0x00, 0x33, 0xFF};   // YouTube red: the part played and the head
const ui::Color kSegmentColor{0xF5, 0xC5, 0x18, 0xFF};  // SponsorBlock
const ui::Color kShadeColor{0x07, 0x07, 0x08, 0xFF};    // the gradient and the chapters' gaps
const ui::Color kAvatarColor{0x5B, 0x61, 0x70, 0xFF};

// The progress bar: a thin track, the part played and its round head in red, the SponsorBlock segments in yellow,
// a gap where each chapter begins.
class SegmentBar : public ui::View {
public:
    SegmentBar() { height = ui::SizeSpec::fixed(kProgressHeight); }
    void set(double position, double duration, const std::vector<youtube::Segment>& segments) {
        // The position moves at every frame, the bar's head a pixel every second or so: drawn again only when the
        // head moves (a redraw is the whole bar with its shade, 21 ms on the console while a video decodes).
        const int pixel = progressPixel(position, duration, frame.width);
        const bool same = duration == duration_ && segments.size() == segments_.size();
        position_ = position;
        if (same && pixel == pixel_ && pixel >= 0) {
            return;
        }
        pixel_ = pixel;
        duration_ = duration;
        segments_ = segments;
        markDirty();
    }
    // where chapters begin: a gap in the bar at each (none at 0)
    void setChapters(std::vector<double> starts) {
        chapters_ = std::move(starts);
        markDirty();
    }
    void draw(NVGcontext* vg) const override {
        const float top = (frame.height - kTrack) / 2;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, top, frame.width, kTrack, kTrack / 2);
        nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0x47));  // 28 %
        nvgFill(vg);
        if (duration_ <= 0) {
            return;
        }
        const auto at = [this](double seconds) {
            return static_cast<float>(std::min(1.0, std::max(0.0, seconds / duration_))) * frame.width;
        };
        for (const youtube::Segment& segment : segments_) {
            nvgBeginPath(vg);
            nvgRect(vg, at(segment.start), top, std::max(2.0f, at(segment.end) - at(segment.start)), kTrack);
            nvgFillColor(vg, ui::nvgColor(kSegmentColor));
            nvgFill(vg);
        }
        const float played = at(position_);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, top, played, kTrack, kTrack / 2);
        nvgFillColor(vg, ui::nvgColor(kPlayedColor));
        nvgFill(vg);
        // chapters: a dark gap where each one begins (the bar reads as parts, as on YouTube)
        for (double start : chapters_) {
            if (start <= 0 || start >= duration_) {
                continue;
            }
            nvgBeginPath(vg);
            nvgRect(vg, at(start) - 2, top, 4, kTrack);
            nvgFillColor(vg, ui::nvgColor(kShadeColor));
            nvgFill(vg);
        }
        nvgBeginPath(vg);
        nvgCircle(vg, played, frame.height / 2, kHead / 2);
        nvgFillColor(vg, ui::nvgColor(kPlayedColor));
        nvgFill(vg);
    }

private:
    double position_ = -1;
    double duration_ = 0;
    int pixel_ = -1;  // where the head was drawn
    std::vector<youtube::Segment> segments_;
    std::vector<double> chapters_;
};

// The dark gradient the bar stands on, at the bottom of the screen (no panel behind the bar): three fades baked once
// (ui::drawVerticalFade), drawn as pictures.
class BarShade : public ui::View {
public:
    BarShade() {
        width = ui::SizeSpec::fill();
        height = ui::SizeSpec::fill();
    }
    void draw(NVGcontext* vg) const override {
        const int across = static_cast<int>(frame.width);
        const float bottom = frame.height;
        ui::drawVerticalFade(vg, 0, bottom - kShadeBottom - kShadeMiddle - kShadeTop, across, kShadeTop, kShadeTop,
                             kShadeColor, 0, kShadeAtTitle);
        ui::drawVerticalFade(vg, 0, bottom - kShadeBottom - kShadeMiddle, across, kShadeMiddle, kShadeMiddle,
                             kShadeColor, kShadeAtTitle, kShadeAtChannel);
        ui::drawVerticalFade(vg, 0, bottom - kShadeBottom, across, kShadeBottom, kShadeBottom, kShadeColor,
                             kShadeAtChannel, kShadeAtBottom);
    }
};

// The channel's initial in a grey circle, at the start of the channel line.
class ChannelInitial : public ui::Stack {
public:
    ChannelInitial() {
        width = ui::SizeSpec::fixed(kAvatar);
        height = ui::SizeSpec::fixed(kAvatar);
        cornerRadius = kAvatar / 2;
        background = kAvatarColor;
        horizontal = ui::CrossAlign::Center;
        vertical = ui::CrossAlign::Center;
        letter_ = add<ui::Label>("", ui::FontStyle{21, ui::FontWeight::Bold});
        letter_->color = ui::theme().text;
    }
    void setLetter(const std::string& letter) {
        if (letter != letter_->text()) {
            letter_->setText(letter);
        }
    }
    // The channel's own picture over the initial (which shows until it comes, and when it does not)
    void setPicture(const std::string& url) {
        if (picture_ == nullptr) {
            picture_ = add<ui::ImageView>();
            picture_->width = ui::SizeSpec::fixed(kAvatar);
            picture_->height = ui::SizeSpec::fixed(kAvatar);
            picture_->cornerRadius = kAvatar / 2;
            picture_->placeholder = ui::Color{0, 0, 0, 0};
        }
        picture_->setUrl(url);
    }
    void onThemeChanged() override {
        letter_->color = ui::theme().text;
        ui::Stack::onThemeChanged();
    }

private:
    ui::Label* letter_;
    ui::ImageView* picture_ = nullptr;
};

struct Extras {
    core::Result<youtube::NextReply> next = core::makeError(core::ErrorKind::Cancelled, "");
    std::vector<youtube::Segment> segments;
    std::optional<youtube::VideoState> state;  // signed in: the account's rating and subscription
    std::optional<youtube::Votes> votes;       // Return YouTube Dislike's, when on
};

player::EngineOptions engineOptions(const AppSession& session, const std::string& userAgent) {
    const std::string resources = platform::resourceDir();
    player::EngineOptions options;
#ifdef PLAYTUBE_PS4
    options.audioOutput = "sdl";
    options.decoderThreads = 4;  // of the 6 cores an app gets (the UI uses the others)
#endif
    if (session.synchronous) {
        options.audioOutput = "null";  // tests
    }
    options.fontsDir = resources + "/fonts";
    options.caFile = session.caFile();
    options.userAgent = userAgent;
    options.subtitleScale = subtitleScale(session.settings().subtitleSize);
    options.subtitleBackground = session.settings().subtitleBackground;
    return options;
}

// when a video ends if it plays on from `position` at `speed` (the clock's seconds)
std::int64_t endsAtSeconds(const AppSession& session, double duration, double position, double speed) {
    const double left = (duration - position) / (speed > 0 ? speed : 1.0);
    return session.clockNow() + static_cast<std::int64_t>(left > 0 ? left + 0.5 : 0);
}

const char* const kFits[] = {"fit", "fill", "stretch"};
const int kSizes[] = {100, 95, 90, 85, 80};  // a smaller picture only makes sense whole

// "Entière", "Entière · 90 %", "Remplir l'écran": how the picture fills the screen
std::string pictureText(const std::string& fit, int size) {
    std::string name = ui::tr("player.picture." + fit);
    if (fit == "fit" && size != 100) {
        name += " \xC2\xB7 " + std::to_string(size) + " %";
    }
    return name;
}

class PlayerScreen : public ShellScreen, public ui::VideoSource {
public:
    PlayerScreen(AppSession& session, std::string videoId, Queue queue, std::size_t index, bool shorts)
        : ShellScreen(session),
          videoId_(std::move(videoId)),
          maxHeight_(session.settings().maxHeight),
          queue_(std::move(queue)),
          shorts_(shorts),
          index_(index) {
        rememberPlayed(session.played(), videoId_);  // what plays next by itself is not a video already played
        // from a playlist: its next video comes after this one (else the related videos); Shorts loop instead
        for (std::size_t i = index + 1; !shorts_ && queue_ && i < queue_->size(); ++i) {
            const youtube::Video& video = (*queue_)[i];
            if (!video.live && !video.id.empty() && (session.settings().shorts || !video.isShort)) {
                nextVideo_ = video;
                queueNext_ = i;
                break;
            }
        }
        // where it was left, unless barely started or watched to its end
        if (const auto progress = session.progress().find(videoId_)) {
            lastPosition_ = static_cast<double>(ProgressStore::resumeAt(*progress));
            resumedFrom_ = lastPosition_;
        }
        if (session.account().signedIn() && session.settings().watchHistory) {  // into the account's history
            history_ = std::make_unique<WatchHistory>(session, videoId_);
        }
        focusable = true;
        buildOverlay();
        bindControls();
    }
    ~PlayerScreen() override { release(); }

    void onShow() override {
        if (ui::Application* app = ui::Application::current()) {
            app->setVideoSource(this);
        }
        shown_ = true;
        if (!started_ || !engine_) {  // the first time, or back after something hid it (its engine went)
            started_ = true;
            showBar();
            start(lastPosition_);
        }
    }
    void onHide() override {
        shown_ = false;
        release();
    }

    bool renderVideo() override {
        if (!engine_) {
            return false;
        }
        for (player::EngineEvent event : engine_->drainEvents()) {
            onEngineEvent(event);
            if (!engine_) {
                return false;
            }
        }
        const bool fresh = engine_->renderIfDue(frame_);
        tick(fresh);
        return fresh;
    }

    platform::VideoLayer videoLayer() const override {
        platform::VideoLayer layer{frame_.pixels.empty() ? nullptr : frame_.pixels.data(), frame_.width,
                                   frame_.height, false};
        const std::string& fit = session_.settings().pictureFit;
        layer.fit = fit == "fill"      ? platform::VideoFit::Fill
                    : fit == "stretch" ? platform::VideoFit::Stretch
                                       : platform::VideoFit::Fit;
        layer.scale = session_.settings().pictureSize / 100.0;
        return layer;
    }

private:
    static std::int64_t now() { return static_cast<std::int64_t>(ui::context().timeMs); }

    // ---- overlay

    void buildOverlay() {
        hidesHints = true;  // the bar and its icons say enough while a video plays
        auto* middle = add<ui::Stack>();
        middle->width = ui::SizeSpec::fill();
        middle->height = ui::SizeSpec::fill();
        middle->horizontal = ui::CrossAlign::Center;
        middle->vertical = ui::CrossAlign::Center;
        spinner_ = middle->add<ui::Spinner>();

        shade_ = add<BarShade>();  // under the bar, shown with it

        auto* bottom = add<ui::Box>(ui::Axis::Column);
        bottom->width = ui::SizeSpec::fill();
        bottom->height = ui::SizeSpec::fill();
        bottom->align = ui::Align::End;
        bottom->padding = {0, kSide, kBottom, kSide};
        bottom->gap = ui::theme().spacingL;

        // "À suivre": the next video, its picture, title and countdown, at the right
        auto* nextRow = bottom->add<ui::Box>(ui::Axis::Row);
        nextRow->width = ui::SizeSpec::fill();
        nextRow->align = ui::Align::End;
        next_ = nextRow->add<ui::Box>(ui::Axis::Row);
        next_->background = ui::withAlpha(ui::theme().surfaceRaised, 0xE6);
        next_->cornerRadius = ui::theme().radius;
        next_->padding = ui::Insets::all(ui::theme().spacingM);
        next_->gap = ui::theme().spacingM;
        next_->crossAlign = ui::CrossAlign::Center;
        next_->visible = false;
        nextImage_ = next_->add<ui::ImageView>();
        nextImage_->width = ui::SizeSpec::fixed(256);
        nextImage_->height = ui::SizeSpec::fixed(144);
        nextImage_->cornerRadius = 8;
        nextImage_->clip = true;
        auto* nextTexts = next_->add<ui::Box>(ui::Axis::Column);
        nextTexts->gap = ui::theme().spacingS;
        auto* nextHeading = nextTexts->add<ui::Label>(ui::tr("player.next"), bodyStrong());
        nextHeading->color = ui::theme().textSecondary;
        nextTitle_ = nextTexts->add<ui::Label>("", bodyStrong());
        nextTitle_->width = ui::SizeSpec::fixed(460);
        nextTitle_->maxLines = 2;
        nextCountdown_ = nextTexts->add<ui::Label>("", bodyText());
        nextCountdown_->color = ui::theme().textSecondary;

        // the bar, top to bottom: title, channel line, progress, times, icon row (no panel: the gradient under it)
        bar_ = bottom->add<ui::Box>(ui::Axis::Column);
        bar_->width = ui::SizeSpec::fill();
        bar_->gap = kGap;
        auto* heading = bar_->add<ui::Box>(ui::Axis::Column);
        heading->width = ui::SizeSpec::fill();
        heading->gap = kHeadingGap;
        title_ = heading->add<ui::Label>("", ui::FontStyle{kTitleSize, ui::FontWeight::SemiBold});
        title_->width = ui::SizeSpec::fill();
        auto* channelLine = heading->add<ui::Box>(ui::Axis::Row);
        channelLine->width = ui::SizeSpec::fill();
        channelLine->gap = 15;
        channelLine->crossAlign = ui::CrossAlign::Center;
        avatar_ = channelLine->add<ChannelInitial>();
        avatar_->visible = false;
        channel_ = channelLine->add<ui::Label>("", ui::FontStyle{kChannelSize, ui::FontWeight::Regular});
        channel_->width = ui::SizeSpec::fill();
        channel_->color = ui::theme().textSecondary;
        progress_ = bar_->add<SegmentBar>();
        progress_->width = ui::SizeSpec::fill();
        // "2:41 / 10:23 · <chapter>" at the left (the position brighter), "Se termine à 21:47" at the right, always whole
        auto* times = bar_->add<ui::Box>(ui::Axis::Row);
        times->width = ui::SizeSpec::fill();
        times->gap = 8;  // a space
        times->crossAlign = ui::CrossAlign::Center;
        live_ = times->add<ui::Label>(ui::tr("video.live"),
                                      ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::SemiBold});
        live_->background = ui::Color{0xCC, 0x00, 0x00, 0xFF};
        live_->cornerRadius = 6;
        live_->padding = {2, 8, 2, 8};
        live_->visible = false;
        position_ = times->add<ui::Label>("", ui::FontStyle{kTimesSize, ui::FontWeight::Regular});
        position_->color = ui::theme().text;
        rest_ = times->add<ui::Label>("", ui::FontStyle{kTimesSize, ui::FontWeight::Regular});
        rest_->color = ui::theme().textSecondary;
        rest_->width = ui::SizeSpec::fill();  // what "Se termine à" leaves: a long chapter is cut with an ellipsis
        endsAt_ = times->add<ui::Label>("", ui::FontStyle{kTimesSize, ui::FontWeight::Regular});
        endsAt_->color = ui::theme().textSecondary;
        row_ = bar_->add<ui::Box>(ui::Axis::Row);
        row_->width = ui::SizeSpec::fill();
        row_->padding = {kRowTop, 0, 0, 0};
        row_->crossAlign = ui::CrossAlign::End;
        buildRow();
        preview_ = add<TrickplayPreview>();  // over the bar
        preview_->setAnchor(progress_, bar_);
    }

    void showBar() {
        barUntil_ = now() + kBarMs;
        setBarShown(true);
    }

    void setBarShown(bool shown) {
        if (bar_->visible != shown) {
            bar_->visible = shown;
            shade_->visible = shown;
            markDirty();
        }
    }

    // a live: its LIVE badge where the times are, and no progress bar
    void showTimes(bool shown) {
        progress_->visible = shown;
        position_->visible = shown;
        rest_->visible = shown;
        endsAt_->visible = shown;
        live_->visible = !shown;
        markDirty();
    }

    void refreshBar(const player::Engine::State& state) {
        if (streams_.live) {
            return;
        }
        const double position = seek_.pending() ? seek_.target() : state.position;
        const std::string at = clockText(position);
        std::string rest = "/ " + clockText(state.duration);
        if (const int chapter = chapterAt(position); chapter >= 0) {  // the part it is in
            rest += " \xC2\xB7 " + core::withoutEmoji(chapters_[static_cast<std::size_t>(chapter)].title);
        }
        if (at != position_->text()) {
            position_->setText(at);
        }
        if (rest != rest_->text()) {
            rest_->setText(rest);
        }
        // when it would end, playing on from here: written again only when that minute changes
        const std::int64_t endsMinute =
            state.duration > 0 ? endsAtSeconds(session_, state.duration, position, speed_) / 60 : -1;
        if (endsMinute != endsMinute_) {
            endsMinute_ = endsMinute;
            const std::int64_t end = endsMinute * 60;
            endsAt_->setText(endsMinute < 0 ? std::string()
                                            : ui::tr("player.ends_at", {clock24(end, session_.utcOffsetMinutes(end))}));
        }
        preview_->update(seek_.pending(), position, state.duration, lastDirection_, clockText(position));
        progress_->set(position, state.duration, segments_);
    }

    // ---- the icon row, always part of the bar. Left: Pause/Lecture, Depuis le début, J'aime, Je n'aime pas, Chat en
    // direct, Commentaires, Enregistrer, S'abonner, File d'attente, Chaîne; right: Sous-titres, Paramètres. Each
    // shows only when it applies (refreshRow).

    void buildRow() {
        auto* left = row_->add<ui::Box>(ui::Axis::Row);
        left->gap = kIconGap;
        auto* space = row_->add<ui::View>();
        space->width = ui::SizeSpec::fill();
        auto* right = row_->add<ui::Box>(ui::Axis::Row);
        right->gap = kIconGap;
        // the actions run on the next frame: some replace or cover this screen
        const auto icon = [this](ui::Box* group, ui::Icon shape, const std::string& label,
                                 std::function<void()> action) {
            return group->add<ui::IconButton>(shape, label, [this, action] { later(action); });
        };
        pauseIcon_ = icon(left, ui::Icon::Pause, ui::tr("player.pause"), [this] { togglePause(); });
        fromStartIcon_ = icon(left, ui::Icon::Restart, ui::tr("player.from_start"), [this] {
            if (engine_) {
                engine_->seekTo(0);
            }
            leaveRow();
        });
        likeIcon_ = icon(left, ui::Icon::Like, ui::tr("player.like"), [this] { rate(true); });
        dislikeIcon_ = icon(left, ui::Icon::Dislike, ui::tr("player.dislike"), [this] { rate(false); });
        chatIcon_ = icon(left, ui::Icon::LiveChat, ui::tr("player.live_chat"), [this] { openLiveChat(); });
        commentsIcon_ = icon(left, ui::Icon::Comments, ui::tr("player.comments"), [this] { openComments(); });
        saveIcon_ = icon(left, ui::Icon::Save, ui::tr("player.save"), [this] { save(); });
        subscribeIcon_ = icon(left, ui::Icon::Bell, ui::tr("channel.subscribe"), [this] { toggleSubscription(); });
        queueIcon_ = icon(left, ui::Icon::Queue, "", [this] { showQueue(); });
        channelIcon_ = icon(left, ui::Icon::Channel, ui::tr("video.channel"), [this] { openChannelHere(); });
        captionsIcon_ = icon(right, ui::Icon::Captions, ui::tr("player.subtitles"), [this] { chooseSubtitles(); });
        settingsIcon_ = icon(right, ui::Icon::Settings, ui::tr("player.settings"), [this] { openSettings(); });
        settingsIcon_->setLabelAlign(ui::CrossAlign::End);  // its name stays on screen
        refreshRow();
    }

    // which icons apply to this video (the streams, the video page, the account and the queue tell)
    void refreshRow() {
        const bool signedIn = session_.account().signedIn();
        const bool channel = !channelId().empty();
        fromStartIcon_->visible = !streams_.live && resumedFrom_ > 0;
        likeIcon_->visible = signedIn;
        dislikeIcon_->visible = signedIn;
        chatIcon_->visible = streams_.live && !liveChat_.empty();
        saveIcon_->visible = signedIn;
        subscribeIcon_->visible = signedIn && channel;
        const std::size_t queued = session_.watchQueue().size();
        queueIcon_->visible = queued > 0;
        const std::string queueLabel = ui::tr("queue.button", {std::to_string(queued)});
        if (queueLabel != queueIcon_->label()) {
            queueIcon_->setLabel(queueLabel);
        }
        channelIcon_->visible = channel;
        if (!channelName_.empty()) {  // its initial once known (kept after); a channel glyph until then
            channelIcon_->setInitial(initialOf(channelName_));
            channelIcon_->setPicture(channelAvatar_);
        }
        captionsIcon_->visible = !streams_.captions.empty();
        // the focused icon went: Pause/Lecture instead
        if (ui::Application* app = ui::Application::current()) {
            ui::View* focused = app->focus().focused();
            if (focused != nullptr && row_->isAncestorOf(focused) && !focused->visible) {
                app->focus().focus(pauseIcon_);
            }
        }
        refreshActions();
    }

    // Pause or Lecture, as the video is
    void refreshPause(bool paused) {
        if (paused == pausedShown_) {
            return;
        }
        pausedShown_ = paused;
        pauseIcon_->setIcon(paused ? ui::Icon::Play : ui::Icon::Pause);
        pauseIcon_->setLabel(ui::tr(paused ? "player.play" : "player.pause"));
    }

    bool rowFocused() const {
        ui::Application* app = ui::Application::current();
        const ui::View* focused = app != nullptr ? app->focus().focused() : nullptr;
        return focused != nullptr && row_->isAncestorOf(focused);
    }

    // ↑ / ↓ / △ / OPTIONS: the bar, with the focus on Pause/Lecture
    void focusRow() {
        showBar();
        if (ui::Application* app = ui::Application::current()) {
            app->focus().focus(pauseIcon_);
        }
        markDirty();
    }

    // the focus came back to an icon (a modal it opened closed): the bar shows again
    void onDescendantFocused(ui::View* view) override {
        if (!bar_->visible && row_->isAncestorOf(view)) {
            showBar();
        }
    }
    // or to the screen itself (the icon that opened the modal went meanwhile)
    void onFocusChanged(bool focused) override {
        if (focused && !bar_->visible) {
            showBar();
        }
    }

    // what has the focus now: taken before a modal is built (a panel may focus its own content as it is made)
    static ui::View* focusedNow() {
        ui::Application* app = ui::Application::current();
        return app != nullptr ? app->focus().focused() : nullptr;
    }

    // a modal over the player, the bar hidden (its row would lie under the modal's hints and panels); when it
    // closes the focus goes back to `opener` (an icon, a line of the panel) and the bar with it
    void showModal(ui::View* opener, std::unique_ptr<ui::View> modal) {
        if (ui::Application* app = ui::Application::current()) {
            app->showModal(std::move(modal), opener);
            setBarShown(false);
        }
    }

    // ◯: back to the video (← / → seek again); the bar hides as usual
    void leaveRow() {
        if (ui::Application* app = ui::Application::current(); app != nullptr && rowFocused()) {
            app->focus().focus(this);
        }
        showBar();
        markDirty();
    }

    // the rating and the subscription YouTube confirmed (the active rating filled, a ticked bell when subscribed),
    // the counts in the names; signed out, the counts go in the channel line
    void refreshActions() {
        const youtube::Rating rating = state_ ? state_->rating : youtube::Rating::None;
        std::string like = ui::tr("player.like");
        if (state_ && !state_->likesText.empty()) {
            like += " \xC2\xB7 " + state_->likesText;
        } else if (votes_) {  // YouTube gave no count: Return YouTube Dislike's
            like += " \xC2\xB7 " + compactCount(votes_->likes);
        }
        likeIcon_->setLabel(like);
        likeIcon_->setIcon(rating == youtube::Rating::Like ? ui::Icon::LikeFilled : ui::Icon::Like);
        std::string dislike = ui::tr("player.dislike");
        if (votes_) {
            dislike += " \xC2\xB7 " + compactCount(votes_->dislikes);
        }
        dislikeIcon_->setLabel(dislike);
        dislikeIcon_->setIcon(rating == youtube::Rating::Dislike ? ui::Icon::DislikeFilled : ui::Icon::Dislike);
        const bool subscribed = state_ && state_->subscribed;
        subscribeIcon_->setLabel(ui::tr(subscribed ? "channel.subscribed" : "channel.subscribe"));
        subscribeIcon_->setIcon(subscribed ? ui::Icon::BellTicked : ui::Icon::Bell);
        std::string line = channelName_;
        if (!session_.account().signedIn() && votes_) {  // no buttons to rate: the counts alone
            const std::string counts =
                ui::tr("player.votes", {compactCount(votes_->likes), compactCount(votes_->dislikes)});
            line = line.empty() ? counts : line + " \xC2\xB7 " + counts;
        }
        if (line != channel_->text()) {
            channel_->setText(line);
        }
        avatar_->visible = !channelName_.empty();
        avatar_->setLetter(initialOf(channelName_));
        if (!channelAvatar_.empty()) {
            avatar_->setPicture(channelAvatar_);
        }
        markDirty();
    }

    // Paramètres: its panel at the right over the dimmed picture (the bar hidden), each line with its value and
    // opening its chooser. Qualité and Vitesse not on a live, Piste audio only with several tracks.
    std::vector<PlayerSettingRow> settingRows() {
        std::vector<PlayerSettingRow> rows;
        if (!streams_.live) {
            rows.push_back({ui::tr("player.quality"), std::to_string(maxHeight_) + "p", [this] { chooseQuality(); }});
            rows.push_back({ui::tr("player.speed"), speedText(speed_), [this] { chooseSpeed(); }});
        }
        const Settings& settings = session_.settings();
        rows.push_back({ui::tr("player.volume"), std::to_string(settings.volume) + " %", [this] { chooseVolume(); }});
        rows.push_back({ui::tr("player.picture"), pictureText(settings.pictureFit, settings.pictureSize),
                        [this] { choosePicture(); }});
        if (streams_.audioTracks.size() > 1) {
            rows.push_back({ui::tr("player.audio"), audioText(), [this] { chooseAudio(); }});
        }
        return rows;
    }

    // the audio track playing: its name (its id when it has none)
    std::string audioText() const {
        for (const youtube::AudioTrack& track : streams_.audioTracks) {
            if (track.id == streams_.audioTrack) {
                return track.name.empty() ? track.id : track.name;
            }
        }
        return {};
    }

    void openSettings() {
        ui::Application* app = ui::Application::current();
        if (app == nullptr) {
            return;
        }
        ui::View* opener = focusedNow();
        auto panel = makePlayerSettings(settingRows());
        panel->trackIn(settings_);
        showModal(opener, std::move(panel));  // the picture alone behind the panel (showModal hides the bar)
    }

    // a chooser changed something: the panel, while open, shows the new value
    void refreshSettings() {
        PlayerSettings* panel = *settings_;  // null once the panel is gone
        if (panel == nullptr) {
            return;
        }
        for (const PlayerSettingRow& row : settingRows()) {
            panel->setValue(row.name, row.value);
        }
    }

    // J'aime / Je n'aime pas; pressing the active one takes the rating back
    void rate(bool like) {
        if (rating_) {
            return;  // one change at a time
        }
        const youtube::Rating current = state_ ? state_->rating : youtube::Rating::None;
        const youtube::Rating pressed = like ? youtube::Rating::Like : youtube::Rating::Dislike;
        const youtube::Rating wanted = current == pressed ? youtube::Rating::None : pressed;
        rating_ = true;
        AppSession* shared = &session_;
        const std::string id = videoId_;
        run<core::Result<core::Unit>>(
            [shared, id, wanted] {
                return shared->signedIn<core::Unit>(
                    [&](const std::string& token) { return shared->youtube().rate(id, wanted, token); });
            },
            [this, wanted](core::Result<core::Unit> done) {
                rating_ = false;
                if (!done.ok()) {
                    PF_LOG_WARN("player: rating: %s", done.error().message.c_str());
                    toast(errorText(done.error()));
                    return;
                }
                if (!state_) {
                    state_ = youtube::VideoState{};
                }
                state_->rating = wanted;
                userRated_ = true;
                refreshActions();
            });
    }

    void toggleSubscription() {
        if (subscribing_) {
            return;
        }
        const std::string channel = channelId();
        const bool wanted = !(state_ && state_->subscribed);
        subscribing_ = true;
        AppSession* shared = &session_;
        run<core::Result<core::Unit>>(
            [shared, channel, wanted] {
                return shared->signedIn<core::Unit>(
                    [&](const std::string& token) { return shared->youtube().setSubscribed(channel, wanted, token); });
            },
            [this, wanted](core::Result<core::Unit> done) {
                subscribing_ = false;
                if (!done.ok()) {
                    PF_LOG_WARN("player: subscription: %s", done.error().message.c_str());
                    toast(errorText(done.error()));
                    return;
                }
                if (!state_) {
                    state_ = youtube::VideoState{};
                }
                state_->subscribed = wanted;
                userSubscribed_ = true;
                refreshActions();
            });
    }

    // "Enregistrer": "À regarder plus tard" and the account's playlists; the chosen one gets the video
    void save() {
        AppSession* shared = &session_;
        const std::string id = videoId_;
        run<core::Result<std::vector<youtube::PlaylistChoice>>>(
            [shared, id] {
                return shared->signedIn<std::vector<youtube::PlaylistChoice>>(
                    [&](const std::string& token) { return shared->youtube().playlistChoices(id, token); });
            },
            [this](core::Result<std::vector<youtube::PlaylistChoice>> choices) {
                if (!choices.ok()) {
                    PF_LOG_WARN("player: playlists: %s", choices.error().message.c_str());
                    toast(errorText(choices.error()));
                    return;
                }
                if (choices.value().empty()) {
                    return;
                }
                auto list = std::make_shared<std::vector<youtube::PlaylistChoice>>(std::move(choices.value()));
                std::vector<std::string> names;
                for (const youtube::PlaylistChoice& choice : *list) {
                    const std::string title = core::withoutEmoji(choice.title);
                    names.push_back(choice.contains ? ui::tr("player.save.contains", {title}) : title);
                }
                ui::View* opener = focusedNow();
                showModal(opener, ui::makeChoiceDialog(ui::tr("player.save.title"), names, 0, [this, list](int index) {
                    addTo((*list)[static_cast<std::size_t>(index)]);
                }));
            });
    }

    void addTo(const youtube::PlaylistChoice& choice) {
        const std::string title = core::withoutEmoji(choice.title);
        if (choice.contains) {
            toast(ui::tr("player.save.contains", {title}));
            return;
        }
        AppSession* shared = &session_;
        const std::string id = videoId_;
        const std::string playlist = choice.id;
        run<core::Result<core::Unit>>(
            [shared, id, playlist] {
                return shared->signedIn<core::Unit>(
                    [&](const std::string& token) { return shared->youtube().addToPlaylist(playlist, id, token); });
            },
            [title](core::Result<core::Unit> added) {
                toast(added.ok() ? ui::tr("player.saved", {title}) : errorText(added.error()));
            });
    }

    // the comments panel on the right; the video keeps playing under it
    void openComments() {
        ui::Application* app = ui::Application::current();
        if (app == nullptr) {
            return;
        }
        if (!commentsChecked_) {
            fetchCommentsStart();
            return;
        }
        AppSession* shared = &session_;
        const core::CancelToken cancel = token_;
        ui::View* opener = focusedNow();  // the panel focuses its first comment as it is made (pages come inline)
        auto panel = std::make_unique<CommentsPanel>(
            commentsContinuation_, [this, shared, cancel](const std::string& token, CommentsPanel::Done done) {
                run<core::Result<youtube::CommentPage>>(
                    [shared, token, cancel] { return shared->youtube().comments(token, cancel); },
                    [done](core::Result<youtube::CommentPage> page) { done(std::move(page)); });
            });
        showModal(opener, std::move(panel));
    }

    // the live chat in its panel over the video, its pages asked for as long as it is open
    void openLiveChat() {
        ui::Application* app = ui::Application::current();
        if (app == nullptr || liveChat_.empty()) {
            return;
        }
        AppSession* shared = &session_;
        const core::CancelToken cancel = token_;
        ui::View* opener = focusedNow();
        auto panel = std::make_unique<ChatPanel>(
            liveChat_, [this, shared, cancel](const std::string& token, ChatPanel::Done done) {
                run<core::Result<youtube::ChatPage>>(
                    [shared, token, cancel] { return shared->youtube().liveChat(token, cancel); },
                    [done](core::Result<youtube::ChatPage> page) { done(std::move(page)); });
            });
        showModal(opener, std::move(panel));
    }

    // the video page failed or has not answered: asked (again) for its comments' first page, then the panel
    void fetchCommentsStart() {
        if (commentsFetching_) {
            return;
        }
        commentsFetching_ = true;
        toast(ui::tr("ui.loading"));
        AppSession* shared = &session_;
        const std::string id = videoId_;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::NextReply>>(
            [shared, id, cancel] { return shared->youtube().next(id, cancel); },
            [this](core::Result<youtube::NextReply> next) {
                commentsFetching_ = false;
                if (!next.ok()) {
                    PF_LOG_WARN("player: comments: %s", next.error().message.c_str());
                    toast(errorText(next.error()));
                    return;
                }
                if (!commentsChecked_) {
                    commentsContinuation_ = next.value().commentsContinuation;
                    commentsChecked_ = true;
                }
                openComments();
            });
    }

    static void toast(const std::string& text) {
        if (ui::Application* app = ui::Application::current()) {
            app->toast(text);
        }
    }

    void chooseQuality() {
        std::vector<std::string> names;
        int selected = 3;
        for (int i = 0; i < 4; ++i) {
            names.push_back(std::to_string(kHeights[i]) + "p");
            selected = kHeights[i] == maxHeight_ ? i : selected;
        }
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.quality"), names, selected, [this](int index) {
            maxHeight_ = kHeights[index];
            restart();  // the same second at the new quality
            refreshSettings();
        }));
    }

    // YouTube's storyboards of the video, for the preview while seeking (not for a live)
    void setupPreview() {
        if (streams_.live || preview_ == nullptr) {
            return;
        }
        const auto board = youtube::parseStoryboard(streams_.storyboardSpec, streams_.durationSeconds);
        if (!board) {
            return;
        }
        player::TrickplayInfo info;
        info.width = board->width;
        info.height = board->height;
        info.tileWidth = board->columns;
        info.tileHeight = board->rows;
        info.thumbnailCount = board->count;
        info.intervalMs = board->intervalMs;
        const youtube::Storyboard sheets = *board;
        preview_->setPictures(info, [sheets](int sheet) { return sheets.sheetUrl(sheet); });
        PF_LOG_INFO("player: storyboard of %dx%d pictures", board->width, board->height);
    }

    // the queued videos, in order, with a way to empty the queue
    void showQueue() {
        const auto& queue = session_.watchQueue();
        std::string text;
        for (std::size_t i = 0; i < queue.size() && i < 8; ++i) {
            text += (text.empty() ? "" : "\n") + std::to_string(i + 1) + ". " + core::withoutEmoji(queue[i].title);
        }
        if (queue.size() > 8) {
            text += "\n\xE2\x80\xA6";
        }
        AppSession* shared = &session_;
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeDialog(ui::tr("queue.title"), text,
                                         {{ui::tr("queue.clear"),
                                           [this, shared] {
                                               shared->watchQueue().clear();
                                               refreshRow();  // its icon goes
                                           }},
                                          {ui::tr("ui.close"), {}}}));
    }

    // ---- volume and picture (kept in the settings for the next videos)

    void chooseVolume() {
        std::vector<std::string> names;
        int selected = 0;
        for (int i = 0; i <= 10; ++i) {
            const int percent = 100 - i * 10;
            names.push_back(std::to_string(percent) + " %");
            selected = percent == session_.settings().volume ? i : selected;
        }
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.volume"), names, selected, [this](int index) {
            session_.settings().volume = 100 - index * 10;
            session_.saveSettings();
            if (engine_) {
                engine_->setVolume(session_.settings().volume);
            }
            refreshSettings();
        }));
    }

    void choosePicture() {
        std::vector<std::string> names;
        int selected = 0;
        for (int f = 0; f < 3; ++f) {
            for (int z = 0; z < (f == 0 ? 5 : 1); ++z) {
                if (session_.settings().pictureFit == kFits[f] &&
                    (f != 0 || session_.settings().pictureSize == kSizes[z])) {
                    selected = static_cast<int>(names.size());
                }
                names.push_back(pictureText(kFits[f], kSizes[z]));
            }
        }
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.picture"), names, selected, [this](int index) {
            Settings& settings = session_.settings();
            settings.pictureFit = index < 5 ? "fit" : index == 5 ? "fill" : "stretch";
            settings.pictureSize = index < 5 ? kSizes[index] : 100;
            session_.saveSettings();
            markDirty();
            refreshSettings();
        }));
    }

    void chooseSpeed() {
        std::vector<std::string> names;
        int selected = 2;
        const auto& speeds = youtube::playbackSpeeds();
        for (std::size_t i = 0; i < speeds.size(); ++i) {
            names.push_back(speedText(speeds[i]));
            selected = speeds[i] == speed_ ? static_cast<int>(i) : selected;
        }
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.speed"), names, selected, [this](int index) {
            speed_ = youtube::playbackSpeeds()[static_cast<std::size_t>(index)];
            if (engine_) {
                engine_->setSpeed(speed_);
            }
            refreshSettings();
        }));
    }

    // YouTube's dubbed languages: the chosen one plays from the same moment, and stays for later restarts
    void chooseAudio() {
        std::vector<std::string> names;
        int selected = 0;
        for (std::size_t i = 0; i < streams_.audioTracks.size(); ++i) {
            names.push_back(streams_.audioTracks[i].name.empty() ? streams_.audioTracks[i].id
                                                                 : streams_.audioTracks[i].name);
            selected = streams_.audioTracks[i].id == streams_.audioTrack ? static_cast<int>(i) : selected;
        }
        ui::View* opener = focusedNow();
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.audio"), names, selected, [this](int index) {
            const youtube::AudioTrack& track = streams_.audioTracks[static_cast<std::size_t>(index)];
            if (track.id == streams_.audioTrack || !engine_) {
                return;
            }
            audioTrack_ = track.id;
            PF_LOG_INFO("player: audio %s", track.id.c_str());
            streams_.audioTrack = track.id;
            streams_.audio = track.url;
            engine_->load(streams_.video, std::max(lastPosition_, engine_->state().position), streams_.audio);
            engine_->setSpeed(speed_);
            refreshSettings();
        }));
    }

    void chooseSubtitles() {
        std::vector<std::string> names{ui::tr("player.subtitles_off")};
        for (const youtube::CaptionTrack& track : streams_.captions) {
            names.push_back(track.name.empty() ? track.language : track.name);
        }
        ui::View* opener = focusedNow();
        const int selected = captionIndex() + 1;
        showModal(opener, ui::makeChoiceDialog(ui::tr("player.subtitles"), names, selected, [this](int index) {
            captionLanguage_ = index > 0 ? streams_.captions[static_cast<std::size_t>(index - 1)].language : "";
            applyCaption();
        }));
    }

    // the chosen language in the current streams' tracks (a restart may get them from another client), -1: none
    int captionIndex() const {
        for (std::size_t i = 0; i < streams_.captions.size() && !captionLanguage_.empty(); ++i) {
            if (streams_.captions[i].language == captionLanguage_) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // captions are added to mpv when chosen (each one is a download)
    void applyCaption() {
        if (!engine_) {
            return;
        }
        const int index = captionIndex();
        if (index < 0) {
            engine_->setSubtitleTrack(0);
            return;
        }
        engine_->addSubtitle(youtube::captionUrl(streams_.captions[static_cast<std::size_t>(index)]), true);
    }

    std::string channelId() const { return !streams_.channelId.empty() ? streams_.channelId : ownerId_; }

    void openChannelHere() {
        const std::string id = channelId();
        AppSession* shared = &session_;
        later([shared, id] {
            if (ui::Application* app = ui::Application::current()) {
                app->pop();  // the player goes: the channel is pushed where it was
                openChannel(*shared, id);
            }
        });
    }

    // ---- controls

    void bindControls() {
        // no hints: the bar and its icons say enough while a video plays. With the focus in the row, ✕ goes to the
        // focused icon and ← / → move along the row.
        bind(platform::Action::Confirm, "", [this] {
            togglePause();
            return true;
        });
        bind(platform::Action::Left, "", [this] { return rowFocused() ? false : (seekBy(-1), true); });
        bind(platform::Action::Right, "", [this] { return rowFocused() ? false : (seekBy(1), true); });
        // with chapters: the previous / next one; else a minute back / on
        bind(platform::Action::L1, "", [this] {
            chapters_.empty() ? seekSeconds(-60) : seekChapter(-1);
            return true;
        });
        bind(platform::Action::R1, "", [this] {
            chapters_.empty() ? seekSeconds(60) : seekChapter(1);
            return true;
        });
        // ↑ / ↓ put the focus on the row; Shorts keep them for the previous / next Short (the row on △ / OPTIONS)
        bind(platform::Action::Up, "", [this] { return upOrDown(-1); });
        bind(platform::Action::Down, "", [this] { return upOrDown(1); });
        // △ and the OPTIONS button: the row, and back to the video
        const auto toggleRow = [this] {
            rowFocused() ? leaveRow() : focusRow();
            return true;
        };
        bind(platform::Action::Options, "", toggleRow);
        bind(platform::Action::Menu, "", toggleRow);
        bind(platform::Action::Back, "", [this] {
            if (nextShown()) {
                cancelNext();
                return true;
            }
            if (rowFocused()) {
                leaveRow();
                return true;
            }
            return false;  // the application pops the player
        });
    }

    bool upOrDown(int direction) {
        if (rowFocused()) {
            showBar();  // one row: the focus stays in it
        } else if (shorts_) {
            nextShort(direction);
        } else {
            focusRow();
        }
        return true;
    }

    // the Short before or after this one in its list, in place of this player
    void nextShort(int direction) {
        const long long target = static_cast<long long>(index_) + direction;
        if (!queue_ || target < 0 || target >= static_cast<long long>(queue_->size())) {
            showBar();
            return;
        }
        AppSession* shared = &session_;
        const Queue queue = queue_;
        const std::size_t index = static_cast<std::size_t>(target);
        later([shared, queue, index] {
            if (ui::Application* app = ui::Application::current()) {
                app->replaceTop(makePlayer(*shared, (*queue)[index].id, queue, index, true));
            }
        });
    }

    void togglePause() {
        if (!engine_) {
            return;
        }
        const bool resuming = engine_->state().paused;
        if (resuming && streamsExpired()) {
            // after a long pause YouTube no longer serves these addresses (403): new ones, from the same second
            PF_LOG_INFO("player: %s: the streams expired during the pause, asked again", videoId_.c_str());
            lastPosition_ = engine_->state().position;
            restart();
            showBar();
            return;
        }
        engine_->setPaused(!engine_->state().paused);
        refreshPause(engine_->state().paused);
        showBar();
    }

    // the addresses stop working at their "expire" time; a minute's margin for the time mpv takes to open them
    bool streamsExpired() const {
        const std::int64_t expiry = youtube::expiresAt(streams_);
        return expiry > 0 && session_.now() >= expiry - 60;
    }

    std::vector<double> chapterStarts() const {
        std::vector<double> starts;
        for (const youtube::Chapter& chapter : chapters_) {
            starts.push_back(chapter.start);
        }
        return starts;
    }

    int chapterAt(double seconds) const { return player::chapterAt(chapterStarts(), seconds); }

    // L1 / R1 with chapters: the previous one (or the start of this one), the next one
    void seekChapter(int direction) {
        if (!engine_ || streams_.live) {
            showBar();
            return;
        }
        const player::Engine::State state = engine_->state();
        const double from = seek_.pending() ? seek_.target() : state.position;
        const double target = player::chapterTarget(chapterStarts(), from, direction);
        if (target >= 0) {
            seek_.press(target - from, now(), from, state.duration);
        }
        showBar();
    }

    void seekBy(int direction) {
        const std::int64_t t = now();
        const bool held = direction == lastDirection_ && t - lastSeekPressMs_ < kHoldGapMs;
        if (!held) {
            holdStartMs_ = t;
        }
        lastDirection_ = direction;
        lastSeekPressMs_ = t;
        seekSeconds(direction * player::seekStep(t - holdStartMs_));
    }

    void seekSeconds(double delta) {
        if (!engine_ || streams_.live) {
            showBar();  // a live stream cannot be sought
            return;
        }
        const player::Engine::State state = engine_->state();
        seek_.press(delta, now(), state.position, state.duration);
        showBar();
        refreshBar(state);
    }

    // ---- loading

    void start(double at) {
        frameShown_ = false;
        const unsigned generation = ++startGeneration_;
        AppSession* shared = &session_;
        const std::string id = videoId_;
        const int height = maxHeight_;
        const std::string invidious = session_.settings().invidious;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::Streams>>(
            [shared, id, height, invidious, cancel] {
                return shared->youtube().streams(id, height, invidious, cancel);
            },
            [this, at, generation](core::Result<youtube::Streams> streams) {
                if (generation == startGeneration_) {  // a later start (another quality) supersedes this one
                    onStreams(std::move(streams), at);
                }
            });
        if (!extrasLoaded_) {
            extrasLoaded_ = true;
            const bool sponsorBlock = session_.settings().sponsorBlock;
            const std::vector<std::string> categories = session_.settings().sponsorCategories;
            const bool signedIn = session_.account().signedIn();
            const bool dislikes = session_.settings().dislikes;
            run<Extras>(
                [shared, id, sponsorBlock, categories, signedIn, dislikes, cancel] {
                    Extras extras;
                    extras.next = shared->youtube().next(id, cancel);
                    if (dislikes) {
                        auto votes = shared->youtube().votes(id, cancel);
                        if (votes.ok()) {
                            extras.votes = votes.value();
                        } else {
                            PF_LOG_INFO("player: votes: %s", votes.error().message.c_str());
                        }
                    }
                    if (signedIn) {
                        auto state = shared->signedIn<youtube::VideoState>(
                            [&](const std::string& token) { return shared->youtube().videoState(id, token, cancel); },
                            cancel);
                        if (state.ok()) {
                            extras.state = std::move(state.value());
                        } else {
                            PF_LOG_WARN("player: rating: %s", state.error().message.c_str());
                        }
                    }
                    if (sponsorBlock && !categories.empty()) {
                        auto segments = shared->youtube().segments(id, categories, cancel);
                        if (segments.ok()) {
                            extras.segments = std::move(segments.value());
                        }
                    }
                    return extras;
                },
                [this](Extras extras) { onExtras(std::move(extras)); });
        }
    }

    void onStreams(core::Result<youtube::Streams> streams, double at) {
        if (!shown_) {
            return;  // hidden meanwhile: onShow starts again
        }
        if (engine_ && engine_->state().position > 0) {
            at = engine_->state().position;  // it kept playing while the new streams were asked for
        }
        if (!streams.ok()) {
            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), streams.error().message.c_str());
            showError(streams.error());
            return;
        }
        streams_ = std::move(streams.value());
        if (history_) {
            history_->setLive(streams_.live);
        }
        setupPreview();
        PF_LOG_INFO("player: %s from %s, %dp%s", videoId_.c_str(), streams_.client.c_str(), streams_.height,
                    streams_.live ? " live" : "");
        if (!streams_.title.empty()) {
            title_->setText(core::withoutEmoji(streams_.title));
        }
        showTimes(!streams_.live);
        refreshRow();
        for (const youtube::AudioTrack& track : streams_.audioTracks) {
            if (!audioTrack_.empty() && track.id == audioTrack_) {  // the language chosen before a restart
                streams_.audioTrack = track.id;
                streams_.audio = track.url;
            }
        }
        // a new engine for each start: the streams answer only to the User-Agent of the client that got them
        logDrops();
        engine_.reset();
        player::EngineOptions options = engineOptions(session_, streams_.userAgent);
        options.loop = shorts_;  // a Short starts again at its end, as on YouTube
        engine_ = player::Engine::create(options, [] {});
        if (!engine_) {
            showError(core::makeError(core::ErrorKind::BadResponse, "the player could not start"));
            return;
        }
        engine_->setSpeed(speed_);
        engine_->setVolume(session_.settings().volume);
        if (!streams_.hls.empty()) {
            engine_->loadPlaylist(streams_.hls, streams_.live ? 0 : at);
        } else {
            engine_->load(streams_.video, streams_.live ? 0 : at, streams_.audio);
        }
        if (session_.synchronous) {
            showFirstPictureNow();
        }
    }

    void onExtras(Extras extras) {
        if (!extras.next.ok()) {
            extrasLoaded_ = false;  // asked again with the next start (Réessayer)
        }
        segments_ = std::move(extras.segments);
        votes_ = extras.votes;
        refreshActions();
        skipped_.assign(segments_.size(), false);
        if (extras.state) {
            // read before a change the user made meanwhile: that change stays
            const youtube::VideoState confirmed = state_ ? *state_ : youtube::VideoState{};
            state_ = std::move(extras.state);
            if (userRated_) {
                state_->rating = confirmed.rating;
            }
            if (userSubscribed_) {
                state_->subscribed = confirmed.subscribed;
            }
            refreshActions();
        }
        if (extras.next.ok()) {
            applyPage(extras.next.value());
        } else {
            pageFailed(extras.next.error());
        }
    }

    // The video page: chapters, the channel, the comments' first page, the live chat, and what plays next.
    void applyPage(const youtube::NextReply& next) {
        pageRetryAtMs_ = 0;
        chapters_ = next.chapters;
        progress_->setChapters(chapterStarts());
        ownerId_ = next.owner.id;
        commentsContinuation_ = next.commentsContinuation;
        commentsChecked_ = true;
        liveChat_ = next.liveChat;
        channelName_ = core::withoutEmoji(next.owner.name);
        channelAvatar_ = next.owner.avatar;
        refreshRow();
        if (!nextVideo_) {  // (else the playlist's next video)
            if (const youtube::Video* video =
                    nextToPlay(next.related, session_.settings().shorts, session_.played())) {
                nextVideo_ = *video;
            } else {
                PF_LOG_INFO("player: %s has no video to play next (%zu related)", videoId_.c_str(),
                            next.related.size());
            }
        }
    }

    // The video page did not come: without it nothing plays next and the player would close at the video's end.
    // Asked again by itself, kPageTries times in all, further and further apart.
    void pageFailed(const core::Error& error) {
        ++pageTries_;
        PF_LOG_WARN("player: the video page of %s did not come (%s, try %d of %d)", videoId_.c_str(),
                    core::errorKindName(error.kind), pageTries_, kPageTries);
        pageRetryAtMs_ = pageTries_ < kPageTries ? now() + kPageRetryMs * pageTries_ : 0;
    }

    void askPageAgain() {
        pageRetryAtMs_ = 0;
        AppSession* shared = &session_;
        const std::string id = videoId_;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::NextReply>>(
            [shared, id, cancel] { return shared->youtube().next(id, cancel); },
            [this](core::Result<youtube::NextReply> next) {
                if (next.ok()) {
                    applyPage(next.value());
                } else {
                    pageFailed(next.error());
                }
            });
    }

    // Test mode: the first picture before the next frame, then paused on it (deterministic screenshots). Up to a
    // minute: the README's screenshots play YouTube's own streams.
    void showFirstPictureNow() {
        bool shown = false;
        for (int i = 0; i < 12000 && !shown && engine_; ++i) {
            for (player::EngineEvent event : engine_->drainEvents()) {
                onEngineEvent(event);
                shown = shown || event == player::EngineEvent::Error;
            }
            if (engine_ && engine_->renderIfDue(frame_)) {
                shown = true;
                frameShown_ = true;  // paused next: no later picture would tell the frame loop
                onEngineEvent(player::EngineEvent::FirstFrame);
            }
            if (!shown) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        if (engine_) {
            engine_->setPaused(true);
            refreshPause(true);
        }
    }

    void onEngineEvent(player::EngineEvent event) {
        switch (event) {
        case player::EngineEvent::FileLoaded:
            if (!captionLanguage_.empty()) {
                applyCaption();
            }
            break;
        case player::EngineEvent::FirstFrame:
            PF_LOG_INFO("player: %s", engine_->describe().c_str());
            if (resumedFrom_ > 0 && !resumeTold_) {
                resumeTold_ = true;
                if (ui::Application* app = ui::Application::current()) {
                    app->toast(ui::tr("player.resumed", {clockText(resumedFrom_)}));
                }
            }
            break;
        case player::EngineEvent::Seeked:
            break;
        case player::EngineEvent::EndOfFile:
            // a stream YouTube cut on the way is no end: asked again and played on from there
            if (!streams_.live && !shorts_ && streamCutShort(lastPosition_, lastDuration_, lastCutAt_)) {
                PF_LOG_WARN("player: %s stopped at %s of %s: its stream was cut, starting again there",
                            videoId_.c_str(), clockText(lastPosition_).c_str(), clockText(lastDuration_).c_str());
                lastCutAt_ = lastPosition_;
                restart();
            } else {
                ended();
            }
            break;
        case player::EngineEvent::Error:
            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), engine_->state().error.c_str());
            showError(core::makeError(core::ErrorKind::Network, engine_->state().error));  // (the streams were checked)
            break;
        }
    }

    // ---- per frame

    void tick(bool freshPicture) {
        const std::int64_t t = now();
        const player::Engine::State state = engine_->state();
        if (pageRetryAtMs_ > 0 && t >= pageRetryAtMs_) {
            askPageAgain();
        }
        if (frameShown_) {  // before the first picture mpv has no position yet: seeks would be lost
            if (const auto target = seek_.due(t)) {
                engine_->seekTo(*target);
            }
        }
        if (state.buffering != spinner_->visible || (!frameShown_ && !spinner_->visible)) {
            spinner_->visible = state.buffering || !frameShown_;
            markDirty();
        }
        frameShown_ = frameShown_ || freshPicture;
        if (frameShown_ && state.position > 0) {
            lastPosition_ = state.position;
            lastDuration_ = state.duration;
            if (history_) {
                history_->update(state.position, state.paused);
            }
            if (t - progressSavedMs_ > kProgressSaveMs) {  // kept on the console now and then while it plays
                progressSavedMs_ = t;
                recordProgress(true);
            }
        }
        refreshPause(state.paused);
        if (bar_->visible) {
            refreshBar(state);
            if (t > barUntil_ && !state.paused && !seek_.pending() && !rowFocused()) {
                setBarShown(false);  // not while the row has the focus
            }
        }
        if (!bar_->visible) {  // no preview without its bar
            preview_->update(false, 0, 0, 0, {});
        }
        if (!seek_.pending() && frameShown_) {  // the pictures around the playing position, ready for a seek
            preview_->follow(state.position);
        }
        if (frameShown_) {
            skipSegment(state);
            updateNext(state);
        }
    }

    void skipSegment(const player::Engine::State& state) {
        if (seek_.pending() || streams_.live) {
            return;
        }
        const auto index = youtube::segmentToSkip(state.position, segments_, skipped_);
        if (!index) {
            return;
        }
        const youtube::Segment& segment = segments_[*index];
        skipped_[*index] = true;
        engine_->seekTo(segment.end);
        PF_LOG_INFO("player: %s segment skipped %.1f-%.1f", segment.category.c_str(), segment.start, segment.end);
        if (ui::Application* app = ui::Application::current()) {
            app->toast(ui::tr("player.skipped", {ui::tr("settings.sponsor." + segment.category),
                                                 clockText(segment.end - segment.start)}));
        }
    }

    // ---- "À suivre"

    bool nextShown() const { return next_->visible; }

    // what plays after this video: the queue's first (the user asked for it), else "À suivre" when autoplay is on
    std::optional<youtube::Video> upcoming() const {
        if (shorts_ || streams_.live) {
            return std::nullopt;
        }
        if (!session_.watchQueue().empty()) {
            return session_.watchQueue().front();
        }
        if (nextVideo_ && session_.settings().autoplay) {
            return nextVideo_;
        }
        return std::nullopt;
    }

    void updateNext(const player::Engine::State& state) {
        const auto upcomingVideo = upcoming();
        const bool queued = !session_.watchQueue().empty();
        if (!upcomingVideo || nextCancelled_ || (!queued && state.duration < kMinDurationForNext)) {
            return;
        }
        const double left = state.duration - state.position;
        if (left > kNextAheadSeconds) {
            if (next_->visible) {  // sought back
                next_->visible = false;
                markDirty();
            }
            return;
        }
        if (!next_->visible || nextShownId_ != upcomingVideo->id) {
            nextShownId_ = upcomingVideo->id;
            nextImage_->setUrl(upcomingVideo->thumbnail);
            nextTitle_->setText(core::withoutEmoji(upcomingVideo->title));
            next_->visible = true;
            markDirty();
        }
        const std::string text = ui::tr("player.next_in", {std::to_string(static_cast<long>(left + 0.999))});
        if (text != nextCountdown_->text()) {
            nextCountdown_->setText(text);
        }
    }

    void cancelNext() {
        nextCancelled_ = true;
        next_->visible = false;
        markDirty();
    }

    // where the video is, for the cards' bar and the next time it is opened (not a live)
    void recordProgress(bool save) {
        if (streams_.live || lastDuration_ <= 0 || !frameShown_) {
            return;
        }
        session_.progress().set(videoId_, static_cast<std::int64_t>(lastPosition_),
                                static_cast<std::int64_t>(lastDuration_), session_.now());
        if (save) {
            session_.progress().save();
        }
    }

    void ended() {
        lastPosition_ = lastDuration_;  // watched: its bar full, and it starts from the beginning next time
        recordProgress(true);
        // the queue first: what the user asked for
        if (!nextCancelled_ && !streams_.live && !session_.watchQueue().empty()) {
            const youtube::Video queued = session_.watchQueue().front();
            session_.watchQueue().erase(session_.watchQueue().begin());
            AppSession* shared = &session_;
            later([shared, id = queued.id] {
                if (ui::Application* app = ui::Application::current()) {
                    app->replaceTop(makePlayer(*shared, id));
                }
            });
            return;
        }
        // as for the card: not after a very short video (it would start with no countdown to cancel it)
        if (nextVideo_ && !nextCancelled_ && !streams_.live && session_.settings().autoplay &&
            lastDuration_ >= kMinDurationForNext) {
            AppSession* shared = &session_;
            const std::string id = nextVideo_->id;
            Queue queue = queueNext_ > 0 ? queue_ : nullptr;  // still in the playlist, or on to related videos
            const std::size_t index = queueNext_;
            later([shared, id, queue, index] {
                if (ui::Application* app = ui::Application::current()) {
                    app->replaceTop(makePlayer(*shared, id, queue, index));
                }
            });
            return;
        }
        PF_LOG_INFO("player: %s ended and nothing plays next (%s)", videoId_.c_str(),
                    nextCancelled_                         ? "cancelled"
                    : streams_.live                        ? "a live"
                    : !session_.settings().autoplay        ? "autoplay off"
                    : !nextVideo_                          ? "no next video"
                                                           : "too short");
        leave();
    }

    // ---- leaving and errors

    // runs on the next frame: reached from the middle of renderVideo and of key handlers
    void leave() {
        later([] {
            if (ui::Application* app = ui::Application::current()) {
                app->pop();
            }
        });
    }

    // at the last position the video had (an engine that failed reports none any more)
    void restart() { start(lastPosition_); }

    void release() {
        if (ui::Application* app = ui::Application::current(); app != nullptr && app->videoSource() == this) {
            app->setVideoSource(nullptr);
        }
        if (engine_) {
            recordProgress(true);
        }
        if (history_) {
            history_->flush();
        }
        logDrops();
        engine_.reset();
    }

    // frames the decoder or the output could not keep up with, to judge the console on a resolution
    void logDrops() {
        if (engine_) {
            const player::Engine::Stats stats = engine_->stats();
            PF_LOG_INFO("player: frames dropped %ld by the decoder, %ld at output; last conversion %.1f ms",
                        stats.decoderDrops, stats.outputDrops, stats.renderMs);
        }
    }

    void showError(const core::Error& error) {
        ui::Application* app = ui::Application::current();
        if (app == nullptr || errorShown_) {
            return;
        }
        errorShown_ = true;
        spinner_->visible = false;
        // YouTube's own words when it refused the video, else what went wrong on the way
        const bool refused = error.kind == core::ErrorKind::Server && error.httpStatus == 0;
        // The bot check flags the connection, not the video: YouTube's words ("sign in to confirm…") would send
        // the user to a sign-in that changes nothing, so the dialog says what it is and that it passes
        const bool blocked = error.kind == core::ErrorKind::Blocked;
        const std::string reason = blocked                 ? ui::tr("player.blocked.text")
                                   : !refused              ? errorText(error)
                                   : error.message.empty() ? ui::tr("player.refused_plain")
                                                           : ui::tr("player.refused", {error.message});
        auto dialog = ui::makeDialog(ui::tr(blocked ? "player.blocked" : "player.error"), reason,
                                     {{ui::tr("player.retry"),
                                       [this] {
                                           errorShown_ = false;
                                           restart();
                                       }},
                                      {ui::tr("player.back"), [this] { leave(); }}});
        // ◯ is Retour: dismissing the dialog would leave a player with nothing to play
        dialog->bind(platform::Action::Back, ui::tr("player.back"), [this] {
            leave();
            return true;
        });
        app->showModal(std::move(dialog));
    }

    std::string videoId_;
    int maxHeight_;
    youtube::Streams streams_;
    std::string ownerId_;
    std::vector<youtube::Segment> segments_;
    std::vector<bool> skipped_;
    std::optional<youtube::Video> nextVideo_;
    std::string nextShownId_;  // the video the "À suivre" card shows
    Queue queue_;                // the playlist played, when there is one
    std::size_t queueNext_ = 0;  // where nextVideo_ is in it (0: not from it)
    std::unique_ptr<player::Engine> engine_;
    player::VideoFrame frame_;
    player::SeekAccumulator seek_;

    ui::Spinner* spinner_ = nullptr;
    ui::Box* next_ = nullptr;
    ui::ImageView* nextImage_ = nullptr;
    ui::Label* nextTitle_ = nullptr;
    ui::Label* nextCountdown_ = nullptr;
    ui::Box* row_ = nullptr;
    ui::IconButton* pauseIcon_ = nullptr;
    ui::IconButton* fromStartIcon_ = nullptr;
    ui::IconButton* likeIcon_ = nullptr;
    ui::IconButton* dislikeIcon_ = nullptr;
    ui::IconButton* chatIcon_ = nullptr;
    ui::IconButton* commentsIcon_ = nullptr;
    ui::IconButton* saveIcon_ = nullptr;
    ui::IconButton* subscribeIcon_ = nullptr;
    ui::IconButton* queueIcon_ = nullptr;
    ui::IconButton* channelIcon_ = nullptr;
    ui::IconButton* captionsIcon_ = nullptr;
    ui::IconButton* settingsIcon_ = nullptr;
    bool pausedShown_ = false;  // the icon shows Lecture (the video is paused)
    std::optional<youtube::VideoState> state_;
    std::string commentsContinuation_;
    ui::Box* bar_ = nullptr;
    BarShade* shade_ = nullptr;
    ui::Label* title_ = nullptr;
    ChannelInitial* avatar_ = nullptr;
    ui::Label* channel_ = nullptr;
    std::string channelName_;
    std::string channelAvatar_;  // the channel's picture (the video page's owner)
    SegmentBar* progress_ = nullptr;
    TrickplayPreview* preview_ = nullptr;
    std::unique_ptr<WatchHistory> history_;  // signed in, with the setting on
    std::string liveChat_;                    // a live's chat: its first page (from the video page)
    std::optional<youtube::Votes> votes_;
    std::vector<youtube::Chapter> chapters_;
    ui::Label* live_ = nullptr;
    ui::Label* position_ = nullptr;  // "2:41"
    ui::Label* rest_ = nullptr;      // "/ 10:23 · <chapter>"
    ui::Label* endsAt_ = nullptr;    // "Se termine à 21:47"
    std::int64_t endsMinute_ = -1;   // the minute endsAt_ shows (-1: none)
    std::shared_ptr<PlayerSettings*> settings_ = std::make_shared<PlayerSettings*>(nullptr);  // the open panel, or null

    double speed_ = 1;
    std::string captionLanguage_;  // the captions chosen, "" none
    std::string audioTrack_;       // the audio language chosen, "" the original
    double lastPosition_ = 0;
    bool shorts_ = false;
    std::size_t index_ = 0;
    double resumedFrom_ = 0;  // where it started again (0: from the start)
    bool resumeTold_ = false;
    std::int64_t progressSavedMs_ = 0;
    static constexpr std::int64_t kProgressSaveMs = 30000;
    double lastDuration_ = 0;
    double lastCutAt_ = -1;            // where the stream was last cut (streamCutShort)
    int pageTries_ = 0;                // the video page: failed tries
    std::int64_t pageRetryAtMs_ = 0;   // when it is asked again (0: not waiting)
    unsigned startGeneration_ = 0;
    bool started_ = false;
    bool shown_ = false;
    bool rating_ = false;       // a rating is being sent
    bool subscribing_ = false;  // a subscription change is being sent
    bool commentsChecked_ = false;
    bool commentsFetching_ = false;
    bool userRated_ = false;       // the rating shown is one the user set (newer than any state read before)
    bool userSubscribed_ = false;  // the same for the subscription
    bool extrasLoaded_ = false;
    bool frameShown_ = false;
    bool errorShown_ = false;
    bool nextCancelled_ = false;
    std::int64_t barUntil_ = 0;
    std::int64_t lastSeekPressMs_ = 0;
    std::int64_t holdStartMs_ = 0;
    int lastDirection_ = 0;
};

}  // namespace

std::unique_ptr<ui::Screen> makePlayer(AppSession& session, const std::string& videoId, Queue queue,
                                       std::size_t index, bool shorts) {
    return std::make_unique<PlayerScreen>(session, videoId, std::move(queue), index, shorts);
}

void addToQueue(AppSession& session, const youtube::Video& video) {
    auto& queue = session.watchQueue();
    if (std::none_of(queue.begin(), queue.end(), [&](const youtube::Video& v) { return v.id == video.id; })) {
        queue.push_back(video);
    }
    if (ui::Application* app = ui::Application::current()) {
        app->toast(ui::tr("queue.added", {std::to_string(queue.size())}));
    }
}

void openShorts(AppSession& session, Queue queue, std::size_t index) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && queue && index < queue->size()) {
        const std::string id = (*queue)[index].id;
        app->push(makePlayer(session, id, std::move(queue), index, true));
    }
}

void openQueue(AppSession& session, Queue queue, std::size_t index) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && queue && index < queue->size()) {
        const std::string id = (*queue)[index].id;
        app->push(makePlayer(session, id, std::move(queue), index));
    }
}

void openVideo(AppSession& session, const std::string& videoId) {
    if (ui::Application* app = ui::Application::current(); app != nullptr && !videoId.empty()) {
        app->push(makePlayer(session, videoId));
    }
}

}  // namespace playtube::app
