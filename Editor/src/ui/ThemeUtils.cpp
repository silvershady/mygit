#include "ThemeUtils.h"

namespace ui {

const Theme& GetActiveTheme() {
    if (activeThemeIdx >= 0 && activeThemeIdx < (int)themes.size()) {
        return themes[activeThemeIdx];
    }
    return themes[0];
}

ftxui::Color GetThemePrimaryColor(int offset) {
    const auto &t = GetActiveTheme();
    if (t.name.find("Minimal") != std::string::npos) {
        return ftxui::Color::RGB(240, 240, 240);
    }
    if (t.name.find("Cyberpunk") != std::string::npos || t.name.find("RGB") != std::string::npos) {
        int step = (animFrame * 3 + offset) % 360;
        float rad = step * 3.14159f / 180.0f;
        uint8_t r = (uint8_t)(127 + 127 * sin(rad));
        uint8_t g = (uint8_t)(127 + 127 * sin(rad + 2.09439f));
        uint8_t b = (uint8_t)(127 + 127 * sin(rad + 4.18879f));
        return ftxui::Color::RGB(r, g, b);
    }
    int wave = (int)(sin((animFrame + offset) * 0.1) * 15);
    int r = std::min(255, std::max(0, (int)t.pR + wave));
    int g = std::min(255, std::max(0, (int)t.pG + wave));
    int b = std::min(255, std::max(0, (int)t.pB + wave));
    return ftxui::Color::RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

ftxui::Color GetThemeSecondaryColor(int offset) {
    const auto &t = GetActiveTheme();
    if (t.name.find("Minimal") != std::string::npos) {
        return ftxui::Color::RGB(160, 160, 160);
    }
    if (t.name.find("Cyberpunk") != std::string::npos || t.name.find("RGB") != std::string::npos) {
        int step = (animFrame * 3 + offset + 120) % 360;
        float rad = step * 3.14159f / 180.0f;
        uint8_t r = (uint8_t)(127 + 127 * sin(rad));
        uint8_t g = (uint8_t)(127 + 127 * sin(rad + 2.09439f));
        uint8_t b = (uint8_t)(127 + 127 * sin(rad + 4.18879f));
        return ftxui::Color::RGB(r, g, b);
    }
    return ftxui::Color::RGB(t.sR, t.sG, t.sB);
}

} // namespace ui
