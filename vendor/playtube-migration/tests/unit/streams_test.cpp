#include <doctest/doctest.h>

#include <initializer_list>
#include <string>

#include "youtube/streams.hpp"

using namespace playtube::youtube;

namespace {

Format format(int itag) {
    Format f;
    f.itag = itag;
    f.url = "https://media.example/" + std::to_string(itag);
    switch (itag) {
        case 140: f.mimeType = "audio/mp4; codecs=\"mp4a.40.2\""; break;
        case 251: f.mimeType = "audio/webm; codecs=\"opus\""; break;
        case 18: f.mimeType = "video/mp4; codecs=\"avc1.42001E, mp4a.40.2\""; f.height = 360; break;
        case 248: f.mimeType = "video/webm; codecs=\"vp9\""; f.height = 1080; break;
        case 299: f.mimeType = "video/mp4; codecs=\"avc1.64002a\""; f.height = 1080; break;  // 60 fps
        case 258: f.mimeType = "audio/mp4; codecs=\"mp4a.40.2\""; break;  // 5.1 AAC
        default:
            f.mimeType = "video/mp4; codecs=\"avc1.640028\"";
            f.height = itag == 137 ? 1080 : itag == 136 ? 720 : itag == 135 ? 480 : itag == 134 ? 360 : 240;
    }
    return f;
}

PlayerReply reply(std::initializer_list<int> itags) {
    PlayerReply r;
    r.playable = true;
    for (int itag : itags) {
        r.formats.push_back(format(itag));
    }
    return r;
}

}  // namespace

TEST_CASE("streams: the best H.264 video within the chosen quality, with the AAC audio") {
    const auto best = chooseStreams(reply({18, 248, 137, 136, 135, 134, 251, 140}), 1080);
    REQUIRE(best);
    CHECK(best->video == "https://media.example/137");
    CHECK(best->audio == "https://media.example/140");
    CHECK(best->height == 1080);
    const auto capped = chooseStreams(reply({137, 136, 135, 140}), 720);
    REQUIRE(capped);
    CHECK(capped->video == "https://media.example/136");
    CHECK(capped->height == 720);
}

TEST_CASE("streams: a video smaller than the chosen quality plays at its own size") {
    const auto small = chooseStreams(reply({134, 133, 140}), 1080);
    REQUIRE(small);
    CHECK(small->video == "https://media.example/134");
    const auto tooBig = chooseStreams(reply({137, 136, 140}), 360);
    REQUIRE(tooBig);
    CHECK(tooBig->video == "https://media.example/136");
}

TEST_CASE("streams: the muxed 360p without separate streams; nothing without H.264") {
    const auto muxed = chooseStreams(reply({18, 248, 251}), 1080);
    REQUIRE(muxed);
    CHECK(muxed->video == "https://media.example/18");
    CHECK(muxed->audio.empty());
    CHECK(muxed->height == 360);
    CHECK_FALSE(chooseStreams(reply({248, 251}), 1080));
    CHECK_FALSE(chooseStreams(reply({137}), 1080));  // no audio
    const auto alone = muxedStreams(reply({137, 140, 18}));
    REQUIRE(alone);
    CHECK(alone->video == "https://media.example/18");
    CHECK_FALSE(muxedStreams(reply({137, 140})));
}

TEST_CASE("streams: a live video plays its HLS playlist; a refused one gives nothing") {
    PlayerReply live = reply({137, 140});
    live.live = true;
    live.hls = "https://media.example/live.m3u8";
    const auto streams = chooseStreams(live, 1080);
    REQUIRE(streams);
    CHECK(streams->hls == "https://media.example/live.m3u8");
    CHECK(streams->video.empty());
    CHECK(streams->live);
    live.hls.clear();
    CHECK_FALSE(chooseStreams(live, 1080));
    PlayerReply refused = reply({137, 140});
    refused.playable = false;
    CHECK_FALSE(chooseStreams(refused, 1080));
}

TEST_CASE("streams: the captions, the title and the channel come along") {
    PlayerReply r = reply({137, 140});
    r.title = "Demo";
    r.channelId = "UCdemo";
    r.captions.push_back({"fr", "Fran\xC3\xA7" "ais", "https://media.example/fr.vtt", false});
    const auto streams = chooseStreams(r, 1080);
    REQUIRE(streams);
    REQUIRE(streams->captions.size() == 1);
    CHECK(streams->captions[0].language == "fr");
    CHECK(streams->title == "Demo");
    CHECK(streams->channelId == "UCdemo");
}

TEST_CASE("streams: 30 fps rather than 60 at the same height, surround left out; the original audio track") {
    const auto streams = chooseStreams(reply({299, 137, 258, 140}), 1080);
    REQUIRE(streams);
    CHECK(streams->video == "https://media.example/137");
    CHECK(streams->audio == "https://media.example/140");
    PlayerReply dubbed = reply({137});
    for (const char* lang : {"de", "en", "fr"}) {
        Format f = format(140);
        f.url = std::string("https://media.example/") + lang;
        f.defaultAudio = std::string(lang) == "en";
        dubbed.formats.push_back(f);
    }
    const auto original = chooseStreams(dubbed, 1080);
    REQUIRE(original);
    CHECK(original->audio == "https://media.example/en");
}

TEST_CASE("streams: every audio language offered, the original first; the volume-levelled copy left out") {
    PlayerReply r = reply({137});
    const char* tracks[][3] = {{"fr.3", "Fran\xC3\xA7" "ais", "0"}, {"en-US.4", "Anglais (US) original", "1"},
                               {"de.3", "Allemand", "0"}};
    for (const auto& t : tracks) {
        Format f = format(140);
        f.url = std::string("https://media.example/") + t[0];
        f.audioTrackId = t[0];
        f.audioTrackName = t[1];
        f.defaultAudio = t[2][0] == '1';
        r.formats.push_back(f);
    }
    Format drc = format(140);
    drc.url = "https://media.example/drc";
    drc.drc = true;
    r.formats.push_back(drc);
    const auto streams = chooseStreams(r, 1080);
    REQUIRE(streams);
    CHECK(streams->audio == "https://media.example/en-US.4");
    CHECK(streams->audioTrack == "en-US.4");
    REQUIRE(streams->audioTracks.size() == 3);
    CHECK(streams->audioTracks[0].id == "en-US.4");
    CHECK(streams->audioTracks[1].name == "Allemand");
    CHECK(streams->audioTracks[2].url == "https://media.example/fr.3");

    PlayerReply plain = reply({137, 140});
    Format levelled = format(140);
    levelled.url = "https://media.example/drc";
    levelled.drc = true;
    plain.formats.insert(plain.formats.begin(), levelled);
    const auto one = chooseStreams(plain, 1080);
    REQUIRE(one);
    CHECK(one->audio == "https://media.example/140");
    CHECK(one->audioTracks.empty());  // one language: no choice to offer
}

TEST_CASE("streams: 60 fps H.264 when it gives a higher resolution than 30 fps, or no 30 fps is offered") {
    PlayerReply r = reply({299, 18, 140});
    Format f298 = format(299);
    f298.itag = 298;
    f298.height = 720;
    f298.url = "https://media.example/298";
    r.formats.push_back(f298);
    const auto full = chooseStreams(r, 1080);
    REQUIRE(full);
    CHECK(full->video == "https://media.example/299");
    const auto capped = chooseStreams(r, 720);
    REQUIRE(capped);
    CHECK(capped->video == "https://media.example/298");
    const auto thirty = chooseStreams(reply({299, 137, 140}), 1080);
    REQUIRE(thirty);
    CHECK(thirty->video == "https://media.example/137");  // 30 fps preferred at the same height
    PlayerReply low = reply({135, 140});
    Format f299 = format(299);
    low.formats.push_back(f299);
    const auto sharper = chooseStreams(low, 1080);
    REQUIRE(sharper);
    CHECK(sharper->video == "https://media.example/299");  // 1080p60 rather than 480p30
}

TEST_CASE("streams: when their addresses expire") {
    Streams s;
    CHECK(expiresAt(s) == 0);
    s.video = "https://r1.googlevideo.com/videoplayback?expire=1762900000&ei=x&itag=137";
    s.audio = "https://r1.googlevideo.com/videoplayback?itag=140&expire=1762890000&ei=y";
    CHECK(expiresAt(s) == 1762890000);  // the earliest
    s = {};
    s.hls = "https://manifest.googlevideo.com/api/manifest/hls_variant/expire/1762911111/ei/abc/index.m3u8";
    CHECK(expiresAt(s) == 1762911111);
    s = {};
    s.video = "https://example.com/v?noexpire=5&clen=10";  // another parameter's name, not "expire"
    CHECK(expiresAt(s) == 0);
}
