#include "ListView.h"

namespace ui {

using namespace ftxui;

Element ListView::Render(const std::vector<ListItem>& items, bool isFocused) {
    Elements rows;
    for (const auto& item : items) {
        if (item.isSelected && isFocused) {
            Elements line;
            line.push_back(text("▶ " + (item.icon.empty() ? "" : item.icon + " ") + item.title + " ") | bold | color(ColorSelectionFg) | bgcolor(ColorSelectionBg));
            if (!item.subtitle.empty()) {
                line.push_back(text("• " + item.subtitle + " ") | bold | color(ColorSelectionFg) | bgcolor(ColorSelectionBg));
            }
            line.push_back(filler());
            if (!item.badge.empty()) {
                line.push_back(text(item.badge + " ") | bold | color(ColorSelectionFg) | bgcolor(ColorSelectionBg));
            }
            rows.push_back(hbox(line));
        } else {
            Elements line;
            line.push_back(text("  " + (item.icon.empty() ? "" : item.icon + " ") + item.title + " ") | color(item.textColor));
            if (!item.subtitle.empty()) {
                line.push_back(text("• " + item.subtitle) | color(ColorGrayDark));
            }
            line.push_back(filler());
            if (!item.badge.empty()) {
                line.push_back(text(item.badge + " ") | color(ColorHint));
            }
            rows.push_back(hbox(line));
        }
    }
    return vbox(rows);
}

Element ListView::RenderRows(const Elements& rows) {
    return vbox(rows);
}

Element ListView::RenderCustom(size_t itemCount, std::function<Element(size_t index)> renderRow) {
    Elements rows;
    rows.reserve(itemCount);
    for (size_t i = 0; i < itemCount; ++i) {
        rows.push_back(renderRow(i));
    }
    return vbox(rows);
}

} // namespace ui
