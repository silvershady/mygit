#pragma once
#include <ftxui/dom/elements.hpp>
#include <string>
#include "core/EditorState.h"
#include "ui/ThemeUtils.h"

namespace ui {

class StatusBar {
public:
    // Render status line based on current editor mode, command buffer, and animation frame
    static ftxui::Element Render(EditorMode mode, const std::string& cmdBuffer, int animFrame);
};

} // namespace ui
