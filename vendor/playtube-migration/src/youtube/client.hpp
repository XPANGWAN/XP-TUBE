#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/result.hpp"
#include "core/task.hpp"
#include "net/http.hpp"
#include "youtube/models.hpp"
#include "youtube/streams.hpp"

namespace playtube::youtube {

struct ClientProfile;

// Largest reply accepted (a search page is about 1 MiB).
constexpr std::size_t kMaxReplyBytes = 8u * 1024u * 1024u;

struct Endpoints {
    std::string youtube = "https://www.youtube.com";
    std::string suggest = "https://clients1.google.com";
    std::string sponsor = "https://sponsor.ajay.app";
    std::string oauthRevoke = "https://oauth2.googleapis.com";
    std::string dislikes = "https://returnyoutubedislikeapi.com";  // Return YouTube Dislike
    std::string dearrow = "https://sponsor.ajay.app";               // DeArrow's titles and thumbnails (optional)
    std::string dearrowThumbnails = "https://dearrow-thumb.ajay.app";
    std::string stats = "https://s.youtube.com";  // reports of watching (PlaybackTracking)
};

// YouTube's private API (InnerTube), with each client's own headers. Blocking: call from worker threads. Replies
// in `hl` (language, "fr") for the region `gl` ("FR").
class Client {
public:
    Client(const net::HttpClient& http, Endpoints endpoints, std::string hl, std::string gl);

    // `params`: the filters (searchParams); empty for a plain search
    core::Result<VideoPage> search(const std::string& query, const core::CancelToken& cancel = {},
                                   const std::string& params = {}) const;
    core::Result<VideoPage> searchMore(const std::string& continuation, const core::CancelToken& cancel = {}) const;
    core::Result<std::vector<std::string>> suggestions(const std::string& text,
                                                       const core::CancelToken& cancel = {}) const;
    core::Result<ChannelPage> channel(const std::string& channelId, const core::CancelToken& cancel = {}) const;
    core::Result<VideoPage> channelMore(const std::string& continuation, const core::CancelToken& cancel = {}) const;
    // A channel's other tabs (their next pages: channelMore)
    core::Result<VideoPage> channelTab(const std::string& channelId, ChannelTab tab,
                                       const core::CancelToken& cancel = {}) const;
    core::Result<NextReply> next(const std::string& videoId, const core::CancelToken& cancel = {}) const;
    // A page of a live video's chat (NextReply::liveChat, then each page's continuation).
    core::Result<ChatPage> liveChat(const std::string& continuation, const core::CancelToken& cancel = {}) const;
    // A page of comments (or of a comment's replies) from a continuation of NextReply or of an earlier page.
    core::Result<CommentPage> comments(const std::string& continuation, const core::CancelToken& cancel = {}) const;
    // A channel's playlists ("Playlists" tab) and their next pages.
    core::Result<VideoPage> channelPlaylists(const std::string& channelId, const core::CancelToken& cancel = {}) const;
    core::Result<VideoPage> channelPlaylistsMore(const std::string& continuation,
                                                 const core::CancelToken& cancel = {}) const;
    // A playlist's videos (TV client); the token may be empty for a public playlist. Next pages: feedMore.
    core::Result<VideoPage> playlist(const std::string& playlistId, const std::string& accessToken,
                                     const core::CancelToken& cancel = {}) const;

    // Tries VISIONOS, ANDROID_VR (a client whose separate streams are refused gives its muxed 360p), IOS and
    // ANDROID (muxed 360p only), then the Invidious instance when one is set. When every client refuses with a
    // visitor id kept from an earlier call, a new id is tried once. The error carries YouTube's reason when they
    // refuse (ErrorKind::Blocked when it is the bot check), the network error when none answered.
    // A connection flagged by the bot check is refused whatever the video, and asking again feeds the flag: a
    // client it refused rests for kClientRest (when all rest, the first is asked alone), and a new visitor id that
    // did not help is not tried again for kVisitorRest.
    core::Result<Streams> streams(const std::string& videoId, int maxHeight, const std::string& invidious = {},
                                  const core::CancelToken& cancel = {}) const;
    // SponsorBlock segments of these categories; none when the video has none.
    core::Result<std::vector<Segment>> segments(const std::string& videoId, const std::vector<std::string>& categories,
                                                const core::CancelToken& cancel = {}) const;

    // Signed-in feeds (TV client): "FEwhat_to_watch", "FEsubscriptions", "FEhistory", "VLWL",
    // "FEplaylist_aggregation" (the account's playlists). An expired token gives ErrorKind::Unauthorized; feedMore
    // takes an empty token for a public playlist's pages.
    // DeArrow's title and thumbnail for a video (empty when it has none)
    core::Result<Branding> branding(const std::string& videoId, const core::CancelToken& cancel = {}) const;
    // The address of DeArrow's thumbnail: the video's frame at `time` (WebP)
    std::string brandingThumbnail(const std::string& videoId, double time) const;
    // Return YouTube Dislike's estimate of a video's votes
    core::Result<Votes> votes(const std::string& videoId, const core::CancelToken& cancel = {}) const;
    core::Result<VideoPage> feed(const std::string& browseId, const std::string& accessToken,
                                 const core::CancelToken& cancel = {}) const;
    core::Result<VideoPage> feedMore(const std::string& continuation, const std::string& accessToken,
                                     const core::CancelToken& cancel = {}) const;
    core::Result<std::string> accountName(const std::string& accessToken, const core::CancelToken& cancel = {}) const;
    // Where to report watching this video for the account (the TV client's player reply). `nonce`: the playback's
    // (playbackNonce), the same as in its reports. The request carries the TV player's own signature timestamp
    // (kTvSignatureTimestamp): without it YouTube answers UNPLAYABLE and no address.
    core::Result<PlaybackTracking> playbackTracking(const std::string& videoId, const std::string& accessToken,
                                                    const std::string& nonce,
                                                    const core::CancelToken& cancel = {}) const;
    // `signatureTimestamp:` of YouTube's TV player script (tv-player-ias-tcl.js), 8 digits; the other players' is
    // 5 digits and is refused here. An older value keeps being accepted for a while: renewed with the TV client's
    // version.
    static constexpr long kTvSignatureTimestamp = 20760001;
    // Sends one report (playbackReportUrl, watchtimeReportUrl) with the account's token. Refused unless the address
    // is YouTube's own reporting service over https: the token goes nowhere else.
    core::Result<core::Unit> report(const std::string& url, const std::string& accessToken,
                                    const core::CancelToken& cancel = {}) const;
    // The account's rating of a video, its likes and whether it is subscribed to the video's channel.
    core::Result<VideoState> videoState(const std::string& videoId, const std::string& accessToken,
                                        const core::CancelToken& cancel = {}) const;
    core::Result<core::Unit> rate(const std::string& videoId, Rating rating, const std::string& accessToken,
                                  const core::CancelToken& cancel = {}) const;
    core::Result<core::Unit> setSubscribed(const std::string& channelId, bool subscribed,
                                           const std::string& accessToken, const core::CancelToken& cancel = {}) const;
    // "Watch later" and the account's playlists, marked when they already hold the video.
    core::Result<std::vector<PlaylistChoice>> playlistChoices(const std::string& videoId,
                                                              const std::string& accessToken,
                                                              const core::CancelToken& cancel = {}) const;
    // Adds the video at the end of the playlist ("WL": watch later).
    core::Result<core::Unit> addToPlaylist(const std::string& playlistId, const std::string& videoId,
                                           const std::string& accessToken, const core::CancelToken& cancel = {}) const;

    const Endpoints& endpoints() const { return endpoints_; }

    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    void setClock(Clock clock) { clock_ = std::move(clock); }  // tests
    static constexpr std::chrono::minutes kClientRest{10};
    static constexpr std::chrono::minutes kVisitorRest{30};

private:
    using Fields = std::vector<std::pair<const char*, std::string>>;
    // `json`: fields whose values are already JSON (arrays, objects)
    core::Result<std::string> post(const ClientProfile& profile, const char* endpoint, const Fields& fields,
                                   const std::string& accessToken, const core::CancelToken& cancel,
                                   const Fields& json = {}) const;
    core::Result<std::string> get(const std::string& url, const std::string& userAgent,
                                  const core::CancelToken& cancel) const;
    std::string visitorData(const core::CancelToken& cancel) const;
    // `setback`: a client before the chosen one failed (refused, its files refused, no answer); `blocked` is set
    // when the bot check refused one
    std::optional<Streams> fromClients(const std::string& videoId, int maxHeight, const core::CancelToken& cancel,
                                       std::optional<std::string>& reason, std::optional<core::Error>& failure,
                                       bool& setback, bool& blocked) const;
    bool reachable(const std::string& url, const std::string& userAgent, const core::CancelToken& cancel) const;

    const net::HttpClient& http_;
    Endpoints endpoints_;
    std::string hl_;
    std::string gl_;
    mutable std::mutex mutex_;
    mutable std::string visitor_;
    using Time = std::chrono::steady_clock::time_point;
    Time now() const { return clock_ ? clock_() : std::chrono::steady_clock::now(); }
    bool resting(const ClientProfile* profile) const;
    Clock clock_;
    mutable std::map<const ClientProfile*, Time> restUntil_;  // clients the bot check refused (under mutex_)
    mutable Time visitorRestUntil_{};                         // no new visitor id before (under mutex_)
};

}  // namespace playtube::youtube
