#pragma once

#include <cstdint>
#include <string>

namespace playtube::app {

// "21:45" (local, 24 h) of a UTC time, `offsetMinutes` from UTC.
std::string clock24(std::int64_t utc, int offsetMinutes);
// "Mercredi 11 · 21:45": the header's date and time.
std::string headerClock(std::int64_t utc, int offsetMinutes);
// "Mercredi 11": the date alone (the screensaver's, under the time).
std::string clockDate(std::int64_t utc, int offsetMinutes);

}  // namespace playtube::app
