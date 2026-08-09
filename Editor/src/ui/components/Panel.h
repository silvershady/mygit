#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include "ui/ThemeUtils.h"

namespace ui {

class Panel {
public:
    // Create standard interactive panel with focus glow and active title badge
    static ftxui::Element Render(const std::string& iconTitle, ftxui::Element content, bool isFocused = false);

    // Create custom window container with specified header and border colors
    static ftxui::Element Window(ftxui::Element header, ftxui::Element content, ftxui::Color borderColor = ColorBorderDim);
};

} // namespace ui
