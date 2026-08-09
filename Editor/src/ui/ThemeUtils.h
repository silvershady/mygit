#pragma once
#include "core/EditorState.h"
#include <ftxui/screen/color.hpp>
#include <cmath>
#include <algorithm>

namespace ui {

// Palette constants
const auto ColorNeonGreen     = ftxui::Color::RGB(0, 255, 136);
const auto ColorCyan          = ftxui::Color::RGB(0, 215, 255);
const auto ColorWhite         = ftxui::Color::RGB(255, 255, 255);
const auto ColorGrayLight     = ftxui::Color::RGB(200, 205, 215);
const auto ColorGrayDark      = ftxui::Color::RGB(110, 115, 125);
const auto ColorBorderDim     = ftxui::Color::RGB(55, 60, 70);
const auto ColorSelectionBg   = ftxui::Color::RGB(0, 90, 180);
const auto ColorSelectionFg   = ftxui::Color::RGB(255, 255, 255);
const auto ColorHint          = ftxui::Color::RGB(80, 160, 220);

const Theme& GetActiveTheme();
ftxui::Color GetThemePrimaryColor(int offset = 0);
ftxui::Color GetThemeSecondaryColor(int offset = 0);

} // namespace ui
