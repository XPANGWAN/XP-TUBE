#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace playtube::player {

// A WebP picture ("RIFF....WEBP": DeArrow's thumbnails) to RGBA, with FFmpeg's decoder (the picture decoder of the
// UI reads JPEG and PNG only). False on anything else, or past `maxSide` pixels on a side.
bool decodeWebp(const std::string& bytes, int& width, int& height, std::vector<std::uint8_t>& rgba, int maxSide);

}  // namespace playtube::player
