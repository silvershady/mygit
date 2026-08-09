#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <functional>
#include "ui/ThemeUtils.h"

namespace ui {

struct ListItem {
    std::string icon;
    std::string title;
    std::string subtitle;
    std::string badge;
    bool isSelected = false;
    ftxui::Color textColor = ColorWhite;
};

class ListView {
public:
    // Render list of structured list items with auto alignment and selection highlighting
    static ftxui::Element Render(const std::vector<ListItem>& items, bool isFocused = false);

    // Combine a list of pre-rendered element rows into a vertical list container
    static ftxui::Element RenderRows(const ftxui::Elements& rows);

    // Custom iterator renderer for dynamic or custom list views
    static ftxui::Element RenderCustom(size_t itemCount, std::function<ftxui::Element(size_t index)> renderRow);
};

} // namespace ui
