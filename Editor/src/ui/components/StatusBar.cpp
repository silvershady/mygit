#include "StatusBar.h"
#include "Label.h"

namespace ui {

using namespace ftxui;

Element StatusBar::Render(EditorMode mode, const std::string& cmdBuffer, int animFrame) {
    const auto &theme = GetActiveTheme();
    const std::string pulseDots[] = {"●", "◐", "◑", "◒", "◓"};
    std::string dot = pulseDots[(animFrame / 3) % 5];

    if (mode == EditorMode::Command) {
        return hbox({
            text(" COMMAND ") | bold | color(Color::Black) | bgcolor(GetThemePrimaryColor(0)),
            text(" :" + cmdBuffer + "█") | bold | color(ColorNeonGreen) | bgcolor(Color::RGB(25, 30, 42)),
            filler(),
            text(" Esc: Cancel │ Enter: Run Command ") | color(ColorHint)
        }) | bgcolor(Color::RGB(18, 22, 30));
    }

    return hbox({
        text(" NORMAL ") | bold | color(ColorWhite) | bgcolor(ColorSelectionBg),
        text(" │ " + dot + " UTF-8 │ C++ │ clangd ✔ │ Theme: " + theme.name + " ") | color(GetThemePrimaryColor(40)),
        filler(),
        text(" : Command Mode │ cd /path │ :theme minimal │ Ctrl+T: Terminal (50%) │ Ctrl+Q: Exit ") | color(ColorHint)
    }) | bgcolor(Color::RGB(18, 22, 30));
}

} // namespace ui
