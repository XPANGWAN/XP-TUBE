#pragma once

#include <nanovg.h>

#include "ui/theme.hpp"

namespace playtube::ui {

inline NVGcolor nvgColor(Color color) { return nvgRGBA(color.r, color.g, color.b, color.a); }

}  // namespace playtube::ui
