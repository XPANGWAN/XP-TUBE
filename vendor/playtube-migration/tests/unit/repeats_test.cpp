#include <doctest/doctest.h>

#include <string>

#include "player/repeats.hpp"

using playtube::player::Repeats;

TEST_CASE("repeats: a message is written once, then at its 10th, 100th and 1000th repeat with the count") {
    Repeats repeats;
    const std::string late = "mpv [ffmpeg/video]: h264: Late SEI is not implemented.";
    int written = 0;
    std::string last;
    for (int i = 1; i <= 2813; ++i) {  // four hours of one video, seen on the console
        if (const auto line = repeats.line(late)) {
            ++written;
            last = *line;
            if (i == 1) {
                CHECK(*line == late);
            }
            if (i == 10) {
                CHECK(*line == late + " (10 times)");
            }
        }
    }
    CHECK(written == 4);  // 1, 10, 100, 1000
    CHECK(last == late + " (1000 times)");
}

TEST_CASE("repeats: each message is counted on its own; another one in between is written") {
    Repeats repeats;
    CHECK(repeats.line("a") == std::optional<std::string>("a"));
    CHECK(repeats.line("b") == std::optional<std::string>("b"));
    CHECK_FALSE(repeats.line("a"));
    CHECK_FALSE(repeats.line("b"));
    CHECK(repeats.line("c") == std::optional<std::string>("c"));
}

TEST_CASE("repeats: only the last 32 distinct messages are remembered") {
    Repeats repeats;
    CHECK(repeats.line("first"));
    for (int i = 0; i < 32; ++i) {
        CHECK(repeats.line("m" + std::to_string(i)));
    }
    CHECK(repeats.line("first") == std::optional<std::string>("first"));  // forgotten: written again
    CHECK_FALSE(repeats.line("m31"));
}
