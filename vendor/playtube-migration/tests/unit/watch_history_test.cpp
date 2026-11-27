#include <doctest/doctest.h>

#include <vector>

#include "app/shell/watch_history.hpp"

using namespace playtube;
using app::WatchReport;
using app::WatchSegments;

namespace {

// Plays from `from` to `to` (excluded) a second at a time; the reports on the way.
std::vector<WatchReport> play(WatchSegments& segments, double from, double to, bool paused = false) {
    std::vector<WatchReport> out;
    for (double position = from; position < to; position += 1) {
        for (const WatchReport& report : segments.update(position, paused)) {
            out.push_back(report);
        }
    }
    return out;
}

}  // namespace

TEST_CASE("watch history: the start once, then a part every 2 minutes of video") {
    WatchSegments segments;
    const auto reports = play(segments, 12, 275);
    REQUIRE(reports.size() == 3);
    CHECK(reports[0].start);
    CHECK(reports[0].to == 12);
    CHECK_FALSE(reports[1].start);
    CHECK(reports[1].from == 12);
    CHECK(reports[1].to == 132);
    CHECK(reports[2].from == 132);
    CHECK(reports[2].to == 252);
    const auto last = segments.flush();  // the player closes
    REQUIRE(last);
    CHECK(last->from == 252);
    CHECK(last->to == 274);
    CHECK_FALSE(segments.flush());  // nothing more watched since
}

TEST_CASE("watch history: a short watch is one start and, when the player closes, the part watched") {
    WatchSegments segments;
    const auto reports = play(segments, 0, 75);  // under 2 minutes: no part on the way
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].start);
    const auto last = segments.flush();
    REQUIRE(last);
    CHECK(last->from == 0);
    CHECK(last->to == 74);
}

TEST_CASE("watch history: a pause and a seek end the part watched before them") {
    WatchSegments segments;
    play(segments, 0, 10);
    auto reports = segments.update(10, true);  // paused at 10 s
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].from == 0);
    CHECK(reports[0].to == 10);
    CHECK(reports[0].paused);
    CHECK(play(segments, 10, 10.5, true).empty());  // still paused: nothing new
    CHECK(play(segments, 10, 20).empty());           // playing again
    reports = segments.update(200, false);           // a seek forward
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].from == 10);
    CHECK(reports[0].to == 19);
    CHECK_FALSE(reports[0].paused);
    CHECK(play(segments, 201, 215).empty());
    reports = segments.update(3, false);  // back near the start (a Short starting again too)
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].from == 200);
    CHECK(reports[0].to == 214);
}

TEST_CASE("watch history: a live video reports its start only") {
    WatchSegments segments(true);
    const auto reports = play(segments, 0, 100);
    REQUIRE(reports.size() == 1);
    CHECK(reports[0].start);
    CHECK_FALSE(segments.flush());
}
