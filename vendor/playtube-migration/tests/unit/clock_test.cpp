#include <doctest/doctest.h>

#include <string>

#include "app/shell/clock.hpp"
#include "ui/i18n.hpp"

using playtube::app::clock24;
using playtube::app::headerClock;

TEST_CASE("the clock: 24 h local time, and the header's day and time") {
    playtube::ui::translator().load(std::string(PLAYTUBE_SOURCE_DIR) + "/resources/i18n", "en");
    const std::int64_t at = 1792266300;  // 2026-10-17 19:45 UTC, a Saturday
    CHECK(clock24(at, 0) == "19:45");
    CHECK(clock24(at, 120) == "21:45");
    CHECK(clock24(at, 300) == "00:45");   // the next day
    CHECK(clock24(at, -1200) == "23:45");  // the day before
    CHECK(headerClock(at, 120) == "Saturday 17 \xC2\xB7 21:45");
    CHECK(headerClock(at, 300) == "Sunday 18 \xC2\xB7 00:45");
    CHECK(headerClock(1798761600, 0) == "Friday 1 \xC2\xB7 00:00");  // 2027-01-01
}
