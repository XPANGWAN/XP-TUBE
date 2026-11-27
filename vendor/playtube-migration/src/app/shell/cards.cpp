#include "app/shell/cards.hpp"

#include <nanovg.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "app/shell/chrome.hpp"
#include "core/utf.hpp"
#include "ui/animation.hpp"
#include "ui/context.hpp"
#include "ui/draw.hpp"
#include "ui/halo.hpp"
#include "ui/i18n.hpp"

namespace playtube::app {

namespace {

constexpr float kImageWidth = 400;
constexpr float kImageHeight = 225;
constexpr float kTitleHeight = 64;  // two lines
constexpr float kGlowFeather = 50;
constexpr float kGlowStrength = 0.30f;  // of the accent under the picture (0.45 looked too strong on the console)
constexpr float kRingWidth = 3;
constexpr float kRingGap = 4;
const ui::Color kLiveRed{0xCC, 0x00, 0x00, 0xFF};

// The glow image of a picture size for the current accent, made once per canvas (a live box gradient costs
// ~200 cycles per pixel on the PS4).
int glowSprite(NVGcontext* vg, int width, int height) {
    struct Entry {
        NVGcontext* vg;
        int width;
        int height;
        unsigned generation;
        int image;
    };
    static std::vector<Entry> cache;
    for (Entry& entry : cache) {
        if (entry.vg == vg && entry.width == width && entry.height == height) {
            if (entry.generation == ui::themeGeneration()) {
                return entry.image;
            }
            nvgDeleteImage(vg, entry.image);
            entry = cache.back();
            cache.pop_back();
            break;
        }
    }
    const std::vector<std::uint8_t> pixels =
        ui::glowImage(width, height, ui::theme().radius, kGlowFeather, ui::theme().accent, kGlowStrength);
    const int image = nvgCreateImageRGBA(vg, width + 2 * static_cast<int>(kGlowFeather),
                                         height + 2 * static_cast<int>(kGlowFeather), NVG_IMAGE_PREMULTIPLIED,
                                         pixels.data());
    cache.push_back({vg, width, height, ui::themeGeneration(), image});
    return image;
}

}  // namespace

ui::Size videoCardSize() {
    const float small = ui::theme().fontSmall * 1.3f;
    return {kImageWidth, kImageHeight + 12 + kTitleHeight + 4 + small + 2 + small};
}

std::string videoMeta(const youtube::Video& video) {
    if (video.viewsText.empty() || video.publishedText.empty()) {
        return video.viewsText + video.publishedText;
    }
    return video.viewsText + " \xC2\xB7 " + video.publishedText;
}

namespace {
ProgressLookup& progressLookup() {
    static ProgressLookup lookup;
    return lookup;
}
}  // namespace

void setProgressLookup(ProgressLookup lookup) { progressLookup() = std::move(lookup); }

namespace {
BrandingSource& brandingSource() {
    static BrandingSource source;
    return source;
}
}  // namespace

void setBrandingSource(BrandingSource source) { brandingSource() = std::move(source); }

namespace {
ShortsOpener& shortsOpener() {
    static ShortsOpener opener;
    return opener;
}
}  // namespace

void setShortsOpener(ShortsOpener opener) { shortsOpener() = std::move(opener); }

namespace {
QueueAdder& queueAdder() {
    static QueueAdder adder;
    return adder;
}
}  // namespace

void setQueueAdder(QueueAdder adder) { queueAdder() = std::move(adder); }

WatchedBar::WatchedBar() {
    width = ui::SizeSpec::fill();
    height = ui::SizeSpec::fixed(5);
    visible = false;
}

void WatchedBar::set(int percent) {
    percent_ = percent;
    visible = percent > 0;
    markDirty();
}

void WatchedBar::draw(NVGcontext* vg) const {
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, frame.width, frame.height);
    nvgFillColor(vg, nvgRGBA(0xFF, 0xFF, 0xFF, 0x50));
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, frame.width * static_cast<float>(percent_) / 100.0f, frame.height);
    nvgFillColor(vg, ui::nvgColor(ui::theme().accent));
    nvgFill(vg);
}

VideoCard::VideoCard(bool channelButton) : ui::Box(ui::Axis::Column) {
    focusable = true;
    drawsOwnFocus = true;
    auto* picture = add<ui::Stack>();
    picture->width = ui::SizeSpec::fixed(kImageWidth);
    picture->height = ui::SizeSpec::fixed(kImageHeight);
    picture->horizontal = ui::CrossAlign::End;
    picture->vertical = ui::CrossAlign::End;
    picture->clip = true;
    picture->cornerRadius = ui::theme().radius;
    image_ = picture->add<ui::ImageView>();
    image_->width = ui::SizeSpec::fill();
    image_->height = ui::SizeSpec::fill();
    watched_ = picture->add<WatchedBar>();
    badge_ = picture->add<ui::Label>("", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::SemiBold});
    badge_->padding = {2, 8, 2, 8};
    badge_->margin = {0, 10, 10, 0};
    badge_->cornerRadius = 6;
    title_ = add<ui::Label>("", ui::FontStyle{ui::theme().fontBody, ui::FontWeight::SemiBold});
    title_->width = ui::SizeSpec::fill();
    title_->height = ui::SizeSpec::fixed(kTitleHeight);
    title_->maxLines = 2;
    title_->margin.top = 12;
    channel_ = add<ui::Label>("", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Regular});
    channel_->width = ui::SizeSpec::fill();
    channel_->margin.top = 4;
    channel_->color = ui::theme().textSecondary;
    meta_ = add<ui::Label>("", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Regular});
    meta_->width = ui::SizeSpec::fill();
    meta_->margin.top = 2;
    meta_->color = ui::theme().textSecondary;
    bind(platform::Action::Confirm, ui::tr("ui.select"), [this] {
        if (onPress) {
            onPress();
        }
        return true;
    });
    bind(
        platform::Action::Options, ui::tr("queue.add"),
        [this] {
            if (onQueue && queueable_) {
                onQueue();
            }
            return true;
        },
        [this] { return queueable_; });
    if (channelButton) {
        bind(platform::Action::Context, ui::tr("video.channel"), [this] {
            if (onChannel) {
                onChannel();
            }
            return true;
        });
    }
}

VideoCard::~VideoCard() {
    *alive_ = false;
    if (ui::context().animator != nullptr) {
        ui::context().animator->cancel(this);
    }
}

void VideoCard::show(const youtube::Video& video) {
    image_->setUrl(video.thumbnail);
    title_->setText(core::withoutEmoji(video.title));
    // a playlist: its owner (or "Playlist"), when it was updated, its size in the corner
    const std::string channel = video.playlist && video.channelName.empty() ? ui::tr("playlist.label")
                                                                            : video.channelName;
    channel_->setText(core::withoutEmoji(channel));
    channel_->visible = !channel.empty();
    meta_->setText(videoMeta(video));
    if (video.playlist) {
        badge_->setText(video.videoCountText);
        badge_->background = ui::Color{0x00, 0x00, 0x00, 0xC8};
    } else if (video.live) {
        badge_->setText(ui::tr("video.live"));
        badge_->background = kLiveRed;
    } else if (video.isShort) {
        badge_->setText(ui::tr("video.short"));
        badge_->background = kLiveRed;
    } else if (video.isChannel) {
        badge_->setText(ui::tr("video.channel"));
        badge_->background = ui::Color{0x00, 0x00, 0x00, 0xC8};
    } else {
        badge_->setText(video.durationText);
        badge_->background = ui::Color{0x00, 0x00, 0x00, 0xC8};
    }
    badge_->visible = !badge_->text().empty();
    queueable_ = !video.playlist && !video.isChannel && !video.live;
    const bool watchable = !video.playlist && !video.live && !video.isChannel;
    videoId_ = watchable ? video.id : std::string();
    youtubePercent_ = watchable ? video.percentWatched : -1;
    refreshWatched();
    if (watchable && !video.isShort) {
        askBranding(video);
    }
    markDirty();
}

// DeArrow's title and thumbnail in place of YouTube's, when it has them; YouTube's picture again if DeArrow's cannot
// be shown. The answer is dropped when the card shows another video by then (lists reuse their cards).
void VideoCard::askBranding(const youtube::Video& video) {
    if (!brandingSource()) {
        return;
    }
    std::weak_ptr<bool> alive = alive_;
    const std::string id = video.id;
    const std::string original = video.thumbnail;
    brandingSource()(id, [this, alive, id, original](const std::string& title, const std::string& thumbnail) {
        const auto flag = alive.lock();
        if (!flag || !*flag || videoId_ != id) {
            return;
        }
        if (!title.empty()) {
            title_->setText(core::withoutEmoji(title));
        }
        if (!thumbnail.empty()) {
            image_->setUrl(thumbnail, original);
        }
        markDirty();
    });
}

// the larger of YouTube's progress and Playtube's own (which changes while the card stays: a video watched, back)
void VideoCard::refreshWatched() const {
    int percent = youtubePercent_;
    if (!videoId_.empty() && progressLookup()) {
        percent = std::max(percent, progressLookup()(videoId_));
    }
    if (percent != watchedPercent_) {
        watchedPercent_ = percent;
        watched_->set(percent);
    }
}

void VideoCard::onFocusChanged(bool focused) {
    if (ui::context().animator != nullptr) {
        ui::context().animator->animate(&focus_, focused ? 1.0f : 0.0f, ui::theme().focusMs, ui::Easing::Smooth,
                                        this);
    } else {
        focus_ = focused ? 1.0f : 0.0f;
    }
}

void VideoCard::draw(NVGcontext* vg) const {
    refreshWatched();
    const float p = focus_;
    if (p <= 0.001f) {
        ui::Box::draw(vg);
        return;
    }
    const ui::Rect picture = children().front()->frame;
    // the glow: accent at 30 %, 50 px soft, 18 px lower, under the grown picture
    const float grow = 0.07f * p;
    const float w = picture.width * (1 + grow);
    const float h = picture.height * (1 + grow);
    const float x = picture.x - (w - picture.width) / 2;
    const float y = picture.y - (h - picture.height) / 2 - 6 * p + 18 * p;
    const int image = glowSprite(vg, static_cast<int>(picture.width), static_cast<int>(picture.height));
    if (image > 0) {
        const float margin = kGlowFeather * (1 + grow);
        const NVGpaint paint = nvgImagePattern(vg, x - margin, y - margin, w + 2 * margin, h + 2 * margin, 0, image, p);
        // the glow around the grown picture only (the opaque picture hides the rest), in four convex pieces
        const float left = x - margin;
        const float top = y - margin;
        const float right = x + w + margin;
        const float bottom = y + h + margin;
        const float inset = ui::theme().radius * (1 + grow);
        const float holeLeft = x + inset;
        const float holeRight = x + w - inset;
        const float holeTop = y - 18 * p + inset;
        const float holeBottom = y - 18 * p + h - inset;
        const float rects[4][4] = {{left, top, right - left, holeTop - top},
                                   {left, holeTop, holeLeft - left, holeBottom - holeTop},
                                   {holeRight, holeTop, right - holeRight, holeBottom - holeTop},
                                   {left, holeBottom, right - left, bottom - holeBottom}};
        nvgSave(vg);
        nvgShapeAntiAlias(vg, 0);
        for (const auto& r : rects) {
            nvgBeginPath(vg);
            nvgRect(vg, r[0], r[1], r[2], r[3]);
            nvgFillPaint(vg, paint);
            nvgFill(vg);
        }
        nvgRestore(vg);
    }
    const float cx = picture.x + picture.width / 2;
    const float cy = picture.y + picture.height / 2;
    nvgSave(vg);
    nvgTranslate(vg, cx, cy - 6 * p);
    nvgScale(vg, 1 + grow, 1 + grow);
    nvgTranslate(vg, -cx, -cy);
    ui::Box::draw(vg);
    const ui::Color accent = ui::theme().accent;
    const float gap = kRingGap + kRingWidth / 2;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, picture.x - gap, picture.y - gap, picture.width + 2 * gap, picture.height + 2 * gap,
                   ui::theme().radius + gap);
    nvgStrokeColor(vg, nvgRGBA(accent.r, accent.g, accent.b, static_cast<unsigned char>(255 * p)));
    nvgStrokeWidth(vg, kRingWidth);
    nvgStroke(vg);
    nvgRestore(vg);
}

VideosSource::VideosSource(std::vector<youtube::Video> videos, Action onPress, Action onChannel)
    : videos_(std::move(videos)), onPress_(std::move(onPress)), onChannel_(std::move(onChannel)) {}

std::unique_ptr<ui::View> VideosSource::create() { return std::make_unique<VideoCard>(onChannel_ != nullptr); }

void VideosSource::bind(ui::View& view, int index) {
    auto& card = static_cast<VideoCard&>(view);
    const youtube::Video& video = videos_[static_cast<std::size_t>(index)];
    card.show(video);
    card.onPress = [this, index] {
        const youtube::Video& pressed = videos_[static_cast<std::size_t>(index)];
        if (pressed.isShort && shortsOpener()) {  // the Shorts player, with this list's other Shorts
            std::vector<youtube::Video> shorts;
            std::size_t at = 0;
            for (const youtube::Video& video : videos_) {
                if (video.isShort) {
                    at = video.id == pressed.id ? shorts.size() : at;
                    shorts.push_back(video);
                }
            }
            shortsOpener()(std::move(shorts), at);
            return;
        }
        if (onPress_) {
            onPress_(pressed);
        }
    };
    card.onQueue = [this, index] {
        const youtube::Video& video = videos_[static_cast<std::size_t>(index)];
        if (queueAdder() && !video.playlist && !video.isChannel && !video.live) {
            queueAdder()(video);
        }
    };
    card.onChannel = onChannel_ ? std::function<void()>([this, index] {
        onChannel_(videos_[static_cast<std::size_t>(index)]);
    })
                                : std::function<void()>();
}

ui::HList* addVideoRow(ui::Box& column, const std::string& title) {
    auto* section = column.add<ui::Box>(ui::Axis::Column);
    section->width = ui::SizeSpec::fill();
    section->gap = 18;
    section->add<ui::Label>(title, headingText());
    auto* row = section->add<ui::HList>(videoCardSize(), ui::theme().cardGap);
    // room around the cards for the grown focused card and its glow, taken from the margins
    row->padding = ui::Insets::all(56);
    row->margin = ui::Insets::all(-56);
    return row;
}

ui::Grid* addVideoGrid(ui::Box& column) {
    auto* grid = column.add<ui::Grid>(videoCardSize(), ui::theme().cardGap, 4);
    grid->width = ui::SizeSpec::fill();
    grid->height = ui::SizeSpec::fill();
    grid->padding = ui::Insets::all(56);
    grid->margin = ui::Insets::all(-56);
    // cards scroll under nothing: cut at the gap above the grid (the focused card's glow still fits)
    grid->margin.top = -column.gap;
    grid->padding.top = 56 - column.gap;
    grid->clip = true;
    return grid;
}

}  // namespace playtube::app
