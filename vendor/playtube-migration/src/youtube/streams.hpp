#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "youtube/models.hpp"

namespace playtube::youtube {

// An audio language YouTube offers for a video (original or dubbed).
struct AudioTrack {
    std::string id;
    std::string name;
    std::string url;
    bool original = false;
};

// What the player opens: an H.264 video with a separate AAC audio, a muxed 360p file (audio empty), or an HLS
// playlist (live).
struct Streams {
    std::string video;
    std::string audio;
    std::string hls;
    std::string userAgent;  // the files only answer to the client that asked for them
    std::string client;     // "VISIONOS", "ANDROID_VR", ..., "Invidious"
    std::string title;      // the video's, from the same reply
    std::string channelId;
    int height = 0;
    bool live = false;
    std::vector<CaptionTrack> captions;
    std::vector<AudioTrack> audioTracks;  // when there are several languages: the original first, then by name
    std::string audioTrack;               // the id of the one in `audio` ("" with a single language)
    std::string storyboardSpec;           // the seek preview's pictures (youtube::parseStoryboard)
    int durationSeconds = 0;
};

// The highest H.264 video at most `maxHeight` high (else the smallest; 30 fps rather than 60 at the same height) with
// the original AAC audio and the other languages offered; the muxed 360p when there
// are no separate streams; the HLS playlist for a live video. Empty when nothing playable is offered.
std::optional<Streams> chooseStreams(const PlayerReply& reply, int maxHeight);
// The muxed 360p alone (itag 18), for when the separate streams are refused.
std::optional<Streams> muxedStreams(const PlayerReply& reply);

// When the streams' addresses stop working (Unix seconds): the earliest "expire" of the video, audio and HLS
// addresses ("expire=…" in a query, "/expire/…/" in a path-style HLS address); 0 when none says.
std::int64_t expiresAt(const Streams& streams);

}  // namespace playtube::youtube
