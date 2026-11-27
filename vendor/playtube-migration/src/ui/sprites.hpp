#pragma once

#include <cstdint>
#include <vector>

#include "ui/theme.hpp"

struct NVGcontext;

namespace playtube::ui {

// A vertical fade of one colour: alpha `from` on the first row to `to` at row `ramp` (sampled at the pixel's
// centre, as NanoVG's linear gradient), `to` below. Premultiplied RGBA, width x height.
std::vector<std::uint8_t> verticalFadeImage(int width, int height, int ramp, Color color, std::uint8_t from,
                                            std::uint8_t to);

// Draws the fade at (x, y) on whole pixels, as solid rows (one rectangle per run of rows with the same alpha): the
// renderer's constant-colour path, several times cheaper than the image (measured on the console, 2026-11-20: the
// player's shade as an image took 16 ms of a 21 ms frame).
void drawVerticalFade(NVGcontext* vg, float x, float y, int width, int height, int ramp, Color color,
                      std::uint8_t from, std::uint8_t to);

}  // namespace playtube::ui
