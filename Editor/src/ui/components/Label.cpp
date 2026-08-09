#include "Label.h"

namespace ui {

using namespace ftxui;

Element Label::Text(const std::string& str, Color fgColor) {
    return ftxui::text(str) | color(fgColor);
}

Element Label::Bold(const std::string& str, Color fgColor) {
    return ftxui::text(str) | bold | color(fgColor);
}

Element Label::Heading(const std::string& str, Color fgColor) {
    return ftxui::text(str) | bold | color(fgColor);
}

Element Label::Secondary(const std::string& str, Color fgColor) {
    return ftxui::text(str) | color(fgColor);
}

Element Label::Hint(const std::string& str, Color fgColor) {
    return ftxui::text(str) | color(fgColor);
}

Element Label::KeyBadge(const std::string& keyStr, Color fgColor) {
    return ftxui::text(keyStr) | color(fgColor);
}

Element Label::WithIcon(const std::string& icon, const std::string& textStr, Color fgColor) {
    return hbox({
        ftxui::text(icon + " ") | bold | color(fgColor),
        ftxui::text(textStr) | color(fgColor)
    });
}

Element Label::Selection(const std::string& str, Color fgColor, Color bgColor) {
    return ftxui::text(str) | bold | color(fgColor) | ftxui::bgcolor(bgColor);
}

} // namespace ui
