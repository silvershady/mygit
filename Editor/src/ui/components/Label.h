#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include "ui/ThemeUtils.h"

namespace ui {

class Label {
public:
    static ftxui::Element Text(const std::string& str, ftxui::Color fgColor = ColorWhite);
    static ftxui::Element Bold(const std::string& str, ftxui::Color fgColor = ColorWhite);
    static ftxui::Element Heading(const std::string& str, ftxui::Color fgColor = ColorCyan);
    static ftxui::Element Secondary(const std::string& str, ftxui::Color fgColor = ColorGrayDark);
    static ftxui::Element Hint(const std::string& str, ftxui::Color fgColor = ColorHint);
    static ftxui::Element KeyBadge(const std::string& keyStr, ftxui::Color fgColor = ColorHint);
    static ftxui::Element WithIcon(const std::string& icon, const std::string& textStr, ftxui::Color fgColor = ColorWhite);
    static ftxui::Element Selection(const std::string& str, ftxui::Color fgColor = ColorSelectionFg, ftxui::Color bgColor = ColorSelectionBg);
};

} // namespace ui
