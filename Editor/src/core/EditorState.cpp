#include "EditorState.h"

const vector<Theme> themes = {
    {"⚡ Cyberpunk Neon", "⚡",
     "\033[38;2;0;255;136m", "\033[38;2;0;191;255m", "\033[38;2;255;255;255m",
     "\033[38;2;120;120;120m", "\033[38;2;0;200;120m", "\033[38;2;255;165;0m", "\033[38;2;255;85;85m",
     0, 255, 136, 0, 191, 255},
    {"📟 Matrix Phosphor", "📟",
     "\033[38;2;0;255;64m", "\033[38;2;0;200;100m", "\033[38;2;220;255;220m",
     "\033[38;2;80;120;80m", "\033[38;2;0;180;50m", "\033[38;2;255;200;0m", "\033[38;2;255;50;50m",
     0, 255, 64, 0, 200, 100},
    {"🔮 Dracula Synth", "🔮",
     "\033[38;2;189;147;249m", "\033[38;2;255;121;198m", "\033[38;2;248;248;242m",
     "\033[38;2;98;114;164m", "\033[38;2;139;117;219m", "\033[38;2;241;250;140m", "\033[38;2;255;85;85m",
     189, 147, 249, 255, 121, 198},
    {"❄️ Nord Aurora", "❄️",
     "\033[38;2;136;192;208m", "\033[38;2;129;161;193m", "\033[38;2;236;239;244m",
     "\033[38;2;76;86;106m", "\033[38;2;96;152;168m", "\033[38;2;235;203;139m", "\033[38;2;191;97;106m",
     136, 192, 208, 129, 161, 193},
    {"🌸 Catppuccin Pastel", "🌸",
     "\033[38;2;203;166;247m", "\033[38;2;137;180;250m", "\033[38;2;205;214;244m",
     "\033[38;2;108;112;134m", "\033[38;2;163;126;207m", "\033[38;2;249;226;175m", "\033[38;2;243;139;168m",
     203, 166, 247, 137, 180, 250},
    {"🍂 Gruvbox Amber", "🍂",
     "\033[38;2;250;189;47m", "\033[38;2;254;128;25m", "\033[38;2;235;219;178m",
     "\033[38;2;146;131;116m", "\033[38;2;210;149;27m", "\033[38;2;215;153;33m", "\033[38;2;204;36;29m",
     250, 189, 47, 254, 128, 25},
    {"⚪ Minimalist Mono", "⚪",
     "\033[38;2;240;240;240m", "\033[38;2;180;180;180m", "\033[38;2;220;220;220m",
     "\033[38;2;100;100;100m", "\033[38;2;140;140;140m", "\033[38;2;200;200;200m", "\033[38;2;255;255;255m",
     240, 240, 240, 180, 180, 180}};
int activeThemeIdx = 0;

ScreenView currentView = ScreenView::Home;
EditorMode currentMode = EditorMode::Normal;
HomeSection homeSection = HomeSection::Explorer;
bool isMaximized = false;
bool collapseExplorer = true;
bool collapseRecent = true;
bool shouldExitApp = false;
int animFrame = 0;
int newFileAnimTimer = 0;
float sidebarAnimWidth = 0.0f;

vector<string> lines(1);
int cx = 0;
int cy = 0;
int rowOffset = 0;
int colOffset = 0;
int screenRows = 24;
int screenCols = 80;

string commandBuffer;
string searchBuffer;
string paletteBuffer;
string currentFile = "index.cpp";
bool isModified = false;

vector<string> openTabs = {"index.cpp"};
int activeTabIdx = 0;

FileNode ftRoot;
vector<FlatEntry> flatTree;
int ftCursor = 0;
int ftScrollOff = 0;

vector<string> recentFiles;
int recentFileCursor = 0;
int quickActionCursor = 0;

string toastMsg = "";
time_t toastTime = 0;

MouseEvent lastMouseEvent;
int selStartX = 0;
int selStartY = 0;
bool isMouseDragging = false;
string yankBuffer = "";
int matchRow = -1;
int matchCol = -1;
bool showFileTree = false;

vector<string> completionList;
bool showCompletion = false;
int completionIdx = 0;

vector<string> paletteItems;
int paletteIdx = 0;

bool showTerminalPanel = false;
int terminalHeight = 7;
vector<string> terminalOutput;

bool showAiPanel = false;
int aiPanelWidth = 30;
string aiOutputBuffer;
string aiInputBuffer;

bool pendingJ = false;
UndoManager undoManager;

bool showContextMenu = false;
int contextMenuX = 0;
int contextMenuY = 0;
