#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace playtube::youtube {

// The video id of a YouTube link (watch?v=, youtu.be/, shorts/, live/, embed/, with or without scheme and "www."
// or "m.") or of a bare 11-character id that looks random (a word such as "minecraft22" stays a search).
// Empty for anything else.
std::optional<std::string> videoIdFrom(std::string_view text);

}  // namespace playtube::youtube
