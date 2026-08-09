#include "Separator.h"

namespace ui {

using namespace ftxui;

Element Separator::Empty() {
    return separatorEmpty();
}

Element Separator::Line(Color color) {
    return separator() | ftxui::color(color);
}

Element Separator::TextLine(const std::string& pattern, Color color) {
    return text(pattern) | ftxui::color(color) | center;
}

Element Separator::SubtleTexture(int animFrame) {
    std::string linePattern;
    const std::string dots[] = {"·", "░", "·", "▒", "·", "✦", "·", "✧"};
    for (int i = 0; i < 80; ++i) {
        linePattern += dots[(i + animFrame / 4) % 8];
    }
    return text(linePattern) | color(Color::RGB(35, 42, 55));
}

} // namespace ui
