#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "youtube/models.hpp"

namespace playtube::youtube {

// The SponsorBlock segment to skip at `position`: the one the position just entered (from its start to half a
// second before its end; shorter than a second: never), not skipped yet (`skipped[i]`, same order).
std::optional<std::size_t> segmentToSkip(double position, const std::vector<Segment>& segments,
                                         const std::vector<bool>& skipped);

// The playback speeds offered: 0.5× to 2×.
const std::vector<double>& playbackSpeeds();

// Reporting watching to the account (PlaybackTracking). A playback's nonce ("cpn"): 16 characters of the URL-safe
// base64 alphabet, from 16 random bytes; the same for every report of one playback.
std::string playbackNonce(const unsigned char (&random)[16]);
// The report that a playback started at `position` (seconds): the playback address with its version, nonce and time.
std::string playbackReportUrl(const std::string& base, const std::string& nonce, double position);
// The report of the part watched, `from` to `to` (seconds), and whether the video is paused now.
std::string watchtimeReportUrl(const std::string& base, const std::string& nonce, double from, double to,
                               bool paused);

// A caption track's address as WebVTT (YouTube's timedtext answers its own XML by default).
std::string captionUrl(const CaptionTrack& track);

}  // namespace playtube::youtube
