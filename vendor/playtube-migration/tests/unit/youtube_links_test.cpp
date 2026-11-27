#include <doctest/doctest.h>

#include "youtube/links.hpp"

using playtube::youtube::videoIdFrom;

TEST_CASE("links: every usual form of a YouTube link gives its video") {
    for (const char* link : {"https://www.youtube.com/watch?v=dQw4w9WgXcQ",
                             "https://www.youtube.com/watch?feature=share&v=dQw4w9WgXcQ&t=42",
                             "http://youtube.com/watch?v=dQw4w9WgXcQ#t=1",
                             "https://m.youtube.com/watch?v=dQw4w9WgXcQ",
                             "youtube.com/watch?v=dQw4w9WgXcQ",
                             "https://youtu.be/dQw4w9WgXcQ",
                             "https://youtu.be/dQw4w9WgXcQ?si=abcdef&t=10",
                             "https://www.youtube.com/shorts/dQw4w9WgXcQ",
                             "https://www.youtube.com/live/dQw4w9WgXcQ?feature=share",
                             "https://www.youtube.com/embed/dQw4w9WgXcQ",
                             "  https://youtu.be/dQw4w9WgXcQ  ",
                             "dQw4w9WgXcQ"}) {
        INFO(link);
        const auto id = videoIdFrom(link);
        REQUIRE(id);
        CHECK(*id == "dQw4w9WgXcQ");
    }
    CHECK(*videoIdFrom("https://youtu.be/a-b_c1D2e3F") == "a-b_c1D2e3F");
    CHECK(*videoIdFrom("jNQXAC9IVRw") == "jNQXAC9IVRw");
    CHECK(*videoIdFrom("kJQP7kiw5Fk") == "kJQP7kiw5Fk");
    CHECK(*videoIdFrom("abc-def_ghi") == "abc-def_ghi");
    CHECK(*videoIdFrom("HTTPS://YOUTU.BE/dQw4w9WgXcQ") == "dQw4w9WgXcQ");
}

TEST_CASE("links: other sites, other pages and plain words give nothing") {
    for (const char* text : {"", "lofi hip hop", "relaxations", "abcdefghijk", "minecraft22", "gta6trailer",
                             "Minecraft22", "2024recap01",
                             "https://example.com/watch?v=dQw4w9WgXcQ", "https://www.youtube.com/watch?v=short",
                             "https://www.youtube.com/@LofiGirl", "https://youtu.be/",
                             "https://www.youtube.com/watch?v=dQw4w9WgXcQ0",
                             "https://www.youtube.com/watch?v=dQw4w9WgX!Q",
                             "https://youtube.com.evil/watch?v=dQw4w9WgXcQ",
                             "https://evilyoutube.com/watch?v=dQw4w9WgXcQ"}) {
        INFO(text);
        CHECK_FALSE(videoIdFrom(text));
    }
}
