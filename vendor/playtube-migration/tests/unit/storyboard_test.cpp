#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "player/trickplay.hpp"
#include "player/seek.hpp"
#include "youtube/storyboard.hpp"

using namespace playtube;

namespace {
// the shape of a real spec (2026-11-11, a 3:33 video): three levels, the last 160 x 90 in 5 x 5 sheets every 2 s
const char* const kSpec =
    "https://i.ytimg.com/sb/abcdefghijk/storyboard3_L$L/$N.jpg?sqp=demo"
    "|48#27#100#10#10#0#default#rs$AAA|80#45#108#10#10#2000#M$M#rs$BBB|160#90#108#5#5#2000#M$M#rs$CCC";
}  // namespace

TEST_CASE("storyboards: the largest pictures up to 320 wide, and each sheet's address") {
    const auto board = youtube::parseStoryboard(kSpec, 213);
    REQUIRE(board);
    CHECK(board->width == 160);
    CHECK(board->height == 90);
    CHECK(board->columns == 5);
    CHECK(board->rows == 5);
    CHECK(board->count == 108);
    CHECK(board->intervalMs == 2000);
    CHECK(board->sheetUrl(0) == "https://i.ytimg.com/sb/abcdefghijk/storyboard3_L2/M0.jpg?sqp=demo&sigh=rs$CCC");
    CHECK(board->sheetUrl(4) == "https://i.ytimg.com/sb/abcdefghijk/storyboard3_L2/M4.jpg?sqp=demo&sigh=rs$CCC");
}

TEST_CASE("storyboards: a level without an interval is spread over the video; unreadable specs give nothing") {
    const auto only = youtube::parseStoryboard("https://i.ytimg.com/sb/x/L$L/$N.jpg|48#27#100#10#10#0#default#rs$A", 200);
    REQUIRE(only);
    CHECK(only->intervalMs == 2000);
    CHECK(only->sheetUrl(0) == "https://i.ytimg.com/sb/x/L0/default.jpg?sigh=rs$A");
    CHECK_FALSE(youtube::parseStoryboard("", 100));
    CHECK_FALSE(youtube::parseStoryboard("http://insecure/x|160#90#10#5#5#2000#M$M#s", 100));
    CHECK_FALSE(youtube::parseStoryboard("https://x/$L|160#90#ten#5#5#2000#M$M#s", 100));
    CHECK_FALSE(youtube::parseStoryboard("https://x/$L|1280#720#10#5#5#2000#M$M#s", 100));  // pictures too large
}

TEST_CASE("trickplay: the frame for a time, clamped to the last thumbnail") {
    player::TrickplayInfo info;
    info.width = 320;
    info.height = 180;
    info.tileWidth = 10;
    info.tileHeight = 10;
    info.thumbnailCount = 523;
    info.intervalMs = 10000;
    auto frame = player::trickplayFrame(info, 0);
    CHECK(frame.sheet == 0);
    CHECK(frame.x == 0);
    CHECK(frame.y == 0);
    CHECK(frame.width == 320);
    CHECK(frame.height == 180);
    frame = player::trickplayFrame(info, 1015);  // thumbnail 101: sheet 1, column 1, row 0
    CHECK(frame.sheet == 1);
    CHECK(frame.x == 320);
    CHECK(frame.y == 0);
    frame = player::trickplayFrame(info, 99999);  // past the end: thumbnail 522, sheet 5, column 2, row 2
    CHECK(frame.sheet == 5);
    CHECK(frame.x == 640);
    CHECK(frame.y == 360);
    CHECK(player::trickplayFrame(info, -5).sheet == 0);
    CHECK(player::trickplaySheets(info) == 6);
}

TEST_CASE("trickplay: the smallest width that looks right") {
    player::TrickplayInfo small;
    small.width = 160;
    player::TrickplayInfo medium;
    medium.width = 320;
    player::TrickplayInfo large;
    large.width = 480;
    CHECK(player::pickTrickplay({large, medium, small})->width == 320);
    CHECK(player::pickTrickplay({small})->width == 160);
    CHECK_FALSE(player::pickTrickplay({}).has_value());
}

TEST_CASE("trickplay: sheets too big to decode are never chosen") {
    player::TrickplayInfo small;  // 3200 x 1800: fine
    small.width = 320;
    small.height = 180;
    small.tileWidth = 10;
    small.tileHeight = 10;
    player::TrickplayInfo big = small;  // 4800 x 2700: over the decoder's 8 megapixels
    big.width = 480;
    big.height = 270;
    CHECK(player::pickTrickplay({big, small})->width == 320);
    CHECK_FALSE(player::pickTrickplay({big}).has_value());
}

TEST_CASE("chapters: the one at a time; L1 back to its start (or the previous within 3 s), R1 to the next") {
    const std::vector<double> starts{0, 117, 302, 455};
    CHECK(player::chapterAt({}, 10) == -1);
    CHECK(player::chapterAt(starts, 0) == 0);
    CHECK(player::chapterAt(starts, 200) == 1);
    CHECK(player::chapterAt(starts, 999) == 3);
    CHECK(player::chapterTarget(starts, 200, 1) == 302);
    CHECK(player::chapterTarget(starts, 200, -1) == 117);   // 83 s into it: back to its start
    CHECK(player::chapterTarget(starts, 118, -1) == 0);     // within its first 3 s: the previous one
    CHECK(player::chapterTarget(starts, 500, 1) == -1);     // the last one: nowhere to go
    CHECK(player::chapterTarget(starts, 1, -1) == 0);       // the first one, near its start: stays
    CHECK(player::chapterTarget({}, 10, 1) == -1);
}
