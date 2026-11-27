#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "youtube/models.hpp"

namespace playtube::youtube {

// Pure parsers of YouTube's replies. Unknown renderers are skipped: a reply YouTube changed gives what is still
// understood. Bounded: at most kMaxItems items per page, nesting deeper than kMaxDepth is ignored.
constexpr std::size_t kMaxItems = 200;
constexpr int kMaxDepth = 96;

// WEB search, first page or continuation: videos and playlists (mixes left out), in their order
core::Result<VideoPage> parseSearch(std::string_view json);
core::Result<ChannelPage> parseChannel(std::string_view json);  // WEB browse of a channel's videos tab
core::Result<VideoPage> parseChannelMore(std::string_view json);  // its continuation
core::Result<NextReply> parseNext(std::string_view json);      // WEB next
core::Result<PlayerReply> parsePlayer(std::string_view json);  // any client's player reply
// TVHTML5 browse (signed-in feeds, playlists, a playlist's videos) or continuation; playlist tiles become
// playlist items (mixes left out)
core::Result<VideoPage> parseTiles(std::string_view json);
// WEB next of a comment section's continuation: a page of comments, or of a comment's replies
core::Result<CommentPage> parseComments(std::string_view json);
core::Result<ChatPage> parseLiveChat(std::string_view json);  // WEB live_chat/get_live_chat
core::Result<VideoState> parseVideoState(std::string_view json);           // TVHTML5 next, signed in
core::Result<std::vector<PlaylistChoice>> parseAddToPlaylist(std::string_view json);
core::Result<VideoPage> parseChannelPlaylists(std::string_view json);  // a channel's playlists tab or its next page
core::Result<std::vector<std::string>> parseSuggestions(std::string_view json);
core::Result<std::vector<Segment>> parseSegments(std::string_view json);  // SponsorBlock
core::Result<Votes> parseVotes(std::string_view json);
core::Result<Branding> parseBranding(std::string_view json);  // DeArrow's /api/branding                     // Return YouTube Dislike
core::Result<PlaybackTracking> parsePlaybackTracking(std::string_view json);  // a player reply's
std::string parseVisitorData(std::string_view swJsData);  // "" when absent
std::string parseAccountName(std::string_view json);      // "" when absent
// Invidious /api/v1/videos/<id>; relative caption URLs are joined to `instance`.
core::Result<PlayerReply> parseInvidious(std::string_view json, const std::string& instance);
core::Result<DeviceCode> parseDeviceCode(std::string_view json);
core::Result<Token> parseToken(std::string_view json);

// A video thumbnail's address that answers JPEG: without its signed query (which answers WebP), hq720.jpg kept,
// any other name replaced by hqdefault.jpg (every video has one); a localised one (/vi_lc/, only signed) becomes
// the original's. Other addresses unchanged.
std::string jpegThumbnail(const std::string& url);

// "1:02:26" -> 3746, "8:00" -> 480; 0 when not a duration.
int parseDuration(std::string_view text);

}  // namespace playtube::youtube
