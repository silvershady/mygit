#include "Panel.h"
#include "Separator.h"

namespace ui {

using namespace ftxui;

Element Panel::Render(const std::string& iconTitle, Element content, bool isFocused) {
    const auto &theme = GetActiveTheme();
    Element header;
    if (isFocused) {
        header = hbox({
            text(" " + iconTitle + " ") | bold | color(ColorWhite),
            text(" [" + theme.symbol + " ACTIVE ►] ") | bold | color(Color::Black) | bgcolor(GetThemePrimaryColor(0))
        });
    } else {
        header = text(" " + iconTitle + " ") | color(ColorGrayDark);
    }

    auto win = window(header, vbox({ Separator::Empty(), content }));
    if (isFocused) {
        return win | borderRounded | color(GetThemePrimaryColor(20));
    } else {
        return win | borderRounded | color(ColorBorderDim);
    }
}

Element Panel::Window(Element header, Element content, Color borderColor) {
    return window(header, content) | borderRounded | color(borderColor);
}

} // namespace ui
