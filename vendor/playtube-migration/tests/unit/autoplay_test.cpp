#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "app/shell/autoplay.hpp"

using namespace playtube;

namespace {

youtube::Video video(const std::string& id, bool live = false, bool isShort = false) {
    youtube::Video v;
    v.id = id;
    v.live = live;
    v.isShort = isShort;
    return v;
}

}  // namespace

TEST_CASE("autoplay: a file that ends minutes before its end was cut, not finished") {
    // seen 2026-11-20 on the console: a 10 minute video "ended" after 6 min 24 s and the player closed
    CHECK(app::streamCutShort(384, 615, -1));
    CHECK_FALSE(app::streamCutShort(614.2, 615, -1));  // the end
    CHECK_FALSE(app::streamCutShort(606, 615, -1));    // within the last seconds: the end
    CHECK(app::streamCutShort(604, 615, -1));
    CHECK_FALSE(app::streamCutShort(3, 4, -1));        // a clip shorter than the margin
    CHECK_FALSE(app::streamCutShort(10, 0, -1));       // no duration known
}

TEST_CASE("autoplay: a second cut at the same place is the end; after some play it is a cut again") {
    CHECK_FALSE(app::streamCutShort(384, 615, 384));  // started again there, ended at once
    CHECK_FALSE(app::streamCutShort(387, 615, 384));
    CHECK(app::streamCutShort(450, 615, 384));        // played a minute more, cut again
}

TEST_CASE("autoplay: the next video is the first related one not played in this session") {
    const std::vector<youtube::Video> related = {video("live1", true), video("short1", false, true), video("a"),
                                                 video("b"), video("c")};
    REQUIRE(app::nextToPlay(related, false, {}) != nullptr);
    CHECK(app::nextToPlay(related, false, {})->id == "a");      // not a live, not a Short
    CHECK(app::nextToPlay(related, true, {})->id == "short1");  // Shorts shown: it may play
    CHECK(app::nextToPlay(related, false, {"a"})->id == "b");   // seen 2026-11-20: the same video came back
    CHECK(app::nextToPlay(related, false, {"b", "a"})->id == "c");
    CHECK(app::nextToPlay(related, false, {"a", "b", "c"})->id == "a");  // all played: the first again
    CHECK(app::nextToPlay({video("live1", true)}, false, {}) == nullptr);
    CHECK(app::nextToPlay({}, false, {}) == nullptr);
    CHECK(app::nextToPlay({video("")}, false, {}) == nullptr);  // no id: nothing to open
    youtube::Video list = video("PLabc");
    list.playlist = true;
    youtube::Video channel = video("UCabc");
    channel.isChannel = true;
    REQUIRE(app::nextToPlay({list, channel, video("z")}, false, {}) != nullptr);
    CHECK(app::nextToPlay({list, channel, video("z")}, false, {})->id == "z");  // a playlist is not a video
}

TEST_CASE("autoplay: the videos played are remembered once each, the latest last, the oldest forgotten") {
    std::vector<std::string> played;
    app::rememberPlayed(played, "a");
    app::rememberPlayed(played, "b");
    app::rememberPlayed(played, "a");  // again: moved to the end
    CHECK(played == std::vector<std::string>{"b", "a"});
    app::rememberPlayed(played, "");
    CHECK(played.size() == 2);
    for (std::size_t i = 0; i < app::kMaxPlayed + 10; ++i) {
        app::rememberPlayed(played, "v" + std::to_string(i));
    }
    CHECK(played.size() == app::kMaxPlayed);
    CHECK(played.back() == "v" + std::to_string(app::kMaxPlayed + 9));
}
