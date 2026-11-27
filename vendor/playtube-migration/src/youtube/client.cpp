#include "youtube/client.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "core/json.hpp"
#include "core/log.hpp"
#include "net/url.hpp"
#include "youtube/parse.hpp"

namespace playtube::youtube {

namespace json = core::json;

// One InnerTube client: its name and version, the extra context fields its app sends, and its User-Agent.
struct ClientProfile {
    const char* name;
    int id;
    const char* version;
    const char* userAgent;
    std::vector<std::pair<const char*, const char*>> context;
    int androidSdk = 0;
    bool muxedOnly = false;  // separate streams stop after about 5 MB without a PO token (IOS, ANDROID)
};

namespace {

const char* const kWebAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 "
    "Safari/537.36";
const char* const kVisionAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 15_7_3) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 "
    "Safari/605.1.15";
const char* const kVrAgent =
    "com.google.android.apps.youtube.vr.oculus/1.60.19 (Linux; U; Android 12L; eureka-user "
    "Build/SQ3A.220605.009.A1) gzip";
const char* const kIosAgent = "com.google.ios.youtube/20.10.4 (iPhone16,2; U; CPU iOS 18_3_2 like Mac OS X;)";
const char* const kAndroidAgent = "com.google.android.youtube/20.10.38 (Linux; U; Android 11) gzip";
const char* const kTvAgent = "Mozilla/5.0 (ChromiumStylePlatform) Cobalt/Version";

const ClientProfile kWeb{"WEB", 1, "2.20250923.01.00", kWebAgent, {}};
const ClientProfile kTv{"TVHTML5", 7, "7.20250923.13.00", kTvAgent, {}};
const ClientProfile kVision{"VISIONOS", 101, "1.02", kVisionAgent,
                            {{"deviceMake", "Apple"}, {"deviceModel", "RealityDevice17,1"}, {"osName", "visionOS"},
                             {"osVersion", "26.5.23O471"}, {"userAgent", kVisionAgent}}};
const ClientProfile kVr{"ANDROID_VR", 28, "1.60.19", kVrAgent,
                        {{"deviceMake", "Oculus"}, {"deviceModel", "Quest 3"}, {"osName", "Android"},
                         {"osVersion", "12L"}, {"userAgent", kVrAgent}},
                        32};
const ClientProfile kIos{"IOS", 5, "20.10.4", kIosAgent,
                         {{"deviceMake", "Apple"}, {"deviceModel", "iPhone16,2"}, {"osName", "iPhone"},
                          {"osVersion", "18.3.2.22D82"}, {"userAgent", kIosAgent}},
                         0,
                         true};
const ClientProfile kAndroid{"ANDROID", 3, "20.10.38", kAndroidAgent,
                             {{"osName", "Android"}, {"osVersion", "11"}, {"userAgent", kAndroidAgent}},
                             30,
                             true};

// "Videos" and "Playlists" tabs of a channel
const char* const kChannelVideos = "EgZ2aWRlb3PyBgQKAjoA";
const char* const kChannelPlaylists = "EglwbGF5bGlzdHPyBgQKAkIA";
const char* const kChannelShorts = "EgZzaG9ydHPyBgUKA5oBAA==";
const char* const kChannelLives = "EgdzdHJlYW1z8gYECgJ6AA==";

core::Result<std::string> checked(core::Result<net::HttpResponse> response, const char* what) {
    if (!response.ok()) {
        return response.error();
    }
    const long status = response.value().status;
    if (status >= 200 && status < 300) {
        return std::move(response.value().body);
    }
    const std::string message = std::string(what) + ": HTTP " + std::to_string(status);
    if (status == 401 || status == 403) {
        return core::makeError(core::ErrorKind::Unauthorized, message, static_cast<int>(status));
    }
    if (status == 404) {
        return core::makeError(core::ErrorKind::NotFound, message, static_cast<int>(status));
    }
    return core::makeError(core::ErrorKind::Server, message, static_cast<int>(status));
}

template <typename T, typename Parser>
core::Result<T> parsed(const core::Result<std::string>& body, Parser parser) {
    if (!body.ok()) {
        return body.error();
    }
    return parser(body.value());
}

core::Error cancelled() { return core::makeError(core::ErrorKind::Cancelled, "cancelled"); }

}  // namespace

Client::Client(const net::HttpClient& http, Endpoints endpoints, std::string hl, std::string gl)
    : http_(http), endpoints_(std::move(endpoints)), hl_(std::move(hl)), gl_(std::move(gl)) {}

core::Result<std::string> Client::post(const ClientProfile& profile, const char* endpoint, const Fields& fields,
                                       const std::string& accessToken, const core::CancelToken& cancel,
                                       const Fields& json) const {
    if (cancel.cancelled()) {
        return cancelled();
    }
    const bool player = std::string(endpoint) == "player";
    const std::string visitor = player ? visitorData(cancel) : std::string();
    json::Writer body;
    body.object().key("context").object().key("client").object();
    body.key("clientName").value(profile.name).key("clientVersion").value(profile.version);
    body.key("hl").value(hl_).key("gl").value(gl_);
    for (const auto& field : profile.context) {
        body.key(field.first).value(field.second);
    }
    if (profile.androidSdk > 0) {
        body.key("androidSdkVersion").value(profile.androidSdk);
    }
    if (!visitor.empty()) {
        body.key("visitorData").value(visitor);
    }
    body.end().end();  // client, context
    for (const auto& field : fields) {
        body.key(field.first).value(field.second);
    }
    for (const auto& field : json) {
        body.key(field.first).raw(field.second);
    }
    body.end();

    net::HttpRequest request;
    request.method = "POST";
    request.url = endpoints_.youtube + "/youtubei/v1/" + endpoint + "?prettyPrint=false";
    request.body = body.str();
    request.maxBodyBytes = kMaxReplyBytes;
    request.headers = {{"Content-Type", "application/json"},
                       {"User-Agent", profile.userAgent},
                       {"X-YouTube-Client-Name", std::to_string(profile.id)},
                       {"X-YouTube-Client-Version", profile.version},
                       {"Origin", "https://www.youtube.com"}};
    if (!visitor.empty()) {
        request.headers.emplace_back("X-Goog-Visitor-Id", visitor);
    }
    if (!accessToken.empty()) {
        request.headers.emplace_back("Authorization", "Bearer " + accessToken);
    }
    return checked(http_.send(request, {}, cancel), endpoint);
}

core::Result<std::string> Client::get(const std::string& url, const std::string& userAgent,
                                      const core::CancelToken& cancel) const {
    if (cancel.cancelled()) {
        return cancelled();
    }
    net::HttpRequest request;
    request.url = url;
    request.maxBodyBytes = kMaxReplyBytes;
    request.headers = {{"User-Agent", userAgent}};
    return checked(http_.send(request, {}, cancel), "request");
}

std::string Client::visitorData(const core::CancelToken& cancel) const {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!visitor_.empty()) {
            return visitor_;
        }
    }
    net::HttpRequest request;
    request.url = endpoints_.youtube + "/sw.js_data";
    request.maxBodyBytes = kMaxReplyBytes;
    request.headers = {{"User-Agent", kWebAgent}, {"Cookie", "SOCS=CAI"}};
    const auto body = checked(http_.send(request, {}, cancel), "sw.js_data");
    const std::string visitor = body.ok() ? parseVisitorData(body.value()) : std::string();
    if (visitor.empty()) {
        PF_LOG_WARN("youtube: no visitor id");
        return {};
    }
    std::lock_guard<std::mutex> lock(mutex_);
    visitor_ = visitor;
    return visitor_;
}

// The first kilobyte of a stream answers: the stream will play (YouTube refuses some with 403).
bool Client::reachable(const std::string& url, const std::string& userAgent, const core::CancelToken& cancel) const {
    // A stream that needs a PO token answers its first bytes and refuses a range further on (mpv opens it with
    // "Range: bytes=0-"): one byte at 80 % of its length (the address carries it as `clen`) tells.
    long long length = 0;
    const std::size_t clen = url.find("clen=");
    if (clen != std::string::npos && clen > 0 && (url[clen - 1] == '?' || url[clen - 1] == '&')) {
        length = std::atoll(url.c_str() + clen + 5);
    }
    const long long at = length > 0 ? length / 10 * 8 : 0;
    net::HttpRequest request;
    request.url = url;
    request.maxBodyBytes = 64 * 1024;
    request.timeoutMs = 15000;
    request.headers = {{"User-Agent", userAgent},
                       {"Range", "bytes=" + std::to_string(at) + "-" + std::to_string(length > 0 ? at : 1023)}};
    const auto response = http_.send(request, {}, cancel);
    const bool ok = response.ok() && response.value().status >= 200 && response.value().status < 300;
    if (!ok) {
        PF_LOG_DEBUG("youtube: probe at %lld: %s", at,
                     response.ok() ? ("HTTP " + std::to_string(response.value().status)).c_str()
                                   : response.error().message.c_str());
    }
    return ok;
}

core::Result<VideoPage> Client::search(const std::string& query, const core::CancelToken& cancel,
                                       const std::string& params) const {
    if (params.empty()) {
        return parsed<VideoPage>(post(kWeb, "search", {{"query", query}}, {}, cancel), parseSearch);
    }
    return parsed<VideoPage>(post(kWeb, "search", {{"query", query}, {"params", params}}, {}, cancel), parseSearch);
}

core::Result<VideoPage> Client::searchMore(const std::string& continuation, const core::CancelToken& cancel) const {
    return parsed<VideoPage>(post(kWeb, "search", {{"continuation", continuation}}, {}, cancel),
                             parseSearch);
}

core::Result<std::vector<std::string>> Client::suggestions(const std::string& text,
                                                           const core::CancelToken& cancel) const {
    const std::string url = endpoints_.suggest + "/complete/search?client=youtube-lr&ds=yt&xhr=t&oe=utf-8&hl=" +
                            net::urlEncode(hl_) + "&gl=" + net::urlEncode(gl_) + "&q=" + net::urlEncode(text);
    return parsed<std::vector<std::string>>(get(url, kWebAgent, cancel), parseSuggestions);
}

core::Result<ChannelPage> Client::channel(const std::string& channelId, const core::CancelToken& cancel) const {
    auto page = parsed<ChannelPage>(
        post(kWeb, "browse", {{"browseId", channelId}, {"params", kChannelVideos}}, {}, cancel),
        parseChannel);
    if (page.ok()) {
        page.value().channel.id = channelId;
    }
    return page;
}

core::Result<VideoPage> Client::channelTab(const std::string& channelId, ChannelTab tab,
                                           const core::CancelToken& cancel) const {
    const char* params = tab == ChannelTab::Shorts ? kChannelShorts : tab == ChannelTab::Lives ? kChannelLives : kChannelVideos;
    auto page = parsed<ChannelPage>(post(kWeb, "browse", {{"browseId", channelId}, {"params", params}}, {}, cancel),
                                    parseChannel);
    if (!page.ok()) {
        return page.error();
    }
    return std::move(page.value().videos);
}

core::Result<VideoPage> Client::channelMore(const std::string& continuation, const core::CancelToken& cancel) const {
    return parsed<VideoPage>(post(kWeb, "browse", {{"continuation", continuation}}, {}, cancel),
                             parseChannelMore);
}

core::Result<NextReply> Client::next(const std::string& videoId, const core::CancelToken& cancel) const {
    return parsed<NextReply>(post(kWeb, "next", {{"videoId", videoId}}, {}, cancel), parseNext);
}

core::Result<CommentPage> Client::comments(const std::string& continuation, const core::CancelToken& cancel) const {
    return parsed<CommentPage>(post(kWeb, "next", {{"continuation", continuation}}, {}, cancel), parseComments);
}

core::Result<VideoPage> Client::channelPlaylists(const std::string& channelId, const core::CancelToken& cancel) const {
    return parsed<VideoPage>(
        post(kWeb, "browse", {{"browseId", channelId}, {"params", kChannelPlaylists}}, {}, cancel),
        parseChannelPlaylists);
}

core::Result<VideoPage> Client::channelPlaylistsMore(const std::string& continuation,
                                                     const core::CancelToken& cancel) const {
    return parsed<VideoPage>(post(kWeb, "browse", {{"continuation", continuation}}, {}, cancel),
                             parseChannelPlaylists);
}

core::Result<VideoPage> Client::playlist(const std::string& playlistId, const std::string& accessToken,
                                         const core::CancelToken& cancel) const {
    return parsed<VideoPage>(post(kTv, "browse", {{"browseId", "VL" + playlistId}}, accessToken, cancel),
                             parseTiles);
}

std::optional<Streams> Client::fromClients(const std::string& videoId, int maxHeight, const core::CancelToken& cancel,
                                           std::optional<std::string>& reason,
                                           std::optional<core::Error>& failure, bool& setback,
                                           bool& blocked) const {
    setback = false;
    // Clients the bot check refused a moment ago are left alone; when all are, the first is asked alone (its
    // answer tells whether the block lifted)
    std::vector<const ClientProfile*> profiles;
    for (const ClientProfile* profile : {&kVision, &kVr, &kIos, &kAndroid}) {
        if (!resting(profile)) {
            profiles.push_back(profile);
        }
    }
    if (profiles.empty()) {
        profiles.push_back(&kVision);
    }
    std::string botWords;  // the bot check's words in this call: another client refused in them is refused by it
    for (const ClientProfile* profile : profiles) {
        const auto reply =
            parsed<PlayerReply>(post(*profile, "player", {{"videoId", videoId}}, {}, cancel), parsePlayer);
        if (cancel.cancelled()) {
            return std::nullopt;
        }
        if (!reply.ok()) {
            PF_LOG_INFO("youtube: %s player: %s", profile->name, reply.error().message.c_str());
            failure = reply.error();
            setback = true;
            continue;
        }
        if (!reply.value().playable) {
            PF_LOG_INFO("youtube: %s refused: %s", profile->name, reply.value().reason.c_str());
            setback = true;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                // (the mobile clients' refusals have another shape than the one parsePlayer knows)
                const bool bot = reply.value().botCheck ||
                                 (!botWords.empty() && reply.value().reason == botWords);
                if (bot) {
                    botWords = reply.value().reason;
                    restUntil_[profile] = now() + kClientRest;
                    blocked = true;
                } else {
                    restUntil_.erase(profile);
                }
            }
            if (!reason || reason->empty()) {
                reason = reply.value().reason;  // the first words YouTube gave
            }
            continue;
        }
        auto chosen = profile->muxedOnly && !reply.value().live ? muxedStreams(reply.value())
                                                                 : chooseStreams(reply.value(), maxHeight);
        if (chosen && !chosen->video.empty() &&
            (!reachable(chosen->video, profile->userAgent, cancel) ||
             (!chosen->audio.empty() && !reachable(chosen->audio, profile->userAgent, cancel)))) {
            const bool wasMuxed = chosen->audio.empty();
            chosen.reset();
            setback = true;
            if (!wasMuxed) {
                PF_LOG_INFO("youtube: %s streams refused, trying its 360p", profile->name);
                chosen = muxedStreams(reply.value());
                if (chosen && !reachable(chosen->video, profile->userAgent, cancel)) {
                    chosen.reset();
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            restUntil_.erase(profile);  // it answered: the bot check let it through
        }
        if (chosen) {
            chosen->userAgent = profile->userAgent;
            chosen->client = profile->name;
            return chosen;
        }
    }
    return std::nullopt;
}

bool Client::resting(const ClientProfile* profile) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = restUntil_.find(profile);
    return found != restUntil_.end() && now() < found->second;
}

core::Result<Streams> Client::streams(const std::string& videoId, int maxHeight, const std::string& invidious,
                                      const core::CancelToken& cancel) const {
    bool keptVisitor = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        keptVisitor = !visitor_.empty();
    }
    std::optional<std::string> reason;  // set when a client refused (YouTube may give no words)
    std::optional<core::Error> failure;
    bool setback = false;
    bool blocked = false;  // a client was refused by the bot check
    auto chosen = fromClients(videoId, maxHeight, cancel, reason, failure, setback, blocked);
    // YouTube stops honouring a visitor id after some hours: the clients' HD files are refused and some clients
    // refused ("not a robot"); only a muxed 360p may still come. A new id brings the quality back.
    const bool fellBack = chosen && setback && !chosen->live && chosen->audio.empty() && chosen->height < maxHeight;
    bool visitorRested = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        visitorRested = now() >= visitorRestUntil_;
    }
    if (keptVisitor && visitorRested && !cancel.cancelled() && ((!chosen && reason) || fellBack)) {
        if (chosen) {
            PF_LOG_INFO("youtube: %dp only, trying a new visitor id", chosen->height);
        } else {
            PF_LOG_INFO("youtube: every client refused, trying a new visitor id");
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            visitor_.clear();
        }
        std::optional<std::string> againReason;
        std::optional<core::Error> againFailure;
        auto again = fromClients(videoId, maxHeight, cancel, againReason, againFailure, setback, blocked);
        if (again && (!chosen || again->height > chosen->height || !again->audio.empty())) {
            chosen = std::move(again);
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            visitorRestUntil_ = now() + kVisitorRest;  // the id was not the cause: no new one for a while
        }
        if (!chosen) {
            reason = againReason;
            failure = againFailure;
        }
    }
    if (chosen) {
        return std::move(*chosen);
    }
    if (!invidious.empty() && !cancel.cancelled()) {
        const auto reply = parsed<PlayerReply>(
            get(invidious + "/api/v1/videos/" + net::urlEncode(videoId) + "?local=true", kWebAgent, cancel),
            [&](const std::string& body) { return parseInvidious(body, invidious); });
        if (reply.ok()) {
            if (auto fromInvidious = chooseStreams(reply.value(), maxHeight)) {
                fromInvidious->userAgent = kWebAgent;
                fromInvidious->client = "Invidious";
                return std::move(*fromInvidious);
            }
        } else {
            PF_LOG_INFO("youtube: Invidious: %s", reply.error().message.c_str());
        }
    }
    if (cancel.cancelled()) {
        return cancelled();
    }
    if (reason) {
        // YouTube's own words, maybe none
        return core::makeError(blocked ? core::ErrorKind::Blocked : core::ErrorKind::Server, *reason);
    }
    if (failure) {
        return *failure;  // no client answered: the network, not YouTube
    }
    return core::makeError(core::ErrorKind::BadResponse, "no playable stream");
}

core::Result<std::vector<Segment>> Client::segments(const std::string& videoId,
                                                    const std::vector<std::string>& categories,
                                                    const core::CancelToken& cancel) const {
    json::Writer list;
    list.array();
    for (const std::string& category : categories) {
        list.value(category);
    }
    list.end();
    const auto body = get(endpoints_.sponsor + "/api/skipSegments?videoID=" + net::urlEncode(videoId) +
                              "&categories=" + net::urlEncode(list.str()),
                          kWebAgent, cancel);
    if (!body.ok() && body.error().kind == core::ErrorKind::NotFound) {
        return std::vector<Segment>{};  // SponsorBlock's way of saying "none"
    }
    return parsed<std::vector<Segment>>(body, parseSegments);
}

core::Result<Branding> Client::branding(const std::string& videoId, const core::CancelToken& cancel) const {
    return parsed<Branding>(get(endpoints_.dearrow + "/api/branding?videoID=" + net::urlEncode(videoId), kWebAgent,
                                cancel),
                            parseBranding);
}

std::string Client::brandingThumbnail(const std::string& videoId, double time) const {
    // DeArrow keeps its pictures under the time exactly as its API wrote it (JavaScript's shortest form, the
    // fewest digits that give the number back): a rounded time is not found and the server must make the
    // picture again, which it refuses when busy (204).
    char seconds[48] = "0";
    if (std::isfinite(time) && time > 0) {
        bool exact = false;
        for (int digits = 1; digits <= 17 && !exact; ++digits) {
            std::snprintf(seconds, sizeof(seconds), "%.*g", digits, time);
            exact = std::strchr(seconds, 'e') == nullptr && std::strtod(seconds, nullptr) == time;
        }
        if (!exact) {
            std::snprintf(seconds, sizeof(seconds), "%.6f", time);
        }
    }
    return endpoints_.dearrowThumbnails + "/api/v1/getThumbnail?videoID=" + net::urlEncode(videoId) +
           "&time=" + seconds;
}

core::Result<Votes> Client::votes(const std::string& videoId, const core::CancelToken& cancel) const {
    return parsed<Votes>(get(endpoints_.dislikes + "/votes?videoId=" + net::urlEncode(videoId), kWebAgent, cancel),
                         parseVotes);
}

core::Result<VideoPage> Client::feed(const std::string& browseId, const std::string& accessToken,
                                     const core::CancelToken& cancel) const {
    return parsed<VideoPage>(post(kTv, "browse", {{"browseId", browseId}}, accessToken, cancel),
                             parseTiles);
}

core::Result<VideoPage> Client::feedMore(const std::string& continuation, const std::string& accessToken,
                                         const core::CancelToken& cancel) const {
    return parsed<VideoPage>(
        post(kTv, "browse", {{"continuation", continuation}}, accessToken, cancel), parseTiles);
}

core::Result<PlaybackTracking> Client::playbackTracking(const std::string& videoId, const std::string& accessToken,
                                                        const std::string& nonce,
                                                        const core::CancelToken& cancel) const {
    const std::string playback =
        R"({"contentPlaybackContext":{"signatureTimestamp":)" + std::to_string(kTvSignatureTimestamp) + "}}";
    return parsed<PlaybackTracking>(post(kTv, "player", {{"videoId", videoId}, {"cpn", nonce}}, accessToken, cancel,
                                         {{"playbackContext", playback}}),
                                    parsePlaybackTracking);
}

core::Result<core::Unit> Client::report(const std::string& url, const std::string& accessToken,
                                        const core::CancelToken& cancel) const {
    const auto startsWith = [&url](const std::string& prefix) { return url.compare(0, prefix.size(), prefix) == 0; };
    if (!startsWith(endpoints_.stats + "/api/stats/") && !startsWith(endpoints_.youtube + "/api/stats/")) {
        return core::makeError(core::ErrorKind::BadResponse, "report: not YouTube's reporting address");
    }
    if (cancel.cancelled()) {
        return cancelled();
    }
    net::HttpRequest request;
    request.url = url;
    request.maxBodyBytes = 64 * 1024;
    request.headers = {{"User-Agent", kTvAgent}, {"Authorization", "Bearer " + accessToken}};
    const auto body = checked(http_.send(request, {}, cancel), "report");
    if (!body.ok()) {
        return body.error();
    }
    return core::Unit{};
}

core::Result<ChatPage> Client::liveChat(const std::string& continuation, const core::CancelToken& cancel) const {
    return parsed<ChatPage>(post(kWeb, "live_chat/get_live_chat", {{"continuation", continuation}}, "", cancel),
                            parseLiveChat);
}

core::Result<VideoState> Client::videoState(const std::string& videoId, const std::string& accessToken,
                                            const core::CancelToken& cancel) const {
    return parsed<VideoState>(post(kTv, "next", {{"videoId", videoId}}, accessToken, cancel), parseVideoState);
}

core::Result<core::Unit> Client::rate(const std::string& videoId, Rating rating, const std::string& accessToken,
                                      const core::CancelToken& cancel) const {
    const char* endpoint = rating == Rating::Like      ? "like/like"
                           : rating == Rating::Dislike ? "like/dislike"
                                                       : "like/removelike";
    json::Writer target;
    target.object().key("videoId").value(videoId).end();
    const auto body = post(kTv, endpoint, {}, accessToken, cancel, {{"target", target.str()}});
    if (!body.ok()) {
        return body.error();
    }
    return core::Unit{};
}

core::Result<core::Unit> Client::setSubscribed(const std::string& channelId, bool subscribed,
                                               const std::string& accessToken, const core::CancelToken& cancel) const {
    json::Writer ids;
    ids.array().value(channelId).end();
    const auto body = post(kTv, subscribed ? "subscription/subscribe" : "subscription/unsubscribe", {}, accessToken,
                           cancel, {{"channelIds", ids.str()}});
    if (!body.ok()) {
        return body.error();
    }
    return core::Unit{};
}

core::Result<std::vector<PlaylistChoice>> Client::playlistChoices(const std::string& videoId,
                                                                  const std::string& accessToken,
                                                                  const core::CancelToken& cancel) const {
    json::Writer ids;
    ids.array().value(videoId).end();
    return parsed<std::vector<PlaylistChoice>>(
        post(kTv, "playlist/get_add_to_playlist", {}, accessToken, cancel, {{"videoIds", ids.str()}}),
        parseAddToPlaylist);
}

core::Result<core::Unit> Client::addToPlaylist(const std::string& playlistId, const std::string& videoId,
                                               const std::string& accessToken, const core::CancelToken& cancel) const {
    json::Writer action;
    action.array().object().key("addedVideoId").value(videoId).key("action").value("ACTION_ADD_VIDEO").end().end();
    const auto body = post(kTv, "browse/edit_playlist", {{"playlistId", playlistId}}, accessToken, cancel,
                           {{"actions", action.str()}});
    if (!body.ok()) {
        return body.error();
    }
    const auto doc = json::Document::parse(body.value());
    if (!doc || json::string(doc->root(), "status") != "STATUS_SUCCEEDED") {
        return core::makeError(core::ErrorKind::BadResponse, "edit_playlist: not added");
    }
    return core::Unit{};
}

core::Result<std::string> Client::accountName(const std::string& accessToken, const core::CancelToken& cancel) const {
    const auto body = post(kTv, "account/accounts_list", {}, accessToken, cancel);
    if (!body.ok()) {
        return body.error();
    }
    std::string name = parseAccountName(body.value());
    if (name.empty()) {
        return core::makeError(core::ErrorKind::BadResponse, "accounts_list: no name");
    }
    return name;
}

}  // namespace playtube::youtube
