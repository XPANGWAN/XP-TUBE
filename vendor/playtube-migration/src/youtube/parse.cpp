#include "youtube/parse.hpp"

#include <cstring>
#include <set>

#include "core/json.hpp"

namespace playtube::youtube {

namespace {

namespace json = core::json;
using V = yyjson_val*;

core::Error badReply(const char* what) { return core::makeError(core::ErrorKind::BadResponse, what); }

V at(V object, const char* key) { return json::field(object, key); }

V item(V array, std::size_t index) {
    return yyjson_is_arr(array) && index < yyjson_arr_size(array) ? yyjson_arr_get(array, index) : nullptr;
}

std::size_t count(V array) { return yyjson_is_arr(array) ? yyjson_arr_size(array) : 0; }

// A text of any of YouTube's shapes: a plain string, {"simpleText"}, {"runs":[{"text"}]} or {"content"}.
std::string readText(V value) {
    if (value == nullptr) {
        return {};
    }
    if (yyjson_is_str(value)) {
        return json::textOf(value);
    }
    if (V simple = at(value, "simpleText")) {
        return json::textOf(simple);
    }
    if (V runs = at(value, "runs")) {
        std::string out;
        for (std::size_t i = 0; i < count(runs) && out.size() < json::kMaxString; ++i) {
            out += json::string(item(runs, i), "text");
        }
        return out;
    }
    return json::string(value, "content");
}

std::string https(std::string url) {
    if (url.rfind("//", 0) == 0) {
        return "https:" + url;
    }
    return url.rfind("https://", 0) == 0 ? url : std::string();
}

// The largest picture at most `maxWidth` wide (else the smallest), from [{url, width}].
std::string bestImage(V list, int maxWidth = 720) {
    std::string best;
    std::int64_t bestWidth = -1;
    std::string smallest;
    std::int64_t smallestWidth = -1;
    for (std::size_t i = 0; i < count(list); ++i) {
        V image = item(list, i);
        const std::string url = https(json::string(image, "url"));
        const std::int64_t width = json::integer(image, "width", 0);
        if (url.empty()) {
            continue;
        }
        if (width <= maxWidth && width > bestWidth) {
            best = url;
            bestWidth = width;
        }
        if (smallestWidth < 0 || width < smallestWidth) {
            smallest = url;
            smallestWidth = width;
        }
    }
    return !best.empty() ? best : smallest;
}

// Calls `visit` with the value of every field named `key`, anywhere below `node` (fields named in `skip` are not
// entered). Depth-bounded.
template <typename F>
void forEach(V node, const char* key, F&& visit, const std::set<std::string>& skip = {}, int depth = 0) {
    if (node == nullptr || depth > kMaxDepth) {
        return;
    }
    if (yyjson_is_obj(node)) {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(node, &iter);
        while (V k = yyjson_obj_iter_next(&iter)) {
            V value = yyjson_obj_iter_get_val(k);
            const char* name = yyjson_get_str(k);
            if (name != nullptr && std::strcmp(name, key) == 0) {
                visit(value);
            }
            if (name == nullptr || skip.count(name) == 0) {
                forEach(value, key, visit, skip, depth + 1);
            }
        }
    } else if (yyjson_is_arr(node)) {
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(node, &iter);
        while (V value = yyjson_arr_iter_next(&iter)) {
            forEach(value, key, visit, skip, depth + 1);
        }
    }
}

// forEach for several keys at once, in the reply's order: `visit(key, value)`.
template <typename F>
void forEachOf(V node, const std::set<std::string>& keys, F&& visit, int depth = 0) {
    if (node == nullptr || depth > kMaxDepth) {
        return;
    }
    if (yyjson_is_obj(node)) {
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(node, &iter);
        while (V k = yyjson_obj_iter_next(&iter)) {
            V value = yyjson_obj_iter_get_val(k);
            const char* name = yyjson_get_str(k);
            if (name != nullptr && keys.count(name) != 0) {
                visit(std::string(name), value);
            }
            forEachOf(value, keys, visit, depth + 1);
        }
    } else if (yyjson_is_arr(node)) {
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(node, &iter);
        while (V value = yyjson_arr_iter_next(&iter)) {
            forEachOf(value, keys, visit, depth + 1);
        }
    }
}

V first(V node, const char* key) {
    V found = nullptr;
    forEach(node, key, [&](V value) {
        if (found == nullptr) {
            found = value;
        }
    });
    return found;
}

void add(VideoPage& page, Video video) {
    if (video.id.empty() || page.items.size() >= kMaxItems) {
        return;
    }
    for (const Video& existing : page.items) {
        if (existing.id == video.id) {
            return;
        }
    }
    page.items.push_back(std::move(video));
}

// The token of the last "load more" item (headers, chips and side panels have their own, not wanted here).
std::string continuationOf(V root) {
    std::string token;
    forEach(
        root, "continuationItemRenderer",
        [&](V renderer) {
            const std::string found =
                json::string(at(at(renderer, "continuationEndpoint"), "continuationCommand"), "token");
            if (!found.empty()) {
                token = found;
            }
        },
        {"header", "engagementPanels", "chipBar", "engagementPanel"});
    return token;
}

std::optional<Video> fromVideoRenderer(V renderer) {
    Video video;
    video.id = json::string(renderer, "videoId");
    if (video.id.empty()) {
        return std::nullopt;
    }
    video.title = readText(at(renderer, "title"));
    V owner = at(renderer, "ownerText") != nullptr ? at(renderer, "ownerText") : at(renderer, "longBylineText");
    video.channelName = readText(owner);
    video.channelId = json::string(at(at(item(at(owner, "runs"), 0), "navigationEndpoint"), "browseEndpoint"),
                                   "browseId");
    video.durationText = readText(at(renderer, "lengthText"));
    video.durationSeconds = parseDuration(video.durationText);
    video.viewsText = readText(at(renderer, "viewCountText"));
    video.publishedText = readText(at(renderer, "publishedTimeText"));
    video.thumbnail = jpegThumbnail(bestImage(at(at(renderer, "thumbnail"), "thumbnails")));
    // a live stream has no length (and a scheduled one has its event data)
    video.live = video.durationText.empty() && at(renderer, "upcomingEventData") == nullptr;
    forEach(at(renderer, "badges"), "metadataBadgeRenderer", [&](V badge) {
        video.live = video.live || json::string(badge, "style") == "BADGE_STYLE_TYPE_LIVE_NOW";
    });
    forEach(at(renderer, "thumbnailOverlays"), "thumbnailOverlayTimeStatusRenderer", [&](V overlay) {
        video.live = video.live || json::string(overlay, "style") == "LIVE";
    });
    forEach(at(renderer, "thumbnailOverlays"), "thumbnailOverlayResumePlaybackRenderer", [&](V overlay) {
        const std::int64_t percent = json::integer(overlay, "percentDurationWatched", -1);
        video.percentWatched = percent >= 0 && percent <= 100 ? static_cast<int>(percent) : -1;
    });
    return video;
}

std::vector<std::vector<std::string>> metadataRows(V contentMetadata) {
    std::vector<std::vector<std::string>> rows;
    V list = at(contentMetadata, "metadataRows");
    for (std::size_t r = 0; r < count(list) && r < 8; ++r) {
        std::vector<std::string> parts;
        V partList = at(item(list, r), "metadataParts");
        for (std::size_t p = 0; p < count(partList) && p < 8; ++p) {
            const std::string part = readText(at(item(partList, p), "text"));
            if (!part.empty()) {
                parts.push_back(part);
            }
        }
        if (!parts.empty()) {
            rows.push_back(std::move(parts));
        }
    }
    return rows;
}

std::optional<Video> fromLockup(V lockup) {
    if (json::string(lockup, "contentType") != "LOCKUP_CONTENT_TYPE_VIDEO") {
        return std::nullopt;
    }
    Video video;
    video.id = json::string(lockup, "contentId");
    V meta = at(at(lockup, "metadata"), "lockupMetadataViewModel");
    video.title = readText(at(meta, "title"));
    const auto rows = metadataRows(at(at(meta, "metadata"), "contentMetadataViewModel"));
    std::vector<std::string> stats;
    if (rows.size() >= 2) {  // channel, then views · date (related videos)
        video.channelName = rows[0][0];
        stats = rows[1];
    } else if (!rows.empty()) {  // views · date (a channel's own videos)
        stats = rows[0];
    }
    if (!stats.empty()) {
        video.viewsText = stats[0];
    }
    if (stats.size() > 1) {
        video.publishedText = stats[1];
    }
    V image = at(at(lockup, "contentImage"), "thumbnailViewModel");
    video.thumbnail = jpegThumbnail(bestImage(at(at(image, "image"), "sources")));
    forEach(at(lockup, "contentImage"), "thumbnailBadgeViewModel", [&](V badge) {
        const std::string label = json::string(badge, "text");
        if (json::string(badge, "badgeStyle") == "THUMBNAIL_OVERLAY_BADGE_STYLE_LIVE") {
            video.live = true;
        } else if (video.durationText.empty() && parseDuration(label) > 0) {
            video.durationText = label;
            video.durationSeconds = parseDuration(label);
        }
    });
    return video;
}

// Mixes ("RD…") are endless radio lists that cannot be browsed as a playlist.
bool browsablePlaylist(const std::string& id) { return !id.empty() && id.rfind("RD", 0) != 0; }

std::optional<Video> fromPlaylistLockup(V lockup) {
    const std::string type = json::string(lockup, "contentType");
    if (type != "LOCKUP_CONTENT_TYPE_PLAYLIST" && type != "LOCKUP_CONTENT_TYPE_PODCAST") {
        return std::nullopt;
    }
    Video playlist;
    playlist.playlist = true;
    playlist.id = json::string(lockup, "contentId");
    if (!browsablePlaylist(playlist.id)) {
        return std::nullopt;
    }
    V meta = at(at(lockup, "metadata"), "lockupMetadataViewModel");
    playlist.title = readText(at(meta, "title"));
    const auto rows = metadataRows(at(at(meta, "metadata"), "contentMetadataViewModel"));
    if (!rows.empty() && rows[0].size() >= 2) {  // owner · "Playlist" (search); a channel's own: "Playlist" only
        playlist.channelName = rows[0][0];
    }
    V image = at(at(at(lockup, "contentImage"), "collectionThumbnailViewModel"), "primaryThumbnail");
    playlist.thumbnail = jpegThumbnail(bestImage(at(at(at(image, "thumbnailViewModel"), "image"), "sources")));
    forEach(image, "thumbnailBadgeViewModel", [&](V badge) {
        if (playlist.videoCountText.empty()) {
            playlist.videoCountText = json::string(badge, "text");
        }
    });
    return playlist;
}

// The token of a continuation item: a "load more" endpoint, or a "show more replies" button.
std::string continuationToken(V renderer) {
    std::string token = json::string(at(at(renderer, "continuationEndpoint"), "continuationCommand"), "token");
    if (token.empty()) {
        token = json::string(at(at(at(at(renderer, "button"), "buttonRenderer"), "command"), "continuationCommand"),
                             "token");
    }
    return token;
}

std::optional<json::Document> parseJson(std::string_view text) { return json::Document::parse(text); }

// A count written as a string ("213", "77000000"): 0 when not a number, capped at `max`.
long long boundedNumber(const std::string& text, long long max) {
    long long value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return 0;
        }
        value = value * 10 + (c - '0');
        if (value > max) {
            return max;
        }
    }
    return value;
}

int boundedInt(std::int64_t value, std::int64_t low, std::int64_t high, int fallback) {
    return value < low || value > high ? fallback : static_cast<int>(value);
}

// "1080p" -> 1080
int leadingNumber(const std::string& text, int max) {
    const std::size_t end = text.find_first_not_of("0123456789");
    return static_cast<int>(boundedNumber(text.substr(0, end), max));
}

constexpr long long kMaxSeconds = 1000000000;  // about 31 years
constexpr long long kMaxBytes = 1LL << 50;

}  // namespace

std::string jpegThumbnail(const std::string& url) {
    // /vi/<id>/, /vi_webp/<id>/ or /vi_lc/<id>/ (localised for the reply's language, signed only)
    std::size_t vi = std::string::npos;
    std::size_t prefix = 0;
    for (const char* folder : {"/vi/", "/vi_webp/", "/vi_lc/"}) {
        vi = url.find(folder);
        if (vi != std::string::npos) {
            prefix = std::strlen(folder);
            break;
        }
    }
    if (vi == std::string::npos) {
        return url;
    }
    const std::size_t idEnd = url.find('/', vi + prefix);
    if (idEnd == std::string::npos) {
        return url;
    }
    const std::string name = url.substr(idEnd + 1, url.find('?', idEnd) - idEnd - 1);
    // hq720.jpg exists when the reply named it, localised (hq720_fr.jpg) or not; every video has hqdefault.jpg
    const bool wide = name == "hq720.jpg" || (name.rfind("hq720_", 0) == 0 && url.find("/vi_lc/") == vi);
    return url.substr(0, vi) + "/vi/" + url.substr(vi + prefix, idEnd - vi - prefix) + "/" +
           (wide ? "hq720.jpg" : "hqdefault.jpg");
}

int parseDuration(std::string_view text) {
    int total = 0;
    int field = 0;
    int fields = 0;
    bool digit = false;
    for (char c : text) {
        if (c >= '0' && c <= '9') {
            field = field * 10 + (c - '0');
            digit = true;
            if (field > 100000) {
                return 0;
            }
        } else if (c == ':' && digit) {
            if (fields == 2) {
                return 0;
            }
            total = total * 60 + field;
            field = 0;
            digit = false;
            ++fields;
        } else {
            return 0;
        }
    }
    if (!digit || fields == 0 || fields > 2) {
        return 0;
    }
    return total * 60 + field;
}

// 11 characters of A-Z a-z 0-9 - _ (it goes into a picture's address)
bool validVideoId(const std::string& id) {
    if (id.size() != 11) {
        return false;
    }
    for (char c : id) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// A Short in search results or a channel's Shorts tab (shortsLockupViewModel): its id, title, views and its upright
// first frame (frame0.jpg answers without a signature, unlike the reply's own picture)
std::optional<Video> fromShortsLockup(V lockup) {
    Video video;
    video.id = json::string(at(at(at(lockup, "onTap"), "innertubeCommand"), "reelWatchEndpoint"), "videoId");
    if (!validVideoId(video.id)) {
        return std::nullopt;
    }
    V overlay = at(lockup, "overlayMetadata");
    video.title = readText(at(overlay, "primaryText"));
    video.viewsText = readText(at(overlay, "secondaryText"));
    // the reply's picture is signed (it may answer WebP): its folder's frame0.jpg instead, upright and unsigned
    const std::string picture =
        bestImage(at(at(at(at(lockup, "thumbnailViewModel"), "thumbnailViewModel"), "image"), "sources"));
    const std::string folder = "/vi/" + video.id + "/";
    const std::size_t found = picture.find(folder);
    video.thumbnail = found != std::string::npos && picture.rfind("https://", 0) == 0
                          ? picture.substr(0, found + folder.size()) + "frame0.jpg"
                          : "https://i.ytimg.com/vi/" + video.id + "/frame0.jpg";
    video.isShort = true;
    return video;
}

// A channel in search results (channelRenderer): its id, name, handle, subscribers and avatar
std::optional<Video> fromChannelRenderer(V renderer) {
    Video video;
    video.id = json::string(renderer, "channelId");
    if (video.id.rfind("UC", 0) != 0 || video.id.size() > 64) {
        return std::nullopt;
    }
    video.title = readText(at(renderer, "title"));
    video.channelName = readText(at(renderer, "subscriberCountText"));  // the handle, in this renderer
    video.viewsText = readText(at(renderer, "videoCountText"));        // the subscribers, in this one
    video.thumbnail = bestImage(at(at(renderer, "thumbnail"), "thumbnails"), 176);
    video.isChannel = true;
    return video;
}

core::Result<VideoPage> parseSearch(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("search: not JSON");
    }
    VideoPage page;
    VideoPage shorts;  // a plain search's Shorts shelf is left out; a search for Shorts gives only them
    forEachOf(doc->root(), {"videoRenderer", "lockupViewModel", "shortsLockupViewModel", "channelRenderer"},
              [&](const std::string& key, V value) {
        if (key == "channelRenderer") {
            if (auto found = fromChannelRenderer(value)) {
                add(page, std::move(*found));
            }
            return;
        }
        if (key == "shortsLockupViewModel") {
            if (auto found = fromShortsLockup(value)) {
                add(shorts, std::move(*found));
            }
            return;
        }
        auto found = key == "videoRenderer" ? fromVideoRenderer(value) : fromPlaylistLockup(value);
        if (found) {
            add(page, std::move(*found));
        }
    });
    if (page.items.empty()) {
        page.items = std::move(shorts.items);
    }
    page.continuation = continuationOf(doc->root());
    return page;
}

core::Result<ChannelPage> parseChannel(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("channel: not JSON");
    }
    ChannelPage page;
    if (V header = first(doc->root(), "pageHeaderViewModel")) {
        Channel& channel = page.channel;
        channel.name = readText(at(at(header, "title"), "dynamicTextViewModel") != nullptr
                                ? at(at(at(header, "title"), "dynamicTextViewModel"), "text")
                                : at(header, "title"));
        V avatar = at(at(at(at(header, "image"), "decoratedAvatarViewModel"), "avatar"), "avatarViewModel");
        channel.avatar = bestImage(at(at(avatar, "image"), "sources"), 176);
        const auto rows = metadataRows(at(at(header, "metadata"), "contentMetadataViewModel"));
        for (const auto& row : rows) {
            for (const std::string& part : row) {
                if (part.rfind("@", 0) == 0 && channel.handle.empty()) {
                    channel.handle = part;
                } else if (channel.subscribersText.empty() && !channel.handle.empty()) {
                    channel.subscribersText = part;
                }
            }
        }
        channel.description = readText(at(at(at(header, "description"), "descriptionPreviewViewModel"), "description"));
        V banner = at(at(at(header, "banner"), "imageBannerViewModel"), "image");
        channel.banner = bestImage(at(banner, "sources"), 2560);
    }
    V grid = first(doc->root(), "richGridRenderer");
    forEachOf(grid != nullptr ? grid : doc->root(), {"lockupViewModel", "shortsLockupViewModel"},
              [&](const std::string& key, V lockup) {
        if (auto video = key == "lockupViewModel" ? fromLockup(lockup) : fromShortsLockup(lockup)) {
            add(page.videos, std::move(*video));
        }
    });
    page.videos.continuation = continuationOf(grid != nullptr ? grid : doc->root());
    return page;
}

core::Result<VideoPage> parseChannelMore(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("channel: not JSON");
    }
    VideoPage page;
    forEachOf(doc->root(), {"lockupViewModel", "shortsLockupViewModel"}, [&](const std::string& key, V lockup) {
        if (auto video = key == "lockupViewModel" ? fromLockup(lockup) : fromShortsLockup(lockup)) {
            add(page, std::move(*video));
        }
    });
    page.continuation = continuationOf(doc->root());
    return page;
}

core::Result<NextReply> parseNext(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("next: not JSON");
    }
    NextReply reply;
    V secondary = first(doc->root(), "secondaryResults");
    VideoPage related;
    forEach(secondary != nullptr ? secondary : doc->root(), "lockupViewModel", [&](V lockup) {
        if (auto video = fromLockup(lockup)) {
            add(related, std::move(*video));
        }
    });
    forEach(secondary != nullptr ? secondary : doc->root(), "compactVideoRenderer", [&](V renderer) {
        if (auto video = fromVideoRenderer(renderer)) {
            add(related, std::move(*video));
        }
    });
    reply.related = std::move(related.items);
    forEach(doc->root(), "itemSectionRenderer", [&](V section) {
        if (reply.commentsContinuation.empty() &&
            json::string(section, "sectionIdentifier") == "comment-item-section") {
            if (V more = first(section, "continuationItemRenderer")) {
                reply.commentsContinuation = continuationToken(more);
            }
        }
    });
    if (V owner = first(doc->root(), "videoOwnerRenderer")) {
        reply.owner.name = readText(at(owner, "title"));
        reply.owner.id = json::string(
            at(at(item(at(at(owner, "title"), "runs"), 0), "navigationEndpoint"), "browseEndpoint"), "browseId");
        reply.owner.avatar = bestImage(at(at(owner, "thumbnail"), "thumbnails"), 176);
        reply.owner.subscribersText = readText(at(owner, "subscriberCountText"));
    }
    // the chapters of the player's bar (the uploader's, else YouTube's own): kept in order of start, from 0
    V bar = at(at(at(at(at(at(doc->root(), "playerOverlays"), "playerOverlayRenderer"), "decoratedPlayerBarRenderer"),
                     "decoratedPlayerBarRenderer"),
                  "playerBar"),
               "multiMarkersPlayerBarRenderer");
    V markers = at(bar, "markersMap");
    for (std::size_t m = 0; m < count(markers) && reply.chapters.empty(); ++m) {
        V list = at(at(item(markers, m), "value"), "chapters");
        for (std::size_t c = 0; c < count(list) && reply.chapters.size() < kMaxItems; ++c) {
            V chapter = at(item(list, c), "chapterRenderer");
            const std::int64_t startMs = json::integer(chapter, "timeRangeStartMillis", -1);
            Chapter parsed;
            parsed.title = readText(at(chapter, "title"));
            parsed.start = static_cast<double>(startMs) / 1000.0;
            if (startMs < 0 || (!reply.chapters.empty() && parsed.start <= reply.chapters.back().start)) {
                continue;  // out of order: left out
            }
            reply.chapters.push_back(std::move(parsed));
        }
    }
    if (reply.chapters.size() < 2 || reply.chapters.front().start > 0) {
        reply.chapters.clear();  // one chapter, or not from the start: none worth showing
    }
    // a live video's chat (not the replay of an ended one's)
    V chat = at(at(at(at(doc->root(), "contents"), "twoColumnWatchNextResults"), "conversationBar"), "liveChatRenderer");
    if (!json::boolean(chat, "isReplay", false)) {
        reply.liveChat = json::string(at(item(at(chat, "continuations"), 0), "reloadContinuationData"), "continuation");
    }
    return reply;
}

core::Result<ChatPage> parseLiveChat(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("live chat: not JSON");
    }
    V chat = at(at(doc->root(), "continuationContents"), "liveChatContinuation");
    if (chat == nullptr) {
        return badReply("live chat: no chat");
    }
    ChatPage page;
    V next = item(at(chat, "continuations"), 0);
    for (const char* kind : {"invalidationContinuationData", "timedContinuationData", "reloadContinuationData"}) {
        if (V data = at(next, kind)) {
            page.continuation = json::string(data, "continuation");
            page.waitMs = static_cast<int>(json::integer(data, "timeoutMs", 0));
            break;
        }
    }
    V actions = at(chat, "actions");
    for (std::size_t i = 0; i < count(actions); ++i) {
        V action = item(actions, i);
        if (V removed = at(action, "removeChatItemAction")) {
            const std::string id = json::string(removed, "targetItemId");
            if (!id.empty() && page.removed.size() < kMaxItems) {
                page.removed.push_back(id);
            }
            continue;
        }
        V entry = at(at(action, "addChatItemAction"), "item");
        V message = at(entry, "liveChatTextMessageRenderer");
        V paid = at(entry, "liveChatPaidMessageRenderer");
        V renderer = message != nullptr ? message : paid;
        if (renderer == nullptr || page.messages.size() >= kMaxItems) {
            continue;
        }
        ChatMessage parsed;
        parsed.id = json::string(renderer, "id");
        parsed.author = readText(at(renderer, "authorName"));
        parsed.text = readText(at(renderer, "message"));
        if (paid != nullptr) {
            parsed.amount = readText(at(paid, "purchaseAmountText"));
        }
        if (parsed.id.empty() || (parsed.text.empty() && parsed.amount.empty())) {
            continue;  // emoji only, or nothing to show
        }
        page.messages.push_back(std::move(parsed));
    }
    return page;
}

core::Result<PlayerReply> parsePlayer(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("player: not JSON");
    }
    V root = doc->root();
    PlayerReply reply;
    V status = at(root, "playabilityStatus");
    reply.playable = json::string(status, "status") == "OK";
    reply.reason = json::string(status, "reason");
    if (reply.reason.empty()) {
        reply.reason = readText(at(at(at(status, "errorScreen"), "playerErrorMessageRenderer"), "subreason"));
    }
    reply.botCheck = json::string(status, "status") == "LOGIN_REQUIRED" && !reply.reason.empty() &&
                     at(status, "desktopLegacyAgeGateReason") == nullptr && at(status, "messages") == nullptr &&
                     at(status, "errorScreen") == nullptr;
    V details = at(root, "videoDetails");
    reply.title = json::string(details, "title");
    reply.channelId = json::string(details, "channelId");
    reply.durationSeconds = static_cast<int>(boundedNumber(json::string(details, "lengthSeconds"), kMaxSeconds));
    reply.live = json::boolean(details, "isLive", false);
    V streaming = at(root, "streamingData");
    for (const char* list : {"formats", "adaptiveFormats"}) {
        V formats = at(streaming, list);
        for (std::size_t i = 0; i < count(formats) && reply.formats.size() < kMaxItems; ++i) {
            V entry = item(formats, i);
            Format format;
            format.url = https(json::string(entry, "url"));
            if (format.url.empty()) {
                continue;  // a signature to decipher: not used
            }
            format.itag = static_cast<int>(json::integer(entry, "itag", 0));
            format.height = static_cast<int>(json::integer(entry, "height", 0));
            format.mimeType = json::string(entry, "mimeType");
            format.contentLength = boundedNumber(json::string(entry, "contentLength"), kMaxBytes);
            V track = at(entry, "audioTrack");
            format.defaultAudio = json::boolean(track, "audioIsDefault", true);
            format.audioTrackId = json::string(track, "id");
            format.audioTrackName = json::string(track, "displayName");
            format.drc = json::boolean(entry, "isDrc", false);
            reply.formats.push_back(std::move(format));
        }
    }
    reply.hls = https(json::string(streaming, "hlsManifestUrl"));
    reply.storyboardSpec = json::string(at(at(root, "storyboards"), "playerStoryboardSpecRenderer"), "spec");
    V tracks = at(at(at(root, "captions"), "playerCaptionsTracklistRenderer"), "captionTracks");
    for (std::size_t i = 0; i < count(tracks) && reply.captions.size() < kMaxItems; ++i) {
        V entry = item(tracks, i);
        CaptionTrack track;
        track.url = https(json::string(entry, "baseUrl"));
        track.language = json::string(entry, "languageCode");
        track.name = readText(at(entry, "name"));
        track.automatic = json::string(entry, "kind") == "asr";
        if (!track.url.empty()) {
            reply.captions.push_back(std::move(track));
        }
    }
    return reply;
}

core::Result<VideoPage> parseTiles(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("feed: not JSON");
    }
    VideoPage page;
    forEach(doc->root(), "tileRenderer", [&](V tile) {
        const std::string type = json::string(tile, "contentType");
        if (type != "TILE_CONTENT_TYPE_VIDEO" && type != "TILE_CONTENT_TYPE_PLAYLIST") {
            return;
        }
        Video video;
        video.playlist = type == "TILE_CONTENT_TYPE_PLAYLIST";
        video.id = json::string(tile, "contentId");
        if (video.id.empty() && !video.playlist) {
            video.id = json::string(at(at(tile, "onSelectCommand"), "watchEndpoint"), "videoId");
        }
        if (video.id.empty() && video.playlist) {
            const std::string browse = json::string(at(at(tile, "onSelectCommand"), "browseEndpoint"), "browseId");
            video.id = browse.rfind("VL", 0) == 0 ? browse.substr(2) : std::string();
        }
        if (video.playlist && !browsablePlaylist(video.id)) {
            return;
        }
        V meta = at(at(tile, "metadata"), "tileMetadataRenderer");
        video.title = readText(at(meta, "title"));
        V lines = at(meta, "lines");
        for (std::size_t l = 0; l < count(lines) && l < 4; ++l) {
            V items = at(at(item(lines, l), "lineRenderer"), "items");
            for (std::size_t i = 0; i < count(items) && i < 8; ++i) {
                const std::string part = readText(at(at(item(items, i), "lineItemRenderer"), "text"));
                if (part.empty() || part == "\xE2\x80\xA2") {
                    continue;
                }
                if (l == 0 && video.channelName.empty()) {
                    video.channelName = part;
                } else if (l > 0 && video.playlist && video.publishedText.empty()) {  // "Mise à jour …"
                    video.publishedText = part;
                } else if (l > 0 && !video.playlist && video.viewsText.empty()) {
                    video.viewsText = part;
                } else if (l > 0 && !video.playlist && video.publishedText.empty()) {
                    video.publishedText = part;
                }
            }
        }
        V header = at(at(tile, "header"), "tileHeaderRenderer");
        video.thumbnail = jpegThumbnail(bestImage(at(at(header, "thumbnail"), "thumbnails")));
        forEach(at(header, "thumbnailOverlays"), "thumbnailOverlayTimeStatusRenderer", [&](V overlay) {
            if (json::string(overlay, "style") == "LIVE") {
                video.live = true;
            } else if (video.playlist) {
                video.videoCountText = readText(at(overlay, "text"));
            } else {
                video.durationText = readText(at(overlay, "text"));
                video.durationSeconds = parseDuration(video.durationText);
            }
        });
        forEach(at(header, "thumbnailOverlays"), "thumbnailOverlayResumePlaybackRenderer", [&](V overlay) {
            video.percentWatched = boundedInt(json::integer(overlay, "percentDurationWatched", -1), -1, 100, -1);
        });
        add(page, std::move(video));
    });
    // the page's own continuation (the list of rows), else the first one found
    for (const char* list : {"sectionListRenderer", "sectionListContinuation", "gridContinuation", "gridRenderer"}) {
        if (V section = first(doc->root(), list)) {
            page.continuation = json::string(at(item(at(section, "continuations"), 0), "nextContinuationData"),
                                             "continuation");
            if (!page.continuation.empty()) {
                break;
            }
        }
    }
    if (page.continuation.empty()) {
        if (V next = first(doc->root(), "nextContinuationData")) {
            page.continuation = json::string(next, "continuation");
        }
    }
    return page;
}

core::Result<std::vector<std::string>> parseSuggestions(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_arr(doc->root())) {
        return badReply("suggestions: not a list");
    }
    std::vector<std::string> out;
    V list = item(doc->root(), 1);
    for (std::size_t i = 0; i < count(list) && out.size() < 20; ++i) {
        const std::string suggestion = json::textOf(item(item(list, i), 0));
        if (!suggestion.empty()) {
            out.push_back(suggestion);
        }
    }
    return out;
}

core::Result<std::vector<Segment>> parseSegments(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_arr(doc->root())) {
        return badReply("segments: not a list");
    }
    std::vector<Segment> out;
    for (std::size_t i = 0; i < count(doc->root()) && out.size() < kMaxItems; ++i) {
        V entry = item(doc->root(), i);
        V range = at(entry, "segment");
        if (count(range) != 2 || !yyjson_is_num(item(range, 0)) || !yyjson_is_num(item(range, 1))) {
            continue;
        }
        Segment segment;
        segment.start = yyjson_get_num(item(range, 0));
        segment.end = yyjson_get_num(item(range, 1));
        segment.category = json::string(entry, "category");
        if (segment.end > segment.start && segment.start >= 0) {
            out.push_back(std::move(segment));
        }
    }
    return out;
}

std::string parseVisitorData(std::string_view text) {
    if (text.rfind(")]}'", 0) == 0) {
        text.remove_prefix(4);
    }
    const auto doc = parseJson(text);
    if (!doc) {
        return {};
    }
    V value = item(item(item(item(item(doc->root(), 0), 2), 0), 0), 13);
    return json::textOf(value);
}

std::string parseAccountName(std::string_view text) {
    const auto doc = parseJson(text);
    return doc ? readText(first(doc->root(), "accountName")) : std::string();
}

core::Result<PlayerReply> parseInvidious(std::string_view text, const std::string& instance) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_obj(doc->root())) {
        return badReply("invidious: not an object");
    }
    V root = doc->root();
    PlayerReply reply;
    reply.title = json::string(root, "title");
    reply.channelId = json::string(root, "authorId");
    reply.durationSeconds = boundedInt(json::integer(root, "lengthSeconds", 0), 0, kMaxSeconds, 0);
    reply.live = json::boolean(root, "liveNow", false);
    reply.hls = https(json::string(root, "hlsUrl"));
    for (const char* list : {"formatStreams", "adaptiveFormats"}) {
        V formats = at(root, list);
        for (std::size_t i = 0; i < count(formats) && reply.formats.size() < kMaxItems; ++i) {
            V entry = item(formats, i);
            Format format;
            format.url = https(json::string(entry, "url"));
            if (format.url.empty()) {
                continue;
            }
            V itag = at(entry, "itag");
            format.itag = yyjson_is_str(itag) ? leadingNumber(json::textOf(itag), 100000)
                                              : static_cast<int>(json::integer(entry, "itag", 0));
            format.mimeType = json::string(entry, "type");
            format.height = format.mimeType.rfind("audio/", 0) == 0
                                ? 0
                                : leadingNumber(json::string(entry, "resolution"), 100000);
            format.contentLength = boundedNumber(json::string(entry, "clen"), kMaxBytes);
            reply.formats.push_back(std::move(format));
        }
    }
    V captions = at(root, "captions");
    for (std::size_t i = 0; i < count(captions) && reply.captions.size() < kMaxItems; ++i) {
        V entry = item(captions, i);
        CaptionTrack track;
        const std::string url = json::string(entry, "url");
        track.url = url.rfind("/", 0) == 0 ? instance + url : https(url);
        track.language = json::string(entry, "language_code");
        track.name = json::string(entry, "label");
        if (!track.url.empty()) {
            reply.captions.push_back(std::move(track));
        }
    }
    reply.playable = !reply.formats.empty() || !reply.hls.empty();
    if (!reply.playable) {
        reply.reason = json::string(root, "error");
    }
    return reply;
}

core::Result<DeviceCode> parseDeviceCode(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("device code: not JSON");
    }
    DeviceCode code;
    code.deviceCode = json::string(doc->root(), "device_code");
    code.userCode = json::string(doc->root(), "user_code");
    code.verificationUrl = json::string(doc->root(), "verification_url");
    if (code.verificationUrl.empty()) {
        code.verificationUrl = json::string(doc->root(), "verification_uri");
    }
    code.interval = boundedInt(json::integer(doc->root(), "interval", 5), 1, 60, 5);
    code.expiresIn = boundedInt(json::integer(doc->root(), "expires_in", 1800), 1, 86400, 1800);
    if (code.deviceCode.empty() || code.userCode.empty()) {
        return badReply("device code: no code");
    }
    return code;
}

core::Result<Token> parseToken(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("token: not JSON");
    }
    Token token;
    const std::string error = json::string(doc->root(), "error");
    if (error == "authorization_pending" || error == "slow_down") {
        token.pending = true;
        token.slowDown = error == "slow_down";
        return token;
    }
    if (error == "invalid_grant" || error == "access_denied" || error == "expired_token") {
        return core::makeError(core::ErrorKind::Unauthorized, error);
    }
    if (!error.empty()) {
        return core::makeError(core::ErrorKind::Server, "token: " + error);
    }
    token.accessToken = json::string(doc->root(), "access_token");
    token.refreshToken = json::string(doc->root(), "refresh_token");
    token.expiresIn = boundedInt(json::integer(doc->root(), "expires_in", 0), 0, 86400, 0);
    if (token.accessToken.empty()) {
        return badReply("token: no access token");
    }
    return token;
}

core::Result<VideoPage> parseChannelPlaylists(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("channel playlists: not JSON");
    }
    VideoPage page;
    forEach(doc->root(), "lockupViewModel", [&](V lockup) {
        if (auto playlist = fromPlaylistLockup(lockup)) {
            add(page, std::move(*playlist));
        }
    });
    page.continuation = continuationOf(doc->root());
    return page;
}

core::Result<CommentPage> parseComments(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("comments: not JSON");
    }
    V root = doc->root();
    // the texts are entities, found by the comment's key
    std::vector<std::pair<std::string, V>> entities;
    V mutations = at(at(at(root, "frameworkUpdates"), "entityBatchUpdate"), "mutations");
    for (std::size_t i = 0; i < count(mutations) && entities.size() < 4 * kMaxItems; ++i) {
        V mutation = item(mutations, i);
        if (V payload = at(at(mutation, "payload"), "commentEntityPayload")) {
            entities.emplace_back(json::string(mutation, "entityKey"), payload);
        }
    }
    auto entity = [&](const std::string& key) -> V {
        for (const auto& [k, v] : entities) {
            if (!key.empty() && k == key) {
                return v;
            }
        }
        return nullptr;
    };
    CommentPage page;
    auto addComment = [&](V viewModel, V thread) {
        V payload = entity(json::string(viewModel, "commentKey"));
        if (payload == nullptr || page.items.size() >= kMaxItems) {
            return;
        }
        V properties = at(payload, "properties");
        V author = at(payload, "author");
        V toolbar = at(payload, "toolbar");
        Comment comment;
        comment.id = json::string(properties, "commentId");
        comment.text = readText(at(properties, "content"));
        comment.publishedText = json::string(properties, "publishedTime");
        comment.author = json::string(author, "displayName");
        comment.avatar = https(json::string(author, "avatarThumbnailUrl"));
        comment.likesText = json::string(toolbar, "likeCountNotliked");
        comment.repliesText = json::string(toolbar, "replyCount");
        if (thread != nullptr) {
            if (V more = first(at(thread, "replies"), "continuationItemRenderer")) {
                comment.repliesContinuation = continuationToken(more);
            }
        }
        if (comment.repliesContinuation.empty()) {
            comment.repliesText.clear();
        }
        page.items.push_back(std::move(comment));
    };
    forEach(at(root, "onResponseReceivedEndpoints"), "continuationItems", [&](V list) {
        for (std::size_t i = 0; i < count(list); ++i) {
            V entry = item(list, i);
            if (V thread = at(entry, "commentThreadRenderer")) {
                addComment(at(at(thread, "commentViewModel"), "commentViewModel"), thread);
            } else if (V reply = at(entry, "commentViewModel")) {
                addComment(reply, nullptr);
            } else if (V more = at(entry, "continuationItemRenderer")) {
                page.continuation = continuationToken(more);
            }
        }
    });
    return page;
}

core::Result<VideoState> parseVideoState(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("video state: not JSON");
    }
    VideoState state;
    if (V like = first(doc->root(), "likeButtonRenderer")) {
        const std::string status = json::string(like, "likeStatus");
        state.rating = status == "LIKE" ? Rating::Like : status == "DISLIKE" ? Rating::Dislike : Rating::None;
        state.likesText = readText(at(like, "likeCountText"));
    }
    if (V subscribe = first(doc->root(), "subscribeButtonRenderer")) {
        state.subscribed = json::boolean(subscribe, "subscribed");
        state.channelId = json::string(subscribe, "channelId");
    }
    return state;
}

core::Result<std::vector<PlaylistChoice>> parseAddToPlaylist(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc) {
        return badReply("playlists: not JSON");
    }
    std::vector<PlaylistChoice> choices;
    forEach(doc->root(), "playlistAddToOptionRenderer", [&](V option) {
        PlaylistChoice choice;
        choice.id = json::string(option, "playlistId");
        choice.title = readText(at(option, "title"));
        const std::string holds = json::string(option, "containsSelectedVideos");
        choice.contains = !holds.empty() && holds != "NONE";
        if (!choice.id.empty() && choices.size() < kMaxItems) {
            choices.push_back(std::move(choice));
        }
    });
    return choices;
}

core::Result<PlaybackTracking> parsePlaybackTracking(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_obj(doc->root())) {
        return badReply("player: not an object");
    }
    V tracking = at(doc->root(), "playbackTracking");
    PlaybackTracking out;
    out.playbackUrl = json::string(at(tracking, "videostatsPlaybackUrl"), "baseUrl");
    out.watchtimeUrl = json::string(at(tracking, "videostatsWatchtimeUrl"), "baseUrl");
    if (out.playbackUrl.empty()) {
        // why: YouTube's own status and words (a refused player reply carries no reporting address)
        V status = at(doc->root(), "playabilityStatus");
        std::string reason = json::string(status, "reason");
        if (reason.empty()) {
            reason = readText(at(at(at(status, "errorScreen"), "playerErrorMessageRenderer"), "subreason"));
        }
        return core::makeError(core::ErrorKind::BadResponse,
                               "player: no playback tracking (" + json::string(status, "status") + ": " + reason + ")");
    }
    return out;
}

// DeArrow lists its submissions best first; the original title or thumbnail first means "keep YouTube's". A
// submission counts when a moderator locked it or its votes are not negative. ">" before a word only tells
// DeArrow's own formatter to leave it as written: removed.
core::Result<Branding> parseBranding(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_obj(doc->root())) {
        return badReply("branding: not an object");
    }
    const auto trusted = [](V entry) {
        return json::boolean(entry, "locked", false) || json::integer(entry, "votes", -1) >= 0;
    };
    Branding branding;
    V titles = at(doc->root(), "titles");
    for (std::size_t i = 0; i < count(titles); ++i) {
        V entry = item(titles, i);
        if (!trusted(entry)) {
            continue;
        }
        if (!json::boolean(entry, "original", false)) {
            std::string title;
            for (char c : json::string(entry, "title")) {
                if (c != '>') {
                    title += c;
                }
            }
            branding.title = title;
        }
        break;
    }
    V thumbnails = at(doc->root(), "thumbnails");
    for (std::size_t i = 0; i < count(thumbnails); ++i) {
        V entry = item(thumbnails, i);
        if (!trusted(entry)) {
            continue;
        }
        if (!json::boolean(entry, "original", false)) {
            const double time = json::number(entry, "timestamp", -1);
            branding.thumbnailTime = time >= 0 && time < 1e7 ? time : -1;
        }
        break;
    }
    return branding;
}

core::Result<Votes> parseVotes(std::string_view text) {
    const auto doc = parseJson(text);
    if (!doc || !yyjson_is_obj(doc->root())) {
        return badReply("votes: not an object");
    }
    Votes votes;
    votes.likes = json::integer(doc->root(), "likes", -1);
    votes.dislikes = json::integer(doc->root(), "dislikes", -1);
    if (votes.likes < 0 || votes.dislikes < 0) {
        return badReply("votes: no counts");
    }
    return votes;
}

}  // namespace playtube::youtube
