#pragma once

#include <string>
#include <vector>

#include "youtube/models.hpp"

namespace playtube::app {

// A file that ends with more than this left to play did not end: its stream was cut (a connection YouTube closed).
constexpr double kCutShortSeconds = 10;

// The player was told the file ended at `position` of `duration`: true when that is a cut stream, to start again
// from there, not the video's end. `lastCutAt`: where the previous cut was (negative: none yet). A second cut
// without 5 s played since is taken for the end (a video whose length YouTube overstates must not loop).
bool streamCutShort(double position, double duration, double lastCutAt);

// What plays after a video, from its related videos: the first that is a video (not a live, a playlist or a
// channel, nor a Short unless they are shown) and that was not played in this session (`played`); when every one
// was, the first that may play.
const youtube::Video* nextToPlay(const std::vector<youtube::Video>& related, bool shorts,
                                 const std::vector<std::string>& played);

// A video starts: remembered as played in this session (the last kMaxPlayed, the latest last).
constexpr std::size_t kMaxPlayed = 200;
void rememberPlayed(std::vector<std::string>& played, const std::string& id);

}  // namespace playtube::app
