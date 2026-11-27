#include "app/shell/clock.hpp"

#include <cstdio>

#include "ui/i18n.hpp"

namespace playtube::app {

namespace {

std::int64_t floorDiv(std::int64_t a, std::int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

// the day of the month of a count of days since 1970-01-01 (the proleptic Gregorian calendar)
int dayOfMonth(std::int64_t days) {
    const std::int64_t z = days + 719468;
    const std::int64_t era = floorDiv(z, 146097);
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    return static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
}

}  // namespace

std::string clock24(std::int64_t utc, int offsetMinutes) {
    const std::int64_t local = utc + static_cast<std::int64_t>(offsetMinutes) * 60;
    const std::int64_t minutes = floorDiv(local, 60) - floorDiv(local, 86400) * 1440;
    char text[8];
    std::snprintf(text, sizeof text, "%02d:%02d", static_cast<int>(minutes / 60), static_cast<int>(minutes % 60));
    return text;
}

std::string clockDate(std::int64_t utc, int offsetMinutes) {
    const std::int64_t day = floorDiv(utc + static_cast<std::int64_t>(offsetMinutes) * 60, 86400);
    const int weekday = static_cast<int>(((day % 7) + 7 + 4) % 7);  // 1970-01-01 was a Thursday
    return ui::tr("clock.date", {ui::tr("clock.weekday." + std::to_string(weekday)), std::to_string(dayOfMonth(day))});
}

std::string headerClock(std::int64_t utc, int offsetMinutes) {
    return clockDate(utc, offsetMinutes) + " \xC2\xB7 " + clock24(utc, offsetMinutes);
}

}  // namespace playtube::app
