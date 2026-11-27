#pragma once

#include <cstdint>
#include <vector>

struct NVGcontext;

namespace playtube::render {

// NanoVG on the current OpenGL ES 2 context (antialiasing, stencil strokes); nullptr on failure.
NVGcontext* createCanvas();
void destroyCanvas(NVGcontext* canvas);

// Pixels of the default framebuffer as top-down RGBA rows (GL reads bottom-up).
std::vector<std::uint8_t> readFramebuffer(int width, int height);

}  // namespace playtube::render
