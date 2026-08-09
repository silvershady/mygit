#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include "ui/ThemeUtils.h"

namespace ui {

class Separator {
public:
    // Empty vertical gap
    static ftxui::Element Empty();

    // Standard horizontal line separator with optional color
    static ftxui::Element Line(ftxui::Color color = ColorBorderDim);

    // Horizontal line separator with styled text pattern
    static ftxui::Element TextLine(const std::string& pattern, ftxui::Color color = ColorGrayDark);

    // Dynamic animated subtle texture pattern divider
    static ftxui::Element SubtleTexture(int animFrame);
};

} // namespace ui
