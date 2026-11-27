#include <doctest/doctest.h>

#include <vector>

#include "app/shell/chrome.hpp"
#include "youtube/playback.hpp"

using namespace playtube;
using namespace playtube::youtube;

TEST_CASE("SponsorBlock: a segment is skipped once, when the position enters it") {
    const std::vector<Segment> segments{{0, 5, "intro"}, {60, 90, "sponsor"}, {120, 120.3, "selfpromo"}};
    std::vector<bool> skipped(segments.size(), false);
    CHECK(segmentToSkip(0.2, segments, skipped) == 0u);
    skipped[0] = true;
    CHECK_FALSE(segmentToSkip(2, segments, skipped));  // sought back into it: not again
    CHECK_FALSE(segmentToSkip(59.9, segments, skipped));
    CHECK(segmentToSkip(61, segments, skipped) == 1u);
    CHECK_FALSE(segmentToSkip(89.7, segments, skipped));  // its last half second: playing on is as quick
    CHECK_FALSE(segmentToSkip(120.1, segments, skipped));  // too short to be worth a seek
    CHECK_FALSE(segmentToSkip(61, {}, {}));
}

TEST_CASE("captions as WebVTT, the speeds offered, clock texts") {
    CaptionTrack track;
    track.url = "https://www.youtube.com/api/timedtext?v=x&lang=fr&fmt=srv3";
    CHECK(captionUrl(track) == "https://www.youtube.com/api/timedtext?v=x&lang=fr&fmt=vtt");
    track.url = "https://www.youtube.com/api/timedtext?v=x&lang=fr";
    CHECK(captionUrl(track) == "https://www.youtube.com/api/timedtext?v=x&lang=fr&fmt=vtt");
    CHECK(playbackSpeeds() == std::vector<double>{0.5, 0.75, 1, 1.25, 1.5, 2});
    CHECK(app::clockText(0) == "0:00");
    CHECK(app::clockText(65.9) == "1:05");
    CHECK(app::clockText(3746) == "1:02:26");
    CHECK(app::clockText(-3) == "0:00");
}

TEST_CASE("reports of watching: the playback's nonce and the two report addresses") {
    const unsigned char bytes[16] = {0, 1, 25, 26, 51, 52, 61, 62, 63, 64, 255, 128, 127, 10, 36, 200};
    const std::string nonce = playbackNonce(bytes);
    CHECK(nonce == "ABZaz09-_A_A_KkI");  // each byte's low 6 bits in the URL-safe base64 alphabet
    CHECK(playbackReportUrl("https://s.youtube.com/api/stats/playback?docid=x&len=212", nonce, 12.5) ==
          "https://s.youtube.com/api/stats/playback?docid=x&len=212&ver=2&cpn=ABZaz09-_A_A_KkI&cmt=12.500");
    CHECK(watchtimeReportUrl("https://s.youtube.com/api/stats/watchtime", "n", 30, 61.25, true) ==
          "https://s.youtube.com/api/stats/watchtime?ver=2&cpn=n&cmt=61.250&st=30.000&et=61.250&state=paused");
    CHECK(watchtimeReportUrl("https://s.youtube.com/api/stats/watchtime?a=1", "n", 0, 30, false)
              .find("&state=playing") != std::string::npos);
}
