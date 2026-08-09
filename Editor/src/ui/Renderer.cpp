#include "core/EditorAPI.h"
#include "ui/components/Widgets.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <algorithm>
#include <thread>
#include <atomic>
#include <csignal>
#include <cmath>
#include <sys/poll.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>

using namespace ftxui;

void signalHandler(int sig) {
    (void)sig;
    disableRawMode();
    cleanupScreen();
    exit(0);
}

void printHelp() {
    std::cout << "arc v1.0 - Fast Terminal Text Editor\n\n"
              << "Usage:\n"
              << "  arc [OPTIONS] [PATH|FILES...]\n"
              << "  Arc [OPTIONS] [PATH|FILES...]\n\n"
              << "Options:\n"
              << "  -h, --help            Show this help message and exit\n"
              << "  -v, --version         Show version information and exit\n"
              << "  +<line>               Jump to line number (e.g. +120 main.cpp)\n"
              << "  --theme <name>        Start with a specific color theme (e.g. --theme minimal)\n\n"
              << "Examples:\n"
              << "  arc                           Open current working directory\n"
              << "  arc .                         Open current working directory\n"
              << "  Arc .                         Open current working directory\n"
              << "  arc src/                      Open specified directory\n"
              << "  arc main.cpp                  Open a file directly in editor\n"
              << "  arc main.cpp ui.cpp           Open multiple files in editor tabs\n"
              << "  arc +45 main.cpp              Open main.cpp and jump to line 45\n";
}

void printVersion() {
    std::cout << "arc version 1.0 (FTXUI powered C++ Terminal Editor)\n";
}

int mapFtxuiEventToKey(Event event) {
    if (event == Event::ArrowLeft) return ARROW_LEFT;
    if (event == Event::ArrowRight) return ARROW_RIGHT;
    if (event == Event::ArrowUp) return ARROW_UP;
    if (event == Event::ArrowDown) return ARROW_DOWN;
    if (event == Event::PageUp) return PAGE_UP;
    if (event == Event::PageDown) return PAGE_DOWN;
    if (event == Event::Home) return HOME_KEY;
    if (event == Event::End) return END_KEY;
    if (event == Event::Backspace) return 127;
    if (event == Event::Delete) return DEL_KEY;
    if (event == Event::Escape) return '\x1b';
    if (event == Event::Return) return '\r';
    if (event == Event::Tab) return '\t';
    if (event == Event::CtrlN) return 14;
    if (event == Event::CtrlP) return 16;
    if (event == Event::CtrlT) return 20;
    if (event == Event::CtrlQ) return 17;
    if (event == Event::CtrlS) return 19;
    if (event == Event::CtrlF) return 6;
    if (event == Event::CtrlV) return 22;
    if (event == Event::CtrlC) return 3;
    if (event == Event::CtrlO) return 15;
    if (event == Event::CtrlB) return 2;
    if (event == Event::CtrlA) return 1;


    if (event.is_character()) {
        std::string s = event.character();
        if (s.size() == 1) return s[0];
    }
    
    return -1;
}

// Clean Framed Header Logo Box composed using reusable Label, Separator, and Panel widgets
Element RenderHeaderLogo() {
    const auto &theme = ui::GetActiveTheme();
    std::vector<std::string> rawLogo = {
        "   ██████╗ ██████╗  ██████╗██╗  ██╗██╗ ██████╗ ██╗██████╗   ",
        "  ██╔══██╗██╔══██╗██╔════╝██║  ██║██║██╔═══██╗██║██╔══██╗  ",
        "  ███████║██████╔╝██║     ███████║██║██║   ██║██║██║  ██║  ",
        "  ██╔══██║██╔══██╗██║     ██╔══██║██║██║   ██║██║██║  ██║  ",
        "  ██║  ██║██║  ██║╚██████╗██║  ██║██║╚██████╔╝██║██████╔╝  ",
        "  ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝╚═╝ ╚═════╝ ╚═╝╚═════╝   "
    };

    Elements logoElements;
    for (size_t i = 0; i < rawLogo.size(); ++i) {
        logoElements.push_back(ui::Label::Bold(rawLogo[i], ui::GetThemePrimaryColor((int)i * 5)) | center);
    }

    const std::string spinners[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    std::string currentSpinner = spinners[(animFrame / 2) % 10];

    logoElements.push_back(ui::Separator::TextLine("════════════════════════════════════════════════════════════════════", ui::GetThemeSecondaryColor()));
    logoElements.push_back(ui::Label::Bold(currentSpinner + " arc — " + theme.name + " " + currentSpinner, ui::ColorWhite) | center);
    logoElements.push_back(ui::Label::Secondary(theme.symbol + " Fast  •  ✨ Symbolic Design  •  🤖 AI Powered  •  ⚙️ Animated UI", ui::ColorGrayLight) | center);

    auto headerTitle = ui::Label::Bold(" arc v1.0 [" + theme.symbol + "] ", ui::GetThemePrimaryColor());
    return ui::Panel::Window(headerTitle, vbox(logoElements), ui::GetThemeSecondaryColor());
}

// Panel 1: Explorer & Recent Projects Content with Spacing Gaps using Label, Separator, and ListView
Element RenderExplorerContent(bool isFocused) {
    Elements items;
    items.push_back(ui::Label::Heading("📁 WORKSPACE TREE", ui::ColorCyan));
    items.push_back(ui::Separator::Empty());
    
    int shown = 0;
    for (size_t i = ftScrollOff; i < flatTree.size() && shown < 8; ++i, ++shown) {
        const auto &e = flatTree[i];
        std::string indent(e.depth * 2, ' ');
        std::string icon = e.isDir ? (e.expanded ? "▼ " : "▶ ") : "📄 ";
        std::string label = indent + icon + e.name;
        
        bool isSelected = ((int)i == ftCursor && isFocused);
        if (isSelected) {
            items.push_back(ui::Label::Selection("▶ " + label));
        } else {
            items.push_back(ui::Label::Text("  " + label, e.isDir ? ui::ColorCyan : ui::ColorGrayLight));
        }
    }
    if (flatTree.empty()) {
        items.push_back(ui::Label::Secondary("  (Empty workspace)", ui::ColorGrayDark));
    }

    items.push_back(ui::Separator::Empty());
    items.push_back(ui::Label::Heading("🚀 RECENT PROJECTS", ui::ColorCyan));
    items.push_back(ui::Separator::Empty());
    
    std::vector<std::pair<std::string, std::string>> projects = {
        {"arc Editor", "Active Workspace"},
        {"Compiler", "~/Desktop/Compiler"},
        {"OS Kernel", "~/Desktop/OS"},
        {"Web Engine", "~/Desktop/WebEngine"}
    };
    for (const auto &p : projects) {
        items.push_back(hbox({
            ui::Label::WithIcon("🔹", p.first, ui::ColorWhite),
            filler(),
            ui::Label::Secondary(p.second + " ", ui::ColorGrayDark)
        }));
    }

    return ui::ListView::RenderRows(items);
}

// Panel 2: Recent Files Content using ListView component
Element RenderRecentContent(bool isFocused) {
    std::vector<ui::ListItem> listItems;
    for (size_t i = 0; i < recentFiles.size() && i < 10; ++i) {
        const std::string &f = recentFiles[i];
        std::string name = f;
        size_t sl = f.rfind('/');
        if (sl != std::string::npos) name = f.substr(sl + 1);

        bool isSelected = ((int)i == recentFileCursor && isFocused);
        listItems.push_back({
            "📂",
            name,
            "",
            isSelected ? "OPEN" : f,
            isSelected,
            ui::ColorGrayLight
        });
    }
    if (recentFiles.empty()) {
        return ui::Label::Secondary("  (No recent files)", ui::ColorGrayDark);
    }

    return ui::ListView::Render(listItems, isFocused);
}

// Panel 3: Quick Actions Content using ListView component
Element RenderActionsContent(bool isFocused) {
    struct ActionItem { std::string icon; std::string title; std::string desc; std::string shortcut; };
    std::vector<ActionItem> actions = {
        {"⚙️", "Build", "Build & Run", ":run"},
        {"🔨", "Compile", "g++ Compiler", ":g++"},
        {"▶", "Run", "Run Binary", ":./bin"},
        {"💻", "Terminal", "Shell Panel", "Ctrl+T"},
        {"⚡", "Palette", "Command List", "Ctrl+P"},
        {"📝", "New", "Create File", "Ctrl+N"},
        {"📂", "Open", "Open File", "Ctrl+O"},
        {"📁", "Explorer", "Sidebar Tree", "Ctrl+B"},
        {"🤖", "AI", "Assistant", "Ctrl+A"},
        {"🎨", "Theme", "Color Theme", ":theme"}
    };

    std::vector<ui::ListItem> listItems;
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto &act = actions[i];
        bool isSelected = ((int)i == quickActionCursor && isFocused);
        listItems.push_back({
            act.icon,
            act.title,
            act.desc,
            act.shortcut,
            isSelected,
            ui::ColorWhite
        });
    }

    return ui::ListView::Render(listItems, isFocused);
}

// Panel 4: System Info & Session Stats Content using Label, Separator, and ListView
Element RenderInfoContent(bool isFocused) {
    (void)isFocused;
    SystemInfo si = getSystemInfo();
    
    int memPercent = si.totalMemKB > 0 ? (int)(100LL * si.usedMemKB / si.totalMemKB) : 38;
    int diskPercent = si.totalDiskBytes > 0 ? (int)(100ULL * si.usedDiskBytes / si.totalDiskBytes) : 25;

    Elements items;
    items.push_back(hbox({ ui::Label::Secondary("🖥 OS       : ", ui::ColorGrayDark), ui::Label::Text(si.osName.empty() ? "Linux" : si.osName, ui::ColorWhite) }));
    items.push_back(hbox({ ui::Label::Secondary("🐚 Shell    : ", ui::ColorGrayDark), ui::Label::Text(si.shell, ui::ColorWhite) }));
    items.push_back(hbox({ ui::Label::Secondary("💻 Terminal : ", ui::ColorGrayDark), ui::Label::Text(si.terminalName.empty() ? "terminal" : si.terminalName, ui::ColorWhite) }));
    items.push_back(hbox({ ui::Label::Secondary("⚙️ Compiler : ", ui::ColorGrayDark), ui::Label::Bold("clangd ✔", ui::ColorNeonGreen) }));
    items.push_back(hbox({ ui::Label::Secondary("⏱ Uptime   : ", ui::ColorGrayDark), ui::Label::Text(si.uptime.empty() ? "42m" : si.uptime, ui::ColorWhite) }));
    
    items.push_back(ui::Separator::Empty());
    items.push_back(ui::Label::Heading("📊 RAM USAGE (" + std::to_string(memPercent) + "%)", ui::GetThemePrimaryColor(0)));
    items.push_back(ui::Separator::Empty());
    items.push_back(gauge(memPercent / 100.0f) | color(ui::GetThemePrimaryColor(10)));
    
    items.push_back(ui::Separator::Empty());
    items.push_back(ui::Label::Heading("📊 DISK USAGE (" + std::to_string(diskPercent) + "%)", ui::GetThemePrimaryColor(20)));
    items.push_back(ui::Separator::Empty());
    items.push_back(gauge(diskPercent / 100.0f) | color(ui::GetThemePrimaryColor(30)));

    items.push_back(ui::Separator::Empty());
    items.push_back(ui::Label::Heading("⏱ LAST SESSION", ui::ColorCyan));
    items.push_back(ui::Separator::Empty());
    items.push_back(ui::Label::Text("  • UTF-8 / Unix (LF)", ui::ColorGrayLight));
    items.push_back(ui::Label::Text("  • Status: Active", ui::ColorNeonGreen));

    return ui::ListView::RenderRows(items);
}

// Embedded Terminal Output Window composed using Panel widget
Element RenderTerminalPanel() {
    Elements termLines;
    if (terminalOutput.empty()) {
        termLines.push_back(ui::Label::Secondary("  (No command output yet. Type commands like 'clear', 'brun', 'javac file.java', 'java file' in : command mode)", ui::ColorGrayDark));
    } else {
        size_t maxRows = 20;
        size_t start = terminalOutput.size() > maxRows ? terminalOutput.size() - maxRows : 0;
        for (size_t i = start; i < terminalOutput.size(); ++i) {
            termLines.push_back(ui::Label::Text(terminalOutput[i], ui::ColorGrayLight));
        }
    }
    
    auto termHeader = ui::Label::Bold(" 💻 EMBEDDED TERMINAL BOX PANEL (Press Ctrl+T to Collapse) ", ui::GetThemePrimaryColor(10));
    return ui::Panel::Window(termHeader, vbox(termLines), ui::GetThemeSecondaryColor()) | bgcolor(Color::RGB(15, 17, 24)) | flex;
}

// Bottom Status Line composed using reusable StatusBar widget
Element RenderStatusLine() {
    return ui::StatusBar::Render(currentMode, commandBuffer, animFrame);
}

// Render Subtle Texture using Separator widget
Element RenderSubtleTexture() {
    return ui::Separator::SubtleTexture(animFrame);
}

// Combined Dashboard composed of Panel, ListView, StatusBar, Label, and Separator widgets
Element RenderHomeScreen() {
    auto header = RenderHeaderLogo();

    bool explorerFocused = (homeSection == HomeSection::Explorer);
    bool recentFocused   = (homeSection == HomeSection::RecentFiles);
    bool actionsFocused  = (homeSection == HomeSection::QuickActions);
    bool infoFocused     = (homeSection == HomeSection::SystemInfo);

    auto panelExplorer = ui::Panel::Render("📁 FILE EXPLORER", RenderExplorerContent(explorerFocused), explorerFocused) | flex;
    auto panelRecent   = ui::Panel::Render("📂 RECENT FILES", RenderRecentContent(recentFocused), recentFocused) | flex;
    auto panelActions  = ui::Panel::Render("⚡ QUICK ACTIONS", RenderActionsContent(actionsFocused), actionsFocused) | flex;
    auto panelInfo     = ui::Panel::Render("🖥 SYSTEM INFO & SESSION", RenderInfoContent(infoFocused), infoFocused) | flex;

    auto grid = hbox({
        panelExplorer,
        panelRecent,
        panelActions,
        panelInfo
    }) | flex;

    auto status = RenderStatusLine();

    Elements mainStack;
    mainStack.push_back(header);
    mainStack.push_back(RenderSubtleTexture());
    mainStack.push_back(grid | flex);

    if (showTerminalPanel) {
        mainStack.push_back(RenderSubtleTexture());
        mainStack.push_back(RenderTerminalPanel() | flex);
    }

    mainStack.push_back(RenderSubtleTexture());
    mainStack.push_back(status);

    auto mainContent = vbox(mainStack);

    if (showContextMenu) {
        Elements menuItems;
        menuItems.push_back(ui::Label::Text(" 📋  Copy  ", ui::ColorWhite));
        menuItems.push_back(ui::Label::Text(" ✂️   Cut   ", ui::ColorWhite));
        menuItems.push_back(ui::Label::Text(" 📥  Paste ", ui::ColorWhite));
        menuItems.push_back(ui::Separator::Line());
        menuItems.push_back(ui::Label::Bold(" ❌  Quit  ", Color::Red));

        auto menuTitle = ui::Label::Bold(" Context Menu ", ui::ColorNeonGreen);
        auto menuWin = ui::Panel::Window(menuTitle, vbox(menuItems), ui::ColorCyan)
                       | bgcolor(Color::RGB(22, 27, 38));

        int topPad = std::max(0, contextMenuY - 1);
        int leftPad = std::max(0, contextMenuX - 1);

        auto overlay = vbox({
            filler() | size(HEIGHT, EQUAL, topPad),
            hbox({
                filler() | size(WIDTH, EQUAL, leftPad),
                menuWin,
                filler()
            }),
            filler()
        });

        return dbox({ mainContent, overlay });
    }

    return mainContent;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::vector<std::string> positionalArgs;
    int targetJumpLine = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "--theme" && i + 1 < argc) {
            std::string themeName = argv[++i];
            for (size_t t = 0; t < themes.size(); ++t) {
                std::string tNameLower = themes[t].name;
                std::string reqLower = themeName;
                std::transform(tNameLower.begin(), tNameLower.end(), tNameLower.begin(), ::tolower);
                std::transform(reqLower.begin(), reqLower.end(), reqLower.begin(), ::tolower);
                if (tNameLower.find(reqLower) != std::string::npos) {
                    activeThemeIdx = (int)t;
                    break;
                }
            }
        } else if (arg.size() > 1 && arg[0] == '+') {
            try {
                targetJumpLine = std::stoi(arg.substr(1));
            } catch (...) {}
        } else {
            positionalArgs.push_back(arg);
        }
    }

    if (positionalArgs.empty()) {
        loadFileList();
        loadRecentFiles();
        currentView = ScreenView::Home;
    } else {
        std::string firstArg = positionalArgs[0];
        std::error_code ec;
        if (std::filesystem::is_directory(firstArg, ec)) {
            std::filesystem::current_path(firstArg, ec);
            loadFileList();
            loadRecentFiles();
            currentView = ScreenView::Home;
        } else {
            loadFileList();
            loadRecentFiles();
            openTabs.clear();
            for (const auto& filePath : positionalArgs) {
                openfile(filePath);
            }
            if (!openTabs.empty()) {
                activeTabIdx = 0;
                openfile(openTabs[0]);
            }
            currentView = ScreenView::Editor;

            if (targetJumpLine > 0 && !lines.empty()) {
                cy = std::max(0, std::min((int)lines.size() - 1, targetJumpLine - 1));
                cx = 0;
                rowOffset = std::max(0, cy - 5);
            }
        }
    }

    while (!shouldExitApp) {
        if (currentView == ScreenView::Home) {
            auto screen = ScreenInteractive::Fullscreen();

            std::atomic<bool> animRunning{true};
            std::thread animThread([&screen, &animRunning]() {
                while (animRunning) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    if (!animRunning) break;
                    animFrame++;
                    screen.Post(Event::Custom);
                }
            });

            auto component = Renderer([] {
                return RenderHomeScreen();
            });

            component |= CatchEvent([&](Event event) {
                if (event == Event::Custom) {
                    return true;
                }

                if (event.is_mouse()) {
                    int mx = event.mouse().x + 1;
                    int my = event.mouse().y + 1;
                    int btn = (event.mouse().button == Mouse::Right) ? 2 :
                              (event.mouse().button == Mouse::Middle) ? 1 : 0;
                    char type = (event.mouse().motion == Mouse::Released) ? 'm' : 'M';
                    lastMouseEvent = {mx, my, btn, type};

                    if (event.mouse().motion == Mouse::Pressed) {
                        handleMouseClick(mx, my, btn, type);
                        screen.Post(Event::Custom);
                        return true;
                    }
                }

                int key = mapFtxuiEventToKey(event);
                if (key != -1) {
                    processKey(key);
                    if (currentView == ScreenView::Editor || shouldExitApp) {
                        animRunning = false;
                        screen.Exit();
                    }
                    return true;
                }
                return false;
            });

            screen.Loop(component);

            animRunning = false;
            if (animThread.joinable()) {
                animThread.detach();
            }

            if (shouldExitApp) {
                disableRawMode();
                cleanupScreen();
                exit(0);
            }

        } else if (currentView == ScreenView::Editor) {
            enableRawMode();
            while (currentView == ScreenView::Editor && !shouldExitApp) {
                animFrame++;
                refresh_screen();
                struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
                if (poll(&pfd, 1, 35) > 0) {
                    int key = readkey();
                    if (key != -1) {
                        processKey(key);
                    }
                }
            }
            disableRawMode();
            if (shouldExitApp) {
                cleanupScreen();
                exit(0);
            }
        }
    }

    disableRawMode();
    cleanupScreen();
    return 0;
}
