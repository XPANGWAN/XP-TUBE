// Development check of the YouTube library against the real services (Linux only): one line per call.
// usage: playtube_yt_check [--old-config ~/.config/Playtube/config.json]   (signed-in feeds with that sign-in)
//                          [--mark-watched VIDEO_ID]   (with --old-config: reports watching it, then reads the history)
// Read only unless --mark-watched: nothing is rated, subscribed or saved (that would change the account).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

#include "net/http.hpp"
#include "youtube/account.hpp"
#include "youtube/auth.hpp"
#include "youtube/client.hpp"
#include "youtube/playback.hpp"
#include "youtube/search_filters.hpp"
#include "youtube/storyboard.hpp"

using namespace playtube;
using namespace playtube::youtube;

namespace {

int failures = 0;

void report(bool ok, const std::string& what, const std::string& detail) {
    std::printf("%s %-12s %s\n", ok ? "ok  " : "FAIL", what.c_str(), detail.c_str());
    failures += ok ? 0 : 1;
}

template <typename T>
bool check(const core::Result<T>& result, const char* what) {
    if (!result.ok()) {
        report(false, what, result.error().message);
    }
    return result.ok();
}

// The first kilobyte of a stream, as the player will ask for it.
std::string probe(const net::HttpClient& http, const std::string& url, const std::string& userAgent) {
    net::HttpRequest request;
    request.url = url;
    request.maxBodyBytes = 64 * 1024;
    request.headers = {{"User-Agent", userAgent}, {"Range", "bytes=0-1023"}};
    const auto response = http.send(request);
    return response.ok() ? "HTTP " + std::to_string(response.value().status) + ", " +
                               std::to_string(response.value().body.size()) + " bytes"
                         : response.error().message;
}

bool fetched(const std::string& probeText) { return probeText.rfind("HTTP 20", 0) == 0; }

}  // namespace

int main(int argc, char** argv) {
    std::string oldConfig;
    std::string markWatched;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--old-config" && i + 1 < argc) {
            oldConfig = argv[++i];
        } else if (std::string(argv[i]) == "--mark-watched" && i + 1 < argc) {
            markWatched = argv[++i];
        } else {
            std::fprintf(stderr, "usage: playtube_yt_check [--old-config FILE] [--mark-watched VIDEO_ID]\n");
            return 2;
        }
    }
    net::globalInit();
    net::HttpConfig config;
    config.caFile = PLAYTUBE_CA_FILE;
    const net::HttpClient http(config);
    const Client client(http, Endpoints{}, "fr", "FR");

    std::string videoId;
    std::string liveId;
    std::string channelId;
    const auto search = client.search("lofi hip hop");
    if (check(search, "search")) {
        for (const Video& v : search.value().items) {
            std::string& slot = v.live ? liveId : videoId;
            if (slot.empty()) {
                slot = v.id;
            }
            if (channelId.empty()) {
                channelId = v.channelId;
            }
        }
        report(!search.value().items.empty() && !search.value().continuation.empty(), "search",
               std::to_string(search.value().items.size()) + " videos, live " + liveId);
        const auto more = client.searchMore(search.value().continuation);
        if (check(more, "search more")) {
            report(!more.value().items.empty(), "search more", std::to_string(more.value().items.size()) + " videos");
        }
    }
    {  // the filters: Shorts only; the most viewed videos of the week
        SearchFilters shortsOnly;
        shortsOnly.type = SearchFilters::Type::Short;
        const auto shorts = client.search("lofi", {}, searchParams(shortsOnly));
        if (check(shorts, "search shorts")) {
            const auto& items = shorts.value().items;
            const bool all = !items.empty() && std::all_of(items.begin(), items.end(), [](const Video& v) {
                return v.isShort;
            });
            report(all, "search shorts", std::to_string(items.size()) + " Shorts");
        }
        SearchFilters week;
        week.sort = SearchFilters::Sort::Views;
        week.upload = SearchFilters::Upload::Week;
        week.type = SearchFilters::Type::Video;
        const auto popular = client.search("lofi", {}, searchParams(week));
        if (check(popular, "search filtered")) {
            report(!popular.value().items.empty(), "search filtered",
                   std::to_string(popular.value().items.size()) + " videos of the week, most viewed first");
        }
    }
    if (!channelId.empty()) {
        const auto channel = client.channel(channelId);
        if (check(channel, "channel")) {
            report(!channel.value().channel.name.empty() && !channel.value().videos.items.empty(), "channel",
                   channel.value().channel.name + ", " + std::to_string(channel.value().videos.items.size()) +
                       " videos");
            const auto more = client.channelMore(channel.value().videos.continuation);
            if (check(more, "channel more")) {
                report(!more.value().items.empty(), "channel more",
                       std::to_string(more.value().items.size()) + " videos");
            }
        }
        for (const auto tab : {ChannelTab::Shorts, ChannelTab::Lives}) {
            const char* name = tab == ChannelTab::Shorts ? "channel shorts" : "channel lives";
            const auto page = client.channelTab(channelId, tab);
            if (check(page, name)) {
                report(true, name, std::to_string(page.value().items.size()) + " items");
            }
        }
        const auto playlists = client.channelPlaylists(channelId);
        if (check(playlists, "playlists")) {
            report(!playlists.value().items.empty(), "playlists",
                   std::to_string(playlists.value().items.size()) + " of the channel");
            if (!playlists.value().items.empty()) {
                const auto videos = client.playlist(playlists.value().items.front().id, {});
                if (check(videos, "playlist")) {
                    report(!videos.value().items.empty(), "playlist",
                           std::to_string(videos.value().items.size()) + " videos, more " +
                               (videos.value().continuation.empty() ? "no" : "yes"));
                }
            }
        }
    }
    if (!videoId.empty()) {
        const auto next = client.next(videoId);
        if (check(next, "next")) {
            report(!next.value().related.empty(), "next",
                   std::to_string(next.value().related.size()) + " related, by " + next.value().owner.name);
            const auto comments = client.comments(next.value().commentsContinuation);
            if (check(comments, "comments")) {
                const auto& items = comments.value().items;
                std::string replies = "no replies";
                for (const Comment& comment : items) {
                    if (!comment.repliesContinuation.empty()) {
                        const auto page = client.comments(comment.repliesContinuation);
                        replies = page.ok() ? std::to_string(page.value().items.size()) + " replies" : "replies failed";
                        break;
                    }
                }
                report(!items.empty() && !comments.value().continuation.empty(), "comments",
                       std::to_string(items.size()) + " comments, " + replies);
            }
        }
    }
    for (const char* id : {"dQw4w9WgXcQ", "jNQXAC9IVRw"}) {
        const auto streams = client.streams(id, 1080);
        if (check(streams, "streams")) {
            const Streams& s = streams.value();
            const std::string video = probe(http, s.video, s.userAgent);
            const std::string audio = s.audio.empty() ? "muxed" : probe(http, s.audio, s.userAgent);
            report(fetched(video) && (s.audio.empty() || fetched(audio)), "streams",
                   std::string(id) + " " + s.client + " " + std::to_string(s.height) + "p, video " + video +
                       ", audio " + audio + ", " + std::to_string(s.captions.size()) + " captions");            if (std::string(id) == "dQw4w9WgXcQ") {  // Return YouTube Dislike's counts
                const auto votes = client.votes(id);
                if (check(votes, "dislikes")) {
                    report(votes.value().dislikes > 0, "dislikes",
                           std::to_string(votes.value().likes) + " likes, " + std::to_string(votes.value().dislikes) +
                               " dislikes");
                }
            }
            if (std::string(id) == "dQw4w9WgXcQ") {  // the seek preview's pictures
                const auto board = parseStoryboard(s.storyboardSpec, s.durationSeconds);
                const std::string sheet = board ? probe(http, board->sheetUrl(0), s.userAgent) : "no storyboard";
                report(board.has_value() && fetched(sheet), "storyboard",
                       board ? std::to_string(board->width) + "x" + std::to_string(board->height) + ", " +
                                   std::to_string(board->count) + " pictures, sheet " + sheet
                             : sheet);
            }
        }
    }
    if (!liveId.empty()) {
        const auto live = client.streams(liveId, 1080);
        if (check(live, "live")) {
            const std::string hls = probe(http, live.value().hls, live.value().userAgent);
            report(live.value().live && fetched(hls), "live", live.value().client + ", playlist " + hls);
        }
        const auto page = client.next(liveId);
        if (check(page, "live chat")) {
            const auto chat = client.liveChat(page.value().liveChat);
            if (check(chat, "live chat")) {
                const auto more = client.liveChat(chat.value().continuation);
                report(!chat.value().continuation.empty() && more.ok(), "live chat",
                       std::to_string(chat.value().messages.size()) + " messages, next page in " +
                           std::to_string(chat.value().waitMs) + " ms");
            }
        }
    }
    const auto suggestions = client.suggestions("lofi");
    if (check(suggestions, "suggestions")) {
        report(!suggestions.value().empty(), "suggestions", std::to_string(suggestions.value().size()) + " entries");
    }
    const auto segments = client.segments("dQw4w9WgXcQ", {"sponsor", "selfpromo", "interaction", "intro", "outro",
                                                          "music_offtopic"});
    if (check(segments, "sponsorblock")) {
        report(true, "sponsorblock", std::to_string(segments.value().size()) + " segments");
    }

    if (!oldConfig.empty()) {
        const Auth auth(http, Endpoints{});
        Account account("");  // in memory only; never signed out here: that would revoke the previous app's sign-in
        report(account.importFromOld(oldConfig), "account", "sign-in imported");
        const std::int64_t now = std::time(nullptr);
        const auto name = account.withToken(auth, now, [&](const std::string& token) {
            return client.accountName(token);
        });
        if (check(name, "account")) {
            report(true, "account", "name of " + std::to_string(name.value().size()) + " bytes");
        }
        const auto state = account.withToken(auth, now, [&](const std::string& token) {
            return client.videoState("dQw4w9WgXcQ", token);
        });
        if (check(state, "rating")) {
            report(!state.value().channelId.empty(), "rating",
                   "likes " + state.value().likesText + ", subscription read");
        }
        const auto choices = account.withToken(auth, now, [&](const std::string& token) {
            return client.playlistChoices("dQw4w9WgXcQ", token);
        });
        if (check(choices, "save to")) {
            report(!choices.value().empty(), "save to", std::to_string(choices.value().size()) + " playlists");
        }
        const auto tracking = account.withToken(auth, now, [&](const std::string& token) {
            return client.playbackTracking("dQw4w9WgXcQ", token, "playtubeCheck000");
        });
        if (check(tracking, "tracking")) {
            report(!tracking.value().watchtimeUrl.empty(), "tracking", "playback and watch time addresses");
        }
        if (!markWatched.empty()) {
            const auto marked = account.withToken(auth, now, [&](const std::string& token) {
                const unsigned char random[16] = {static_cast<unsigned char>(now), 7, 13, 21, 34, 55, 89, 144, 233,
                                                  17, 99, 3, 250, 61, 42, static_cast<unsigned char>(now >> 8)};
                const std::string nonce = youtube::playbackNonce(random);
                auto where = client.playbackTracking(markWatched, token, nonce);
                if (!where.ok()) {
                    return core::Result<core::Unit>(where.error());
                }
                auto started = client.report(youtube::playbackReportUrl(where.value().playbackUrl, nonce, 0), token);
                if (!started.ok() || where.value().watchtimeUrl.empty()) {
                    return started;
                }
                return client.report(youtube::watchtimeReportUrl(where.value().watchtimeUrl, nonce, 0, 30, false),
                                     token);
            });
            if (check(marked, "mark watched")) {
                const auto history = account.withToken(auth, now, [&](const std::string& token) {
                    return client.feed("FEhistory", token);
                });
                if (check(history, "mark watched")) {
                    const auto& items = history.value().items;
                    report(!items.empty() && items.front().id == markWatched, "mark watched",
                           items.empty() ? "empty history" : "history starts with " + items.front().id);
                }
            }
        }
        for (const char* feed : {"FEwhat_to_watch", "FEsubscriptions", "FEhistory", "VLWL", "FEplaylist_aggregation"}) {
            const auto page = account.withToken(auth, now, [&](const std::string& token) {
                return client.feed(feed, token);
            });
            if (check(page, feed)) {
                report(!page.value().items.empty(), feed, std::to_string(page.value().items.size()) + " videos");
            }
        }
    }
    std::printf("%s\n", failures == 0 ? "all good" : (std::to_string(failures) + " failed").c_str());
    return failures == 0 ? 0 : 1;
}
