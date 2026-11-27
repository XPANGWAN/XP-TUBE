#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace playtube::youtube {

struct Video {
    std::string id;
    std::string title;
    std::string channelName;
    std::string channelId;      // empty when the reply does not carry it (TV tiles, related videos)
    std::string viewsText;      // as YouTube writes it ("1,2 M de vues", "824 spectateurs")
    std::string publishedText;  // "il y a 3 jours"
    std::string durationText;   // "24:18"; empty for live videos
    std::string thumbnail;      // https, at most about 720 px wide
    int durationSeconds = 0;
    bool live = false;
    bool playlist = false;       // a playlist: `id` is the playlist's ("PL…", "WL"), no duration
    std::string videoCountText;  // a playlist's size as YouTube writes it ("24 vidéos")
    int percentWatched = -1;     // how much the account has watched (0 to 100, YouTube's), -1 unknown
    bool isShort = false;        // a Short: upright, played in the Shorts player; `thumbnail` is upright too
    bool isChannel = false;      // a channel (search results): `id` its "UC…", `thumbnail` its avatar
};

enum class ChannelTab { Videos, Shorts, Lives };

// DeArrow's title and thumbnail for a video, chosen by its users instead of the clickbait ones. An empty title or a
// negative time: keep YouTube's.
struct Branding {
    std::string title;
    double thumbnailTime = -1;  // seconds into the video: the frame DeArrow's thumbnail server draws
};

// Return YouTube Dislike's counts of a video (YouTube itself shows no dislikes)
struct Votes {
    std::int64_t likes = 0;
    std::int64_t dislikes = 0;
};

struct VideoPage {
    std::vector<Video> items;
    std::string continuation;  // empty on the last page
};

struct Channel {
    std::string id;
    std::string name;
    std::string handle;           // "@LofiGirl"
    std::string subscribersText;  // "15,8 M d'abonnés"
    std::string avatar;
    std::string banner;
    std::string description;
};

struct ChannelPage {
    Channel channel;
    VideoPage videos;
};

// The video page: what plays next and who published it.
// A part of a video its uploader (or YouTube) named: where it starts and its title.
struct Chapter {
    double start = 0;
    std::string title;
};

struct NextReply {
    std::vector<Video> related;
    Channel owner;
    std::string commentsContinuation;  // the first page of comments; empty when they are off
    std::vector<Chapter> chapters;     // in order, the first at 0; empty when the video has none
    std::string liveChat;              // a live video's chat: its first page (Client::liveChat); else empty
};

// A message of a live chat. Emoji are left out of `text` (the fonts have none): a message of emoji only is empty.
struct ChatMessage {
    std::string id;
    std::string author;  // "@name"
    std::string text;
    std::string amount;  // a Super Chat's amount ("5,00 €"); empty for a plain message
};

// A page of a live chat: the new messages, the ids of those removed by the moderators, the next page and how long
// to wait before asking it (YouTube's own figure, bounded by the caller).
struct ChatPage {
    std::vector<ChatMessage> messages;
    std::vector<std::string> removed;
    std::string continuation;
    int waitMs = 0;
};

struct Comment {
    std::string id;
    std::string author;         // "@name"
    std::string avatar;
    std::string text;           // may hold line breaks
    std::string publishedText;  // "il y a 6 ans (modifié)"
    std::string likesText;      // "104 k"; empty when none
    std::string repliesText;    // the number of replies as YouTube writes it ("611"); empty when none
    std::string repliesContinuation;
};

struct CommentPage {
    std::vector<Comment> items;
    std::string continuation;  // empty on the last page
};

enum class Rating { None, Like, Dislike };

// The signed-in account's view of a video.
struct VideoState {
    Rating rating = Rating::None;
    std::string likesText;  // "19 M"
    bool subscribed = false;
    std::string channelId;
};

// A playlist a video can be saved to.
struct PlaylistChoice {
    std::string id;  // "WL" for "watch later"
    std::string title;
    bool contains = false;  // the video is already in it
};

struct Format {
    int itag = 0;
    int height = 0;  // 0 for audio
    std::string url;
    std::string mimeType;
    long long contentLength = 0;
    bool defaultAudio = true;  // false for a dubbed or described audio track
    std::string audioTrackId;    // "en-US.4", "fr.3": set when YouTube offers several languages
    std::string audioTrackName;  // in the reply's language: "Anglais (US) original", "Allemand"
    bool drc = false;            // a volume-levelled copy of an audio stream
};

struct CaptionTrack {
    std::string language;  // "fr", "en"
    std::string name;      // "Français", "Anglais (générés automatiquement)"
    std::string url;
    bool automatic = false;
};

struct PlayerReply {
    bool playable = false;
    std::string reason;  // why YouTube refused, in the requested language
    // Refused by the bot check ("Sign in to confirm you're not a bot"): the connection is flagged for a while,
    // whatever the video. YouTube gives no code for it: LOGIN_REQUIRED with a reason and nothing else (an age gate
    // carries desktopLegacyAgeGateReason, a private video `messages`, the others an errorScreen).
    bool botCheck = false;
    std::string title;
    std::string channelId;
    int durationSeconds = 0;
    bool live = false;
    std::vector<Format> formats;  // direct URLs only (no signature to decipher)
    std::string hls;              // HLS master playlist, when offered
    std::vector<CaptionTrack> captions;
    std::string storyboardSpec;  // storyboards.playerStoryboardSpecRenderer.spec: the seek preview's pictures
};


// Where YouTube's apps report watching for a signed-in account (the player reply's `playbackTracking`)
struct PlaybackTracking {
    std::string playbackUrl;   // once, when the video starts: the video goes into the account's history
    std::string watchtimeUrl;  // then each part watched: the position YouTube resumes from elsewhere
};

struct Segment {
    double start = 0;
    double end = 0;
    std::string category;  // "sponsor", "selfpromo", "interaction", "intro", "outro", "music_offtopic"
};

struct DeviceCode {
    std::string deviceCode;
    std::string userCode;         // shown to the user
    std::string verificationUrl;  // where to type it
    int interval = 5;             // seconds between polls
    int expiresIn = 1800;
};

struct Token {
    std::string accessToken;
    std::string refreshToken;  // only in the first reply of the device flow
    int expiresIn = 0;
    bool pending = false;   // the user has not entered the code yet
    bool slowDown = false;  // pending, and polling too often
};

}  // namespace playtube::youtube
