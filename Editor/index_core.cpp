#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <thread>
#include <unordered_map>

using namespace std;

string getSystemClipboard();
void insertTextAtCursor(const string &text);

//------------------------------------------------------------
// ARCHIOID Cyberpunk Terminal IDE Engine v0.5.0
//------------------------------------------------------------

enum KeyCode {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  HOME_KEY,
  END_KEY,
  DEL_KEY,
  MOUSE_EVENT
};

enum class ScreenView { Home, Editor };
enum class EditorMode {
  Normal,
  Insert,
  Visual,
  Command,
  Search,
  AiInput,
  Palette,
  SavePrompt
};
enum class HomeSection {
  Explorer,
  RecentFiles,
  QuickActions,
  SystemInfo
}; // keyboard focus on home screen

struct MouseEvent {
  int x;
  int y;
  int btn;
  char type; // 'M' for press/drag, 'm' for release
};

struct BufferState {
  vector<string> lines;
  int cx;
  int cy;
};

//------------------------------------------------------------
// Layout Manager System (Rect & Bounding Architecture)
//------------------------------------------------------------

struct Rect {
  int x;
  int y;
  int width;
  int height;
};

struct Layout {
  Rect header;
  Rect sidebar;
  Rect main;
  Rect terminal;
  Rect status;
  Rect command;
};

class LayoutManager {
public:
  static Layout calculate(int screenRows, int screenCols, bool showSidebar,
                          bool showTerminal, int termHeight, bool maximized) {
    Layout l;
    l.header = {1, 1, screenCols, 2};

    int termH = showTerminal ? max(6, (screenRows - 5) / 2) : 0;

    if (maximized) {
      l.sidebar = {0, 0, 0, 0};
      l.main = {1, 3, screenCols,
                screenRows - 4 - termH};
    } else {
      int sidebarWidth = showSidebar ? max(26, (int)(screenCols * 0.24)) : 0;
      l.sidebar = {1, 3, sidebarWidth,
                   screenRows - 4 - termH};

      int mainX = 1 + sidebarWidth;
      int mainWidth = screenCols - sidebarWidth;
      int mainHeight = screenRows - 4 - termH;
      l.main = {mainX, 3, mainWidth, mainHeight};
    }

    if (showTerminal && !maximized) {
      l.terminal = {1, 3 + l.main.height, screenCols, termH};
    } else {
      l.terminal = {0, 0, 0, 0};
    }

    l.status = {1, screenRows - 1, screenCols, 1};
    l.command = {1, screenRows, screenCols, 1};

    return l;
  }
};

//------------------------------------------------------------
// 24-Bit TrueColor Theme System
//------------------------------------------------------------

struct Theme {
  string name;
  string symbol;
  string primary;   // Neon Green Accent
  string secondary; // Cyan Sub Accent
  string text;      // Body Text
  string inactive;  // Muted Gray
  string border;    // Border Box
  string warning;   // Orange
  string error;     // Red
  uint8_t pR, pG, pB;
  uint8_t sR, sG, sB;
};

extern const vector<Theme> themes = {
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

class UndoManager {
private:
  vector<BufferState> undoStack;
  vector<BufferState> redoStack;
  const size_t MAX_STACK_SIZE = 100;

public:
  void saveSnapshot(const vector<string> &l, int c_x, int c_y) {
    undoStack.push_back({l, c_x, c_y});
    redoStack.clear();
    if (undoStack.size() > MAX_STACK_SIZE) {
      undoStack.erase(undoStack.begin());
    }
  }

  bool undo(vector<string> &l, int &c_x, int &c_y) {
    if (undoStack.empty())
      return false;
    redoStack.push_back({l, c_x, c_y});
    BufferState prev = undoStack.back();
    undoStack.pop_back();
    l = prev.lines;
    c_x = prev.cx;
    c_y = prev.cy;
    return true;
  }

  bool redo(vector<string> &l, int &c_x, int &c_y) {
    if (redoStack.empty())
      return false;
    undoStack.push_back({l, c_x, c_y});
    BufferState next = redoStack.back();
    redoStack.pop_back();
    l = next.lines;
    c_x = next.cx;
    c_y = next.cy;
    return true;
  }
};

//------------------------------------------------------------
// Real Filesystem Tree
//------------------------------------------------------------

struct FileNode {
  string name;
  string fullPath;
  bool isDir;
  bool expanded;
  vector<FileNode> children;
};

struct FlatEntry {
  int depth;
  string name;
  string fullPath;
  bool isDir;
  bool expanded;
  bool hasChildren;
};

//------------------------------------------------------------
// Real System Information
//------------------------------------------------------------

struct SystemInfo {
  string osName, cpuModel, shell, terminalName, uptime;
  long totalMemMB = 0, usedMemMB = 0;
  long totalMemKB = 0, usedMemKB = 0;
  long totalDiskGB = 0, usedDiskGB = 0;
  unsigned long long totalDiskBytes = 0, usedDiskBytes = 0;
};

SystemInfo getSystemInfo() {
  SystemInfo info;

  // Shell
  const char *sh = getenv("SHELL");
  if (sh) {
    info.shell = sh;
    size_t p = info.shell.rfind('/');
    if (p != string::npos)
      info.shell = info.shell.substr(p + 1);
  } else
    info.shell = "sh";

  // Terminal name (check common env vars)
  {
    const char *t = getenv("TERM_PROGRAM");
    if (t)
      info.terminalName = t;
    else {
      t = getenv("TERMINAL");
      if (t)
        info.terminalName = t;
      else {
        t = getenv("TERM");
        info.terminalName = t ? t : "terminal";
      }
    }
  }

  // OS from /etc/os-release
  ifstream osr("/etc/os-release");
  string ln;
  while (getline(osr, ln)) {
    if (ln.rfind("PRETTY_NAME=", 0) == 0) {
      info.osName = ln.substr(12);
      if (!info.osName.empty() && info.osName.front() == '"')
        info.osName = info.osName.substr(1, info.osName.size() - 2);
      break;
    }
  }
  if (info.osName.empty()) {
    struct utsname u;
    uname(&u);
    info.osName = u.sysname;
  }

  // CPU model
  ifstream cpi("/proc/cpuinfo");
  while (getline(cpi, ln)) {
    if (ln.rfind("model name", 0) == 0) {
      size_t c = ln.find(':');
      if (c != string::npos) {
        info.cpuModel = ln.substr(c + 2);
        size_t at = info.cpuModel.find('@');
        if (at != string::npos)
          info.cpuModel = info.cpuModel.substr(0, at - 1);
        while (!info.cpuModel.empty() &&
               isspace((unsigned char)info.cpuModel.back()))
          info.cpuModel.pop_back();
      }
      break;
    }
  }

  // Memory from /proc/meminfo
  ifstream mi("/proc/meminfo");
  long memTotal = 0, memAvail = 0;
  while (getline(mi, ln)) {
    if (ln.rfind("MemTotal:", 0) == 0)
      sscanf(ln.c_str() + 9, "%ld", &memTotal);
    else if (ln.rfind("MemAvailable:", 0) == 0)
      sscanf(ln.c_str() + 13, "%ld", &memAvail);
  }
  info.totalMemKB = memTotal;
  info.usedMemKB = memTotal - memAvail;
  info.totalMemMB = memTotal / 1024;
  info.usedMemMB = (memTotal - memAvail) / 1024;

  // Disk from statvfs on cwd
  struct statvfs sv;
  if (statvfs(".", &sv) == 0) {
    unsigned long long bs = sv.f_frsize;
    info.totalDiskBytes = (unsigned long long)sv.f_blocks * bs;
    info.usedDiskBytes = (unsigned long long)(sv.f_blocks - sv.f_bfree) * bs;
    info.totalDiskGB = (long)(info.totalDiskBytes / (1024ULL * 1024 * 1024));
    info.usedDiskGB = (long)(info.usedDiskBytes / (1024ULL * 1024 * 1024));
  }

  // Uptime from /proc/uptime
  ifstream upt("/proc/uptime");
  if (upt.is_open()) {
    double uptSec = 0;
    upt >> uptSec;
    int h = (int)uptSec / 3600, m = ((int)uptSec % 3600) / 60;
    char ubuf[32];
    snprintf(ubuf, sizeof(ubuf), "%dh %dm", h, m);
    info.uptime = ubuf;
  }

  return info;
}

//------------------------------------------------------------
// Global State
//------------------------------------------------------------

struct termios orig_termios;

ScreenView currentView = ScreenView::Home;
EditorMode currentMode = EditorMode::Normal;
HomeSection homeSection =
    HomeSection::Explorer; // which panel has kb focus on home

bool isMaximized = false;
bool shouldExitApp = false;
int animFrame = 0;

// Sidebar / Section collapse state
bool collapseExplorer = true;
bool collapseRecent = true;

vector<string> lines(1);
int cx = 0;
int cy = 0;
int rowOffset = 0;

int screenRows = 24;
int screenCols = 80;

string commandBuffer;
string searchBuffer;
string paletteBuffer;
string savePromptBuffer;
bool isQuittingAfterSave = false;
string currentFile = "index.cpp";
bool isModified = false;

// Open File Tabs & Workspace
vector<string> openTabs = {"index.cpp"};
int activeTabIdx = 0;

// Real File Tree
FileNode ftRoot;            // root of the real filesystem tree
vector<FlatEntry> flatTree; // flattened view for display
int ftCursor = 0;           // selected row in flatTree
int ftScrollOff = 0;        // scroll offset for sidebar

// Recent files (populated as files are opened)
vector<string> recentFiles;
int recentFileCursor = 0;
int quickActionCursor = 0;

// Notification Toast
string toastMsg = "";
time_t toastTime = 0;
int newFileAnimTimer = 0;

void showToast(const string &msg) {
  toastMsg = msg;
  toastTime = time(NULL);
}

// Visual Mode Selection & Mouse Drag State
int selStartX = 0;
int selStartY = 0;
bool isMouseDragging = false;
string yankBuffer = "";

bool showContextMenu = false;
int contextMenuX = 0;
int contextMenuY = 0;

// Matching Brace Highlight Coordinates
int matchRow = -1;
int matchCol = -1;

// Sidebar File Tree State — collapsed by default
bool showFileTree = false;

// LSP & Auto-Completion Overlay State
bool showCompletion = false;
int completionIdx = 0;
vector<string> completionList;
static const vector<string> lspSymbols = {"std::cout",
                                          "std::cin",
                                          "std::endl",
                                          "std::vector",
                                          "std::string",
                                          "std::map",
                                          "std::set",
                                          "std::pair",
                                          "std::make_pair",
                                          "std::unique_ptr",
                                          "std::shared_ptr",
                                          "push_back",
                                          "size",
                                          "substr",
                                          "find",
                                          "length",
                                          "empty",
                                          "clear",
                                          "erase",
                                          "insert",
                                          "begin",
                                          "end",
                                          "printf",
                                          "malloc",
                                          "free",
                                          "main",
                                          "openfile",
                                          "savefile",
                                          "refresh_screen",
                                          "executeCommand",
                                          "<iostream>",
                                          "<vector>",
                                          "<string>",
                                          "<fstream>",
                                          "<algorithm>",
                                          "<map>"};

// Command Palette Registry
struct PaletteItem {
  string name;
  string action;
};

static const vector<PaletteItem> paletteActions = {
    {"Build & Run Current File", "run"},
    {"Save File", "w"},
    {"Save & Quit", "wq"},
    {"Quit Editor", "q"},
    {"Toggle File Explorer", "tree"},
    {"Toggle Embedded Terminal", "term"},
    {"Toggle AI Panel", "ai"},
    {"Switch Color Theme", "theme"}};

int paletteIdx = 0;
vector<PaletteItem> filteredPalette;

// Embedded Terminal Panel State
bool showTerminalPanel = false;
int terminalHeight = 7;
vector<string> terminalOutput;

// AI Assistant Side-Panel State
bool showAiPanel = false;
int aiPanelWidth = 30;
vector<string> aiChatHistory;
string aiInputBuffer;

MouseEvent lastMouseEvent;
bool pendingJ = false;
char lastKeyChar = 0;

UndoManager undoManager;

// Helper: Compute visual column width of a string,
// properly handling ANSI escape sequences and UTF-8 multi-byte characters.
// - ANSI escapes (\033[...m etc.) contribute 0 columns
// - ASCII bytes (0x00-0x7F) contribute 1 column each
// - 2-byte UTF-8 (0xC0-0xDF start) = 1 column
// - 3-byte UTF-8 (0xE0-0xEF start) = 1 column (box drawing, block elements,
// etc.)
// - 4-byte UTF-8 (0xF0-0xF7 start) = 2 columns (emoji)
// - Continuation bytes (0x80-0xBF) are skipped (part of multi-byte sequence)
int visualWidth(const string &str) {
  int width = 0;
  bool inEscape = false;
  size_t i = 0;
  while (i < str.size()) {
    unsigned char ch = (unsigned char)str[i];
    if (ch == '\033') {
      inEscape = true;
      i++;
    } else if (inEscape) {
      // End of escape sequence on a letter (e.g. 'm', 'H', 'J', 'K')
      if (isalpha((int)ch))
        inEscape = false;
      i++;
    } else if (ch < 0x80) {
      // Plain ASCII: 1 column
      width++;
      i++;
    } else if ((ch & 0xE0) == 0xC0) {
      // 2-byte UTF-8 sequence: 1 column
      width++;
      i += 2;
    } else if ((ch & 0xF0) == 0xE0) {
      // 3-byte UTF-8 sequence: 1 column (box drawing, block chars, symbols)
      width++;
      i += 3;
    } else if ((ch & 0xF8) == 0xF0) {
      // 4-byte UTF-8 sequence: 2 columns (emoji)
      width += 2;
      i += 4;
    } else {
      // Continuation byte or invalid — skip
      i++;
    }
  }
  return width;
}

string truncateToVisualWidth(const string &str, int maxW) {
  int width = 0;
  size_t i = 0;
  bool inEscape = false;
  string res = "";
  
  while (i < str.size()) {
    unsigned char ch = (unsigned char)str[i];
    int charWidth = 0;
    size_t charBytes = 1;

    if (ch == '\033') {
      inEscape = true;
    } else if (inEscape) {
      if (isalpha((int)ch))
        inEscape = false;
    } else if (ch < 0x80) {
      charWidth = 1;
    } else if ((ch & 0xE0) == 0xC0) {
      charWidth = 1;
      charBytes = 2;
    } else if ((ch & 0xF0) == 0xE0) {
      charWidth = 1;
      charBytes = 3;
    } else if ((ch & 0xF8) == 0xF0) {
      charWidth = 2;
      charBytes = 4;
    }

    if (!inEscape && width + charWidth > maxW) {
      break; 
    }

    width += charWidth;
    for (size_t b = 0; b < charBytes && i < str.size(); ++b) {
      res += str[i];
      i++;
    }
  }
  return res;
}

// Helper: Repeat a string n times
string repeatStr(const string &s, int n) {
  string r;
  r.reserve(s.size() * n);
  for (int i = 0; i < n; ++i)
    r += s;
  return r;
}

// Helper: Pad a styled string to exactly w visible chars
string padTo(const string &s, int w, char fill = ' ') {
  int vw = visualWidth(s);
  return (vw >= w) ? s : s + string(w - vw, fill);
}

// Helper: Draw a percentage progress bar
string progressBar(int pct, int barWidth) {
  int filled = max(0, min(barWidth, (barWidth * pct) / 100));
  string bar = "\033[38;2;0;255;136m"; // primary green fill
  for (int i = 0; i < barWidth; ++i)
    bar += (i < filled) ? "█" : "\033[38;2;60;60;60m░\033[38;2;0;255;136m";
  bar += "\033[0m";
  return bar;
}

// Helper: Get Current Formatting Date and Time
string getCurrentTimeString() {
  time_t now = time(NULL);
  tm *ltm = localtime(&now);
  char buf[32];
  strftime(buf, sizeof(buf), "%I:%M %p", ltm);
  return string(buf);
}

string getCurrentDateString() {
  time_t now = time(NULL);
  tm *ltm = localtime(&now);
  char buf[32];
  strftime(buf, sizeof(buf), "%d %b %Y", ltm);
  return string(buf);
}

//------------------------------------------------------------
// Function Prototypes
//------------------------------------------------------------

void disableRawMode();
void enableRawMode();
int readkey();

void moveLeft();
void moveRight();
void moveUp();
void moveDown();
void moveWordForward();
void moveWordBackward();
void moveWordEnd();
bool isToolInstalled(const string &toolName);
void executeLanguageFile(const string &filename);


bool getWindowSize(int &rows, int &cols);
void scroll();
void refresh_screen();

void savefile(const string &filename);
void openfile(const string &filename);
void loadFileList();
void autoFormatCode();
void runShellCommand(const string &cmd);
void updateBraceMatching();

void executeCommand(const string &cmd);
string promptSaveAs();
void cleanupScreen();
void handleMouseClick(int mx, int my, int btn, char type);
void saveRecentFiles();
void loadRecentFiles();

//------------------------------------------------------------
// Terminal Setup
//------------------------------------------------------------

void disableRawMode() {
  const char disableMouse[] = "\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?1015l\033[?2004l\033[?1049l\033[?25h";
  ssize_t res = write(STDOUT_FILENO, disableMouse, sizeof(disableMouse) - 1);
  (void)res;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cflag |= CS8;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  const char enableMouse[] = "\033[?1000h\033[?1006h\033[?2004h";
  ssize_t res = write(STDOUT_FILENO, enableMouse, sizeof(enableMouse) - 1);
  (void)res;
}

int readkey() {
  char c;
  if (read(STDIN_FILENO, &c, 1) != 1)
    return -1;

  if (c == '\x1b') {
    struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
    if (poll(&pfd, 1, 25) > 0) {
      char seq[64];
      int n = 0;
      while (n < 63 && read(STDIN_FILENO, &seq[n], 1) == 1) {
        if (seq[n] == 'M' || seq[n] == 'm' || seq[n] == '~' ||
            isalpha(seq[n])) {
          n++;
          break;
        }
        n++;
      }
      seq[n] = '\0';

      if (string(seq) == "[200~") {
        string pastePayload = "";
        char pbuf;
        while (read(STDIN_FILENO, &pbuf, 1) == 1) {
          if (pbuf == '\x1b') {
            char endSeq[16];
            int en = 0;
            while (en < 15 && read(STDIN_FILENO, &endSeq[en], 1) == 1) {
              if (endSeq[en] == '~') { en++; break; }
              en++;
            }
            endSeq[en] = '\0';
            if (string(endSeq) == "[201~") {
              break;
            } else {
              pastePayload += '\x1b';
              pastePayload += endSeq;
            }
          } else {
            pastePayload += pbuf;
          }
        }
        insertTextAtCursor(pastePayload);
        showToast("Pasted from system clipboard");
        return -2;
      }

      if (seq[0] == '[') {
        if (seq[1] == '<') {
          int btn = 0, mouseCol = 0, mouseRow = 0;
          char type = 'M';
          sscanf(seq + 2, "%d;%d;%d%c", &btn, &mouseCol, &mouseRow, &type);
          lastMouseEvent = {mouseCol, mouseRow, btn, type};
          return MOUSE_EVENT;
        }

        if (seq[1] >= '0' && seq[1] <= '9') {
          if (seq[2] == '~') {
            switch (seq[1]) {
            case '1':
              return HOME_KEY;
            case '3':
              return DEL_KEY;
            case '4':
              return END_KEY;
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
            }
          }
        } else {
          switch (seq[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
          }
        }
      }
    }
    return '\x1b';
  }

  return c;
}

void autoFormatCode() {
  int indentLevel = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    string &line = lines[i];
    size_t first = line.find_first_not_of(" \t");
    if (first == string::npos) {
      line = "";
      continue;
    }
    line = line.substr(first);

    if (line[0] == '}') {
      if (indentLevel > 0)
        indentLevel--;
    }

    string indentStr(indentLevel * 2, ' ');
    line = indentStr + line;

    for (size_t j = first; j < line.size(); ++j) {
      if (line[j] == '{')
        indentLevel++;
      else if (line[j] == '}' && j > 0)
        indentLevel = max(0, indentLevel - 1);
    }
  }
}

static std::mutex termMutex;
static unordered_map<string, bool> toolCache;

bool isToolInstalledFast(const string &toolName) {
  auto it = toolCache.find(toolName);
  if (it != toolCache.end()) return it->second;
  string checkCmd = "which " + toolName + " >/dev/null 2>&1";
  bool found = (system(checkCmd.c_str()) == 0);
  toolCache[toolName] = found;
  return found;
}

bool detectInteractiveOrGUI(const string &filename) {
  ifstream f(filename);
  if (!f.is_open()) return false;
  string line;
  while (getline(f, line)) {
    if (line.find("Scanner") != string::npos ||
        line.find("System.in") != string::npos ||
        line.find("cin") != string::npos ||
        line.find("wcin") != string::npos ||
        line.find("scanf") != string::npos ||
        line.find("fscanf") != string::npos ||
        line.find("input(") != string::npos ||
        line.find("raw_input") != string::npos ||
        line.find("Console") != string::npos ||
        line.find("readline") != string::npos ||
        line.find("readLine") != string::npos ||
        line.find("BufferedReader") != string::npos ||
        line.find("InputStreamReader") != string::npos ||
        line.find("getchar") != string::npos ||
        line.find("fgetc") != string::npos ||
        line.find("fgets") != string::npos ||
        line.find("sys.stdin") != string::npos ||
        line.find("getpass") != string::npos ||
        line.find("process.stdin") != string::npos ||
        line.find("read_line") != string::npos ||
        line.find("readln") != string::npos ||
        line.find("read ") != string::npos ||
        line.find("java.awt") != string::npos ||
        line.find("javax.swing") != string::npos ||
        line.find("JFrame") != string::npos ||
        line.find("Frame") != string::npos ||
        line.find("Window") != string::npos ||
        line.find("Panel") != string::npos ||
        line.find("Canvas") != string::npos ||
        line.find("Applet") != string::npos ||
        line.find("JOptionPane") != string::npos ||
        line.find("javafx") != string::npos ||
        line.find("Stage") != string::npos ||
        line.find("Scene") != string::npos ||
        line.find("tkinter") != string::npos ||
        line.find("Tkinter") != string::npos ||
        line.find("PyQt") != string::npos ||
        line.find("PySide") != string::npos ||
        line.find("wx") != string::npos ||
        line.find("pygame") != string::npos ||
        line.find("turtle") != string::npos ||
        line.find("SFML") != string::npos ||
        line.find("sf::") != string::npos ||
        line.find("SDL") != string::npos ||
        line.find("glut") != string::npos ||
        line.find("GLFW") != string::npos ||
        line.find("glfw") != string::npos ||
        line.find("QApplication") != string::npos ||
        line.find("QWidget") != string::npos ||
        line.find("Gtk") != string::npos ||
        line.find("gtk_") != string::npos ||
        line.find("graphics.h") != string::npos ||
        line.find("windows.h") != string::npos ||
        line.find("WinMain") != string::npos ||
        line.find("winit") != string::npos ||
        line.find("egui") != string::npos) {
      f.close();
      return true;
    }
  }
  f.close();
  return false;
}

bool detectInteractiveInput(const string &filename) {
  return detectInteractiveOrGUI(filename);
}

bool runInteractiveCommand(const string &cmd, const string &title) {
  string scriptPath = "/tmp/arc_runner.sh";
  ofstream sf(scriptPath);
  if (sf.is_open()) {
    sf << "#!/usr/bin/env bash\n";
    sf << "clear\n";
    sf << "echo '=====================================================' \n";
    sf << "echo ' 💻 arc Interactive & GUI Session'\n";
    sf << "echo ' 📄 Target: " << title << "'\n";
    sf << "echo '=====================================================' \n";
    sf << "echo\n";
    sf << cmd << "\n";
    sf << "ret=$?\n";
    sf << "echo\n";
    sf << "echo '-----------------------------------------------------'\n";
    sf << "if [ $ret -eq 0 ]; then\n";
    sf << "  echo '✅ Process finished successfully (exit code 0).'\n";
    sf << "else\n";
    sf << "  echo \"⚠️ Process exited with code $ret.\"\n";
    sf << "fi\n";
    sf << "echo\n";
    sf << "read -p 'Press Enter to exit...' key\n";
    sf.close();
    chmod(scriptPath.c_str(), 0755);
  } else {
    return false;
  }

  struct TermPattern {
    string bin;
    string pattern;
  };

  vector<TermPattern> candidates = {
    {"ptyxis", "ptyxis -T \"%TITLE%\" -- bash \"%SCRIPT%\""},
    {"ptyxis", "ptyxis -- bash \"%SCRIPT%\""},
    {"ptyxis", "ptyxis -x \"bash %SCRIPT%\""},
    {"x-terminal-emulator", "x-terminal-emulator -T \"%TITLE%\" -- bash \"%SCRIPT%\""},
    {"x-terminal-emulator", "x-terminal-emulator -- bash \"%SCRIPT%\""},
    {"x-terminal-emulator", "x-terminal-emulator -e \"bash %SCRIPT%\""},
    {"x-terminal-emulator", "x-terminal-emulator -T \"%TITLE%\" -e bash \"%SCRIPT%\""},
    {"gnome-terminal", "gnome-terminal --title=\"%TITLE%\" -- bash \"%SCRIPT%\""},
    {"gnome-terminal", "gnome-terminal -- bash \"%SCRIPT%\""},
    {"konsole", "konsole --title \"%TITLE%\" -e bash \"%SCRIPT%\""},
    {"konsole", "konsole -e bash \"%SCRIPT%\""},
    {"xfce4-terminal", "xfce4-terminal --title=\"%TITLE%\" -x bash \"%SCRIPT%\""},
    {"xfce4-terminal", "xfce4-terminal -e \"bash %SCRIPT%\""},
    {"alacritty", "alacritty -t \"%TITLE%\" -e bash \"%SCRIPT%\""},
    {"kitty", "kitty --title \"%TITLE%\" bash \"%SCRIPT%\""},
    {"xterm", "xterm -T \"%TITLE%\" -e bash \"%SCRIPT%\""},
    {"lxterminal", "lxterminal -t \"%TITLE%\" -e bash \"%SCRIPT%\""},
    {"mate-terminal", "mate-terminal --title=\"%TITLE%\" -e bash \"%SCRIPT%\""}
  };

  string launchedBin = "";
  bool launched = false;

  for (const auto &cand : candidates) {
    if (!isToolInstalledFast(cand.bin)) continue;

    string fullCmd = cand.pattern;
    size_t pos;
    while ((pos = fullCmd.find("%TITLE%")) != string::npos) {
      fullCmd.replace(pos, 7, title);
    }
    while ((pos = fullCmd.find("%SCRIPT%")) != string::npos) {
      fullCmd.replace(pos, 8, scriptPath);
    }

    int res = system((fullCmd + " 2>/dev/null &").c_str());
    if (res == 0) {
      launchedBin = cand.bin;
      launched = true;
      break;
    }
  }

  if (!launched) {
    string bgCmd = "(" + cmd + ") &";
    system(bgCmd.c_str());
    launchedBin = "background process";
  }

  showTerminalPanel = true;
  {
    lock_guard<mutex> lock(termMutex);
    terminalOutput.clear();
    terminalOutput.push_back(" 💻 [arc Shell]: " + cmd);
    terminalOutput.push_back(" 🚀 Interactive / GUI session launched (" + launchedBin + ")");
    terminalOutput.push_back(" 💡 Enter input in the pop-up terminal window / interact with GUI.");
  }

  return true;
}

void runShellCommand(const string &cmd) {
  showTerminalPanel = true;
  {
    lock_guard<mutex> lock(termMutex);
    terminalOutput.clear();
    terminalOutput.push_back(" 💻 [arc Shell]: " + cmd);
  }

  std::thread([cmd]() {
    FILE *fp = popen((cmd + " 2>&1").c_str(), "r");
    if (!fp) {
      lock_guard<mutex> lock(termMutex);
      terminalOutput.push_back(" ❌ Command execution failed.");
      return;
    }

    char buf[512];
    string currentLine = "";

    while (fgets(buf, sizeof(buf), fp) != NULL) {
      string text(buf);
      for (char c : text) {
        if (c == '\n' || c == '\r') {
          if (!currentLine.empty()) {
            lock_guard<mutex> lock(termMutex);
            terminalOutput.push_back(" " + currentLine);
            currentLine.clear();
          }
        } else {
          currentLine += c;
        }
      }
      if (!currentLine.empty()) {
        lock_guard<mutex> lock(termMutex);
        if (!terminalOutput.empty() && terminalOutput.back().rfind(" ", 0) == 0 && terminalOutput.back() != " 💻 [arc Shell]: " + cmd) {
          terminalOutput.back() = " " + currentLine;
        } else {
          terminalOutput.push_back(" " + currentLine);
        }
      }
    }
    if (!currentLine.empty()) {
      lock_guard<mutex> lock(termMutex);
      terminalOutput.push_back(" " + currentLine);
    }

    int code = pclose(fp);
    int exitCode = WIFEXITED(code) ? WEXITSTATUS(code) : code;

    lock_guard<mutex> lock(termMutex);
    if (exitCode == 0) {
      terminalOutput.push_back(" ✅ Process finished successfully (exit code 0).");
    } else {
      terminalOutput.push_back(" ⚠️ Process exited with code " + to_string(exitCode));
    }
  }).detach();
}

string getSystemClipboard() {
  string clipText = "";
  const char* cmds[] = {
    "xclip -selection clipboard -o 2>/dev/null",
    "xclip -o 2>/dev/null",
    "xsel --clipboard --output 2>/dev/null",
    "wl-paste 2>/dev/null",
    "pbpaste 2>/dev/null"
  };

  for (const char* cmd : cmds) {
    FILE *fp = popen(cmd, "r");
    if (fp) {
      char buf[512];
      string result = "";
      while (fgets(buf, sizeof(buf), fp) != NULL) {
        result += buf;
      }
      int status = pclose(fp);
      if (status == 0 && !result.empty()) {
        clipText = result;
        break;
      }
    }
  }
  return clipText;
}

void insertTextAtCursor(const string &text) {
  if (text.empty()) return;
  undoManager.saveSnapshot(lines, cx, cy);

  stringstream ss(text);
  string lineStr;
  vector<string> textLines;
  while (getline(ss, lineStr)) {
    if (!lineStr.empty() && lineStr.back() == '\r') lineStr.pop_back();
    textLines.push_back(lineStr);
  }

  if (textLines.empty()) return;

  if (textLines.size() == 1) {
    lines[cy].insert(cx, textLines[0]);
    cx += textLines[0].size();
  } else {
    string current = lines[cy];
    string left = current.substr(0, cx);
    string right = current.substr(cx);

    lines[cy] = left + textLines[0];
    for (size_t i = 1; i < textLines.size() - 1; ++i) {
      lines.insert(lines.begin() + cy + i, textLines[i]);
    }
    lines.insert(lines.begin() + cy + textLines.size() - 1, textLines.back() + right);
    cy += textLines.size() - 1;
    cx = textLines.back().size();
  }
  isModified = true;
}

void updateBraceMatching() {
  matchRow = -1;
  matchCol = -1;
  if (cy < 0 || cy >= (int)lines.size() || cx < 0 ||
      cx >= (int)lines[cy].size())
    return;

  char ch = lines[cy][cx];
  char matchCh = 0;
  int dir = 0;

  if (ch == '{') {
    matchCh = '}';
    dir = 1;
  } else if (ch == '}') {
    matchCh = '{';
    dir = -1;
  } else if (ch == '(') {
    matchCh = ')';
    dir = 1;
  } else if (ch == ')') {
    matchCh = '(';
    dir = -1;
  } else if (ch == '[') {
    matchCh = ']';
    dir = 1;
  } else if (ch == ']') {
    matchCh = '[';
    dir = -1;
  }

  if (dir == 0)
    return;

  int depth = 0;
  int r = cy, c = cx;

  while (r >= 0 && r < (int)lines.size()) {
    while (c >= 0 && c < (int)lines[r].size()) {
      if (lines[r][c] == ch)
        depth++;
      else if (lines[r][c] == matchCh) {
        depth--;
        if (depth == 0) {
          matchRow = r;
          matchCol = c;
          return;
        }
      }
      c += dir;
    }
    r += dir;
    c = (dir == 1) ? 0 : (r >= 0 ? (int)lines[r].size() - 1 : 0);
  }
}

// Load real directory children, sort dirs first then files
void ftLoadChildren(FileNode &node) {
  if (!node.isDir)
    return;
  node.children.clear();

  DIR *d = opendir(node.fullPath.c_str());
  if (!d)
    return;

  vector<FileNode> dirs, files;
  struct dirent *ent;
  while ((ent = readdir(d)) != NULL) {
    string nm = ent->d_name;
    if (nm == "." || nm == "..")
      continue;
    // Skip hidden files (starting with .)
    if (nm[0] == '.')
      continue;

    FileNode child;
    child.name = nm;
    child.fullPath = node.fullPath + "/" + nm;
    child.expanded = false;

    // Use stat to determine type (more reliable than d_type)
    struct stat st;
    if (stat(child.fullPath.c_str(), &st) == 0)
      child.isDir = S_ISDIR(st.st_mode);
    else
      child.isDir = (ent->d_type == DT_DIR);

    if (child.isDir)
      dirs.push_back(child);
    else
      files.push_back(child);
  }
  closedir(d);

  auto byName = [](const FileNode &a, const FileNode &b) {
    return a.name < b.name;
  };
  sort(dirs.begin(), dirs.end(), byName);
  sort(files.begin(), files.end(), byName);

  for (auto &x : dirs)
    node.children.push_back(move(x));
  for (auto &x : files)
    node.children.push_back(move(x));
}

// Recursively append visible entries to flatTree
void ftBuildFlat(FileNode &node, int depth = 0) {
  FlatEntry e;
  e.depth = depth;
  e.name = node.name;
  e.fullPath = node.fullPath;
  e.isDir = node.isDir;
  e.expanded = node.expanded;
  e.hasChildren = node.isDir;
  flatTree.push_back(e);

  if (node.expanded) {
    for (auto &child : node.children)
      ftBuildFlat(child, depth + 1);
  }
}

// Rebuild the flat display list from the tree
void ftRebuild() {
  flatTree.clear();
  // Show root's children directly (root itself is the cwd)
  for (auto &child : ftRoot.children)
    ftBuildFlat(child, 0);
}

// Find a node by fullPath and toggle its expanded state
bool ftToggle(FileNode &node, const string &path) {
  if (node.fullPath == path) {
    if (node.isDir) {
      node.expanded = !node.expanded;
      if (node.expanded && node.children.empty())
        ftLoadChildren(node);
    }
    return true;
  }
  for (auto &child : node.children)
    if (ftToggle(child, path))
      return true;
  return false;
}

// Initialize the file tree at a given directory path
void loadFileList() {
  char cwdBuf[512];
  string cwd = (getcwd(cwdBuf, sizeof(cwdBuf)) != NULL) ? string(cwdBuf) : ".";

  ftRoot.name = cwd;
  ftRoot.fullPath = cwd;
  ftRoot.isDir = true;
  ftRoot.expanded = true;
  ftRoot.children.clear();

  ftLoadChildren(ftRoot);
  ftRebuild();
  ftCursor = 0;
  ftScrollOff = 0;
}

bool getWindowSize(int &rows, int &cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return false;
  }
  rows = ws.ws_row;
  cols = ws.ws_col;
  return true;
}

void scroll() {
  int reservedRows = 4 + (showTerminalPanel ? terminalHeight : 0);
  int textRows = screenRows - reservedRows;
  if (textRows < 1)
    textRows = 1;

  if (cy < rowOffset)
    rowOffset = cy;
  if (cy >= rowOffset + textRows)
    rowOffset = cy - textRows + 1;
  if (rowOffset < 0)
    rowOffset = 0;
}

void moveLeft() {
  if (cx > 0)
    cx--;
  else if (cy > 0) {
    cy--;
    cx = lines[cy].size();
  }
  updateBraceMatching();
}

void moveRight() {
  if (cx < (int)lines[cy].size())
    cx++;
  else if (cy < (int)lines.size() - 1) {
    cy++;
    cx = 0;
  }
  updateBraceMatching();
}

void moveUp() {
  if (cy > 0) {
    cy--;
    if (cx > (int)lines[cy].size())
      cx = lines[cy].size();
  }
  updateBraceMatching();
}

void moveDown() {
  if (cy < (int)lines.size() - 1) {
    cy++;
    if (cx > (int)lines[cy].size())
      cx = lines[cy].size();
  }
  updateBraceMatching();
}

void moveWordForward() {
  if (cy >= (int)lines.size())
    return;
  string &line = lines[cy];
  int len = line.size();
  if (cx >= len) {
    if (cy < (int)lines.size() - 1) {
      cy++;
      cx = 0;
    }
    return;
  }
  while (cx < len && (isalnum(line[cx]) || line[cx] == '_'))
    cx++;
  while (cx < len && isspace(line[cx]))
    cx++;
  if (cx >= len && cy < (int)lines.size() - 1) {
    cy++;
    cx = 0;
  }
  updateBraceMatching();
}

void moveWordBackward() {
  if (cx == 0 && cy == 0)
    return;
  if (cx == 0 && cy > 0) {
    cy--;
    cx = lines[cy].size();
  }
  string &line = lines[cy];
  if (cx > 0)
    cx--;
  while (cx > 0 && isspace(line[cx]))
    cx--;
  while (cx > 0 && (isalnum(line[cx - 1]) || line[cx - 1] == '_'))
    cx--;
  updateBraceMatching();
}

void moveWordEnd() {
  if (lines.empty() || cy < 0 || cy >= (int)lines.size())
    return;
  string &line = lines[cy];
  int len = line.size();
  if (len == 0) {
    if (cy + 1 < (int)lines.size()) {
      cy++;
      cx = 0;
    }
    return;
  }

  // If currently on word character and next char is word char, advance to end of current word
  if (cx < len && (isalnum((unsigned char)line[cx]) || line[cx] == '_')) {
    if (cx + 1 < len && (isalnum((unsigned char)line[cx + 1]) || line[cx + 1] == '_')) {
      cx++;
      while (cx + 1 < len && (isalnum((unsigned char)line[cx + 1]) || line[cx + 1] == '_')) {
        cx++;
      }
      updateBraceMatching();
      return;
    }
  }

  // Advance past non-word chars/spaces to find next word
  cx++;
  while (cx < len && !(isalnum((unsigned char)line[cx]) || line[cx] == '_')) {
    cx++;
  }

  // If end of line, wrap to next line
  while (cx >= len && cy + 1 < (int)lines.size()) {
    cy++;
    cx = 0;
    string &nxt = lines[cy];
    len = nxt.size();
    while (cx < len && !(isalnum((unsigned char)nxt[cx]) || nxt[cx] == '_')) {
      cx++;
    }
  }

  if (cy < (int)lines.size()) {
    string &curr = lines[cy];
    len = curr.size();
    if (cx < len && (isalnum((unsigned char)curr[cx]) || curr[cx] == '_')) {
      while (cx + 1 < len && (isalnum((unsigned char)curr[cx + 1]) || curr[cx + 1] == '_')) {
        cx++;
      }
    } else {
      if (cx > len) cx = len;
    }
  }
  updateBraceMatching();
}

bool isToolInstalled(const string &toolName) {
  string checkCmd = "which " + toolName + " >/dev/null 2>&1";
  return (system(checkCmd.c_str()) == 0);
}

void executeLanguageFile(const string &filename) {
  if (filename.empty()) {
    showToast("Error: No open file to execute!");
    return;
  }

  savefile(filename);

  string ext;
  size_t dot = filename.rfind('.');
  if (dot != string::npos) {
    ext = filename.substr(dot);
    for (auto &c : ext) c = tolower(c);
  }

  string basename = filename;
  if (dot != string::npos) basename = filename.substr(0, dot);
  size_t slash = basename.rfind('/');
  if (slash != string::npos) basename = basename.substr(slash + 1);

  try {
    bool isInteractive = detectInteractiveInput(filename);

    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".hpp") {
      if (!isToolInstalledFast("g++") && !isToolInstalledFast("clang++")) {
        showToast("Error: g++ compiler not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: C++ compiler (g++) is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install build-essential");
        showTerminalPanel = true;
        return;
      }
      string compiler = isToolInstalledFast("g++") ? "g++" : "clang++";
      string runCmd = compiler + " -O2 \"" + filename + "\" -o \"/tmp/" + basename + "\" && \"/tmp/" + basename + "\"";
      showToast("Compiling & Executing C++...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".c") {
      if (!isToolInstalledFast("gcc") && !isToolInstalledFast("clang")) {
        showToast("Error: gcc compiler not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: C compiler (gcc) is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install gcc");
        showTerminalPanel = true;
        return;
      }
      string compiler = isToolInstalledFast("gcc") ? "gcc" : "clang";
      string runCmd = compiler + " -O2 \"" + filename + "\" -o \"/tmp/" + basename + "\" && \"/tmp/" + basename + "\"";
      showToast("Compiling & Executing C...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".py") {
      if (!isToolInstalledFast("python3") && !isToolInstalledFast("python")) {
        showToast("Error: Python interpreter not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: Python (python3) is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install python3");
        showTerminalPanel = true;
        return;
      }
      string pyTool = isToolInstalledFast("python3") ? "python3" : "python";
      string runCmd = pyTool + " -u \"" + filename + "\"";
      showToast("Executing Python script...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".java") {
      if (!isToolInstalledFast("java") && !isToolInstalledFast("javac")) {
        showToast("Error: Java JDK/JRE (java/javac) not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: Java (java/javac) is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install default-jdk");
        showTerminalPanel = true;
        return;
      }

      string className = basename;
      string packageName = "";

      // Parse class name and package directly from disk file
      ifstream f(filename);
      if (f.is_open()) {
        string l;
        while (getline(f, l)) {
          size_t pkgPos = l.find("package ");
          if (pkgPos != string::npos) {
            size_t start = pkgPos + 8;
            while (start < l.size() && isspace(l[start])) start++;
            size_t end = start;
            while (end < l.size() && (isalnum(l[end]) || l[end] == '_' || l[end] == '.')) end++;
            if (end > start) packageName = l.substr(start, end - start);
          }
          size_t p = l.find("class ");
          if (p != string::npos) {
            size_t start = p + 6;
            while (start < l.size() && isspace(l[start])) start++;
            size_t end = start;
            while (end < l.size() && (isalnum(l[end]) || l[end] == '_')) end++;
            if (end > start) {
              string candidate = l.substr(start, end - start);
              if (l.find("public class") != string::npos) {
                className = candidate;
                break;
              }
              if (className == basename) className = candidate;
            }
          }
        }
        f.close();
      }

      string dirPath = ".";
      string fileOnly = filename;
      size_t slash = filename.rfind('/');
      if (slash != string::npos) {
        dirPath = filename.substr(0, slash);
        fileOnly = filename.substr(slash + 1);
      }

      string fullClassTarget = packageName.empty() ? className : (packageName + "." + className);

      string runCmd;
      if (isToolInstalledFast("javac")) {
        runCmd = "cd \"" + dirPath + "\" && javac -d /tmp \"" + fileOnly + "\" && java -cp /tmp \"" + fullClassTarget + "\"";
      } else {
        runCmd = "cd \"" + dirPath + "\" && java \"" + fileOnly + "\"";
      }

      showToast("Compiling & Executing Java...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".js" || ext == ".mjs" || ext == ".cjs") {
      if (!isToolInstalledFast("node") && !isToolInstalledFast("nodejs")) {
        showToast("Error: Node.js runtime not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: Node.js (node) JavaScript runtime is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install nodejs");
        showTerminalPanel = true;
        return;
      }
      string nodeTool = isToolInstalledFast("node") ? "node" : "nodejs";
      string runCmd = nodeTool + " \"" + filename + "\"";
      showToast("Executing JavaScript...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".html" || ext == ".htm") {
      if (!isToolInstalledFast("xdg-open") && !isToolInstalledFast("firefox") && !isToolInstalledFast("google-chrome")) {
        showToast("Error: Browser launcher not found!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: No browser or xdg-open found to open HTML.");
        showTerminalPanel = true;
        return;
      }
      string launcher = isToolInstalledFast("xdg-open") ? "xdg-open" : (isToolInstalledFast("firefox") ? "firefox" : "google-chrome");
      string runCmd = launcher + " \"" + filename + "\" &";
      showToast("Opening HTML in Browser...");
      runShellCommand(runCmd);
    }
    else if (ext == ".rs") {
      if (!isToolInstalledFast("rustc")) {
        showToast("Error: Rust compiler (rustc) not installed!");
        terminalOutput.clear();
        terminalOutput.push_back("❌ Error: Rust compiler (rustc) is not installed.");
        terminalOutput.push_back("💡 Install via terminal: sudo apt install rustc or curl https://sh.rustup.rs");
        showTerminalPanel = true;
        return;
      }
      string runCmd = "rustc \"" + filename + "\" -o \"/tmp/" + basename + "\" && \"/tmp/" + basename + "\"";
      showToast("Compiling & Executing Rust...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else if (ext == ".sh" || ext == ".bash") {
      string runCmd = "bash \"" + filename + "\"";
      showToast("Executing Bash script...");
      if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
      runShellCommand(runCmd);
    }
    else {
      if (isToolInstalledFast("g++")) {
        string runCmd = "g++ \"" + filename + "\" -o \"/tmp/" + basename + "\" && \"/tmp/" + basename + "\"";
        showToast("Compiling & Executing...");
        if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
        runShellCommand(runCmd);
      } else {
        showToast("Executing...");
        string runCmd = "./\"" + filename + "\"";
        if (isInteractive && runInteractiveCommand(runCmd, basename)) return;
        runShellCommand(runCmd);
      }
    }
  } catch (const std::exception &e) {
    showToast(string("Exception: ") + e.what());
    terminalOutput.clear();
    terminalOutput.push_back(string("⚠️ Exception during execution: ") + e.what());
    showTerminalPanel = true;
  } catch (...) {
    showToast("Exception during execution!");
    terminalOutput.clear();
    terminalOutput.push_back("⚠️ Exception: Unknown error occurred while running command.");
    showTerminalPanel = true;
  }
}


bool isInsideSelection(int lineIdx, int colIdx) {
  if (currentMode != EditorMode::Visual)
    return false;
  int startY = selStartY, startX = selStartX;
  int endY = cy, endX = cx;
  if (startY > endY || (startY == endY && startX > endX)) {
    swap(startY, endY);
    swap(startX, endX);
  }
  if (lineIdx < startY || lineIdx > endY)
    return false;
  if (lineIdx == startY && lineIdx == endY)
    return colIdx >= startX && colIdx <= endX;
  if (lineIdx == startY)
    return colIdx >= startX;
  if (lineIdx == endY)
    return colIdx <= endX;
  return true;
}

void getSelectionRange(int &startY, int &startX, int &endY, int &endX) {
  startY = selStartY;
  startX = selStartX;
  endY = cy;
  endX = cx;
  if (startY > endY || (startY == endY && startX > endX)) {
    swap(startY, endY);
    swap(startX, endX);
  }
}

void deleteSelection() {
  int startY, startX, endY, endX;
  getSelectionRange(startY, startX, endY, endX);
  undoManager.saveSnapshot(lines, cx, cy);
  if (startY == endY) {
    string &line = lines[startY];
    if (startX < (int)line.size())
      line.erase(startX, endX - startX + 1);
  } else {
    string firstLineLeft = lines[startY].substr(0, startX);
    string lastLineRight =
        (endX < (int)lines[endY].size()) ? lines[endY].substr(endX + 1) : "";
    lines[startY] = firstLineLeft + lastLineRight;
    lines.erase(lines.begin() + startY + 1, lines.begin() + endY + 1);
  }
  cy = startY;
  cx = startX;
  if (lines.empty())
    lines.push_back("");
  isModified = true;
  currentMode = EditorMode::Normal;
}

void yankSelection() {
  int startY, startX, endY, endX;
  getSelectionRange(startY, startX, endY, endX);
  yankBuffer = "";
  if (startY == endY) {
    yankBuffer = lines[startY].substr(startX, endX - startX + 1);
  } else {
    for (int r = startY; r <= endY; ++r) {
      if (r == startY)
        yankBuffer += lines[r].substr(startX) + "\n";
      else if (r == endY)
        yankBuffer += lines[r].substr(0, endX + 1);
      else
        yankBuffer += lines[r] + "\n";
    }
  }
  currentMode = EditorMode::Normal;
}

void setSystemClipboard(const string &text) {
  yankBuffer = text;
  if (text.empty()) return;

  const char* cmds[] = {
    "xclip -selection clipboard 2>/dev/null",
    "xsel --clipboard --input 2>/dev/null",
    "wl-copy 2>/dev/null"
  };

  for (const char* cmd : cmds) {
    FILE *fp = popen(cmd, "w");
    if (fp) {
      fwrite(text.c_str(), 1, text.size(), fp);
      pclose(fp);
      break;
    }
  }
}

void executeContextMenuCopy() {
  if (currentMode == EditorMode::Visual) {
    yankSelection();
    setSystemClipboard(yankBuffer);
  } else {
    string lineText = (cy >= 0 && cy < (int)lines.size()) ? lines[cy] : "";
    yankBuffer = lineText;
    setSystemClipboard(lineText);
  }
  showToast("Copied to clipboard!");
}

void executeContextMenuCut() {
  if (currentMode == EditorMode::Visual) {
    yankSelection();
    setSystemClipboard(yankBuffer);
    deleteSelection();
  } else {
    if (cy >= 0 && cy < (int)lines.size()) {
      undoManager.saveSnapshot(lines, cx, cy);
      yankBuffer = lines[cy];
      setSystemClipboard(lines[cy]);
      lines.erase(lines.begin() + cy);
      if (lines.empty()) lines.push_back("");
      if (cy >= (int)lines.size()) cy = max(0, (int)lines.size() - 1);
      cx = min(cx, (int)lines[cy].size());
      isModified = true;
    }
  }
  showToast("Cut to clipboard!");
}

void executeContextMenuPaste() {
  string textToPaste = getSystemClipboard();
  if (textToPaste.empty()) textToPaste = yankBuffer;
  if (!textToPaste.empty()) {
    insertTextAtCursor(textToPaste);
    showToast("Pasted from clipboard!");
  } else {
    showToast("Clipboard is empty");
  }
}

void executeContextMenuQuit() {
  disableRawMode();
  cleanupScreen();
  shouldExitApp = true;
  exit(0);
}

string highlightLine(const string &line, int lineIdx) {
  const Theme &theme = themes[activeThemeIdx];
  string res = "";
  size_t len = line.size();
  size_t i = 0;

  static const vector<string> keywords = {
      "int",      "char",      "void",   "double",    "float",     "bool",
      "class",    "struct",    "public", "private",   "protected", "return",
      "if",       "else",      "while",  "for",       "switch",    "case",
      "break",    "continue",  "const",  "constexpr", "auto",      "template",
      "typename", "namespace", "using",  "include",   "true",      "false"};

  size_t leadingSpaces = 0;
  while (leadingSpaces < len && line[leadingSpaces] == ' ')
    leadingSpaces++;

  while (i < len) {
    if ((lineIdx == cy && (int)i == cx && (matchRow != -1)) ||
        (lineIdx == matchRow && (int)i == matchCol)) {
      res += "\033[48;2;0;255;136m\033[38;2;0;0;0m";
      res += line[i++];
      res += "\033[0m";
      continue;
    }

    if (isInsideSelection(lineIdx, i)) {
      res += "\033[48;2;0;191;255m\033[38;2;0;0;0m";
      res += line[i++];
      res += "\033[0m";
      continue;
    }

    if (!searchBuffer.empty() && currentMode == EditorMode::Search) {
      if (line.substr(i, searchBuffer.size()) == searchBuffer) {
        res += "\033[48;2;240;180;0m\033[38;2;0;0;0m";
        res += searchBuffer;
        res += "\033[0m";
        i += searchBuffer.size();
        continue;
      }
    }

    if (i < leadingSpaces && i % 2 == 0) {
      res += "\033[38;2;60;60;60m│\033[0m";
      i++;
      continue;
    }

    if (i + 1 < len && line[i] == '/' && line[i + 1] == '/') {
      res += "\033[38;2;106;153;85m" + line.substr(i) + "\033[0m";
      break;
    }

    if (line[i] == '#') {
      res += "\033[38;2;197;134;192m";
      while (i < len && !isspace(line[i]))
        res += line[i++];
      res += "\033[0m";
      continue;
    }

    if (line[i] == '"' || line[i] == '\'') {
      char quote = line[i];
      res += "\033[38;2;206;145;120m";
      res += line[i++];
      while (i < len && line[i] != quote) {
        if (line[i] == '\\' && i + 1 < len)
          res += line[i++];
        res += line[i++];
      }
      if (i < len)
        res += line[i++];
      res += "\033[0m";
      continue;
    }

    if (isdigit(line[i])) {
      res += "\033[38;2;181;206;168m";
      while (i < len && (isdigit(line[i]) || line[i] == '.' || line[i] == 'x'))
        res += line[i++];
      res += "\033[0m";
      continue;
    }

    if (isalpha(line[i]) || line[i] == '_') {
      size_t start = i;
      while (i < len && (isalnum(line[i]) || line[i] == '_'))
        i++;
      string word = line.substr(start, i - start);
      if (find(keywords.begin(), keywords.end(), word) != keywords.end()) {
        res += theme.primary + word + "\033[0m";
      } else {
        res += theme.secondary + word + "\033[0m";
      }
      continue;
    }

    res += theme.text;
    res += line[i++];
    res += "\033[0m";
  }

  return res;
}

//------------------------------------------------------------
// Dashboard Renderer — Real Data, Premium Layout
//------------------------------------------------------------

// Box-drawing card helpers (use repeatStr defined above)
string cardTop(const string &title, int innerW, const string &bc,
               const string &rs) {
  int fill = max(0, innerW - (int)title.size() - 3);
  return bc + "╭─ " + rs + "\033[1m" + title + "\033[0m" + bc + " " +
         repeatStr("─", fill) + "╮" + rs;
}
string cardRow(const string &content, int innerW, const string &bc,
               const string &rs) {
  return bc + "│" + rs + " " + padTo(content, innerW) + " " + bc + "│" + rs;
}
string cardBot(int innerW, const string &bc, const string &rs) {
  return bc + "╰" + repeatStr("─", innerW + 2) + "╯" + rs;
}

// ──────────────────────────────────────────────────────────
//  TOP HEADER BAR
// ──────────────────────────────────────────────────────────
void renderHeaderBar(string &frame, int width) {
  const Theme &theme = themes[activeThemeIdx];
  const string &bc = theme.border;
  const string &pc = theme.primary;
  const string &sc = theme.secondary;
  const string &ic = theme.inactive;
  const string rs = "\033[0m";

  string brand = "\u25b2 arc  v1.0.0";
  string mid = "Modern Terminal Text Editor";
  string right = "Theme: " + themes[activeThemeIdx].name;
  // Window controls — [ − ] [ □ ] [ × ]
  string wins = " \u2500 [\u2212] [\u25a1] [\u00d7] \u2500\u256e";

  // Calculate padding so all three sections are evenly spaced
  int brandL = visualWidth(brand) + 4; // "╭─ " + brand
  int rightL = (int)right.size() + visualWidth(wins) + 2;
  int midL = (int)mid.size();
  int avail = width - brandL - rightL - midL - 4;
  int padL = max(2, avail / 2);
  int padR = max(2, avail - padL);

  frame += bc + "\u256d\u2500 " + pc + brand + rs + bc +
           repeatStr("\u2500", padL) + " " + sc + mid + rs + bc + " " +
           repeatStr("\u2500", padR) + " " + ic + right + rs + bc + wins + rs +
           "\n";
}

// ──────────────────────────────────────────────────────────
//  HOME DASHBOARD
// ──────────────────────────────────────────────────────────
void renderHomeScreen(string &frame) {
  getWindowSize(screenRows, screenCols);
  const Theme &theme = themes[activeThemeIdx];
  const string &bc = theme.border;
  const string &pc = theme.primary;
  const string &sc = theme.secondary;
  const string &tc = theme.text;
  const string &ic = theme.inactive;
  const string rs = "\033[0m";
  const string bg = "\033[48;2;8;8;14m";

  frame += "\033[2J\033[H\033[?25l";

  // Real data
  char cwdBuf[512];
  string cwd = (getcwd(cwdBuf, sizeof(cwdBuf)) != NULL) ? string(cwdBuf) : ".";
  static SystemInfo si;
  static time_t siT = 0;
  if (time(NULL) - siT > 5) {
    si = getSystemInfo();
    siT = time(NULL);
  }

  // Layout: outer frame (header=top, footer=bottom) + cmdbar + statusbar
  // = 5 fixed rows, rest = contentRows
  int contentRows = screenRows - 5;
  if (contentRows < 8)
    contentRows = 8;
  int SBW =
      (!showFileTree || isMaximized) ? 0 : max(28, (int)(screenCols * 0.22));
  int SBi = SBW > 2 ? SBW - 2 : 0;
  int gap = SBW > 0 ? 1 : 0;
  // outer frame uses 2 cols (left + right │), 1 space padding each side inside
  // outer = 4 cols inner panels: SBW + gap + MBW fill the inner width
  // (screenCols-3, between outer │ bars, leaving 1 space to prevent terminal auto-wrap)
  int MBW = screenCols - 3 - (SBW > 0 ? SBW + gap : 0);
  int MBi = MBW > 2 ? MBW - 2 : 0;

  // Banner — ARCHIOID with neon glow
  // Use a shadow/glow effect: dim copy behind, bright primary on top
  string neonGlow = "\033[38;2;0;180;100m"; // subtle glow undercolor
  vector<string> banner = {
      "  \u2588\u2588\u2588\u2588\u2588\u2588\u2557 "
      "\u2588\u2588\u2588\u2588\u2588\u2588\u2557  "
      "\u2588\u2588\u2588\u2588\u2588\u2588\u2557\u2588\u2588\u2557  "
      "\u2588\u2588\u2557\u2588\u2588\u2557 "
      "\u2588\u2588\u2588\u2588\u2588\u2588\u2557 "
      "\u2588\u2588\u2557\u2588\u2588\u2588\u2588\u2588\u2588\u2557 ",
      " \u2588\u2588\u2554\u2550\u2550\u2588\u2588\u2557\u2588\u2588\u2554"
      "\u2550\u2550\u2588\u2588\u2557\u2588\u2588\u2554\u2550\u2550\u2550\u2550"
      "\u255d\u2588\u2588\u2551  "
      "\u2588\u2588\u2551\u2588\u2588\u2551\u2588\u2588\u2554\u2550\u2550\u2550"
      "\u2588\u2588\u2557\u2588\u2588\u2551\u2588\u2588\u2554\u2550\u2550\u2588"
      "\u2588\u2557",
      " \u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2551\u2588\u2588\u2588"
      "\u2588\u2588\u2588\u2554\u255d\u2588\u2588\u2551     "
      "\u2588\u2588\u2588\u2588\u2588\u2588\u2588\u2551\u2588\u2588\u2551\u2588"
      "\u2588\u2551   \u2588\u2588\u2551\u2588\u2588\u2551\u2588\u2588\u2551  "
      "\u2588\u2588\u2551",
      " \u2588\u2588\u2554\u2550\u2550\u2588\u2588\u2551\u2588\u2588\u2554"
      "\u2550\u2550\u2588\u2588\u2557\u2588\u2588\u2551     "
      "\u2588\u2588\u2554\u2550\u2550\u2588\u2588\u2551\u2588\u2588\u2551\u2588"
      "\u2588\u2551   \u2588\u2588\u2551\u2588\u2588\u2551\u2588\u2588\u2551  "
      "\u2588\u2588\u2551",
      " \u2588\u2588\u2551  \u2588\u2588\u2551\u2588\u2588\u2551  "
      "\u2588\u2588\u2551\u255a\u2588\u2588\u2588\u2588\u2588\u2588\u2557\u2588"
      "\u2588\u2551  "
      "\u2588\u2588\u2551\u2588\u2588\u2551\u255a\u2588\u2588\u2588\u2588\u2588"
      "\u2588\u2554\u255d\u2588\u2588\u2551\u2588\u2588\u2588\u2588\u2588\u2588"
      "\u2554\u255d",
      " \u255a\u2550\u255d  \u255a\u2550\u255d\u255a\u2550\u255d  "
      "\u255a\u2550\u255d "
      "\u255a\u2550\u2550\u2550\u2550\u2550\u255d\u255a\u2550\u255d  "
      "\u255a\u2550\u255d\u255a\u2550\u255d "
      "\u255a\u2550\u2550\u2550\u2550\u2550\u255d "
      "\u255a\u2550\u255d\u255a\u2550\u2550\u2550\u2550\u2550\u255d"};

  // Build sidebar content lines (SBi wide each)
  vector<string> sbL;
  if (SBW > 0) {
    string dp = cwd;
    if ((int)dp.size() > SBi)
      dp = "\u2026" + dp.substr(dp.size() - SBi + 1);
    sbL.push_back(sc + dp + rs);
    int treeR = contentRows - 5;
    int shown = 0;
    for (int i = ftScrollOff; i < (int)flatTree.size() && shown < treeR;
         ++i, ++shown) {
      const FlatEntry &e = flatTree[i];
      string ind(e.depth * 2, ' ');
      string ico = e.isDir ? (e.expanded ? "\u25bc " : "\u25b6 ") : "\u25a1 ";
      string lbl = ind + ico + e.name;
      if (visualWidth(lbl) > SBi - 1) {
        while (visualWidth(lbl) > SBi - 2)
          lbl.pop_back();
        lbl += "+";
      }
      bool sel = (i == ftCursor && homeSection == HomeSection::Explorer);
      bool cur = (!currentFile.empty() && e.fullPath == currentFile);
      if (sel)
        sbL.push_back("\033[48;2;0;70;45m" + pc + " " + lbl + rs);
      else if (cur && !e.isDir)
        sbL.push_back("\033[48;2;0;45;28m" + pc + " " + lbl + rs);
      else if (e.isDir)
        sbL.push_back(sc + " " + lbl + rs);
      else
        sbL.push_back(ic + " " + lbl + rs);
    }
    while ((int)sbL.size() < contentRows - 4)
      sbL.push_back("");
    sbL.push_back(bc + repeatStr("\u2500", SBi) + rs);
    sbL.push_back(ic + "\u2191/k \u2193/j  Enter: Open  Bs: Up" + rs);
    while ((int)sbL.size() < contentRows - 2)
      sbL.push_back("");
  }

  // Build main content lines (MBi wide each)
  vector<string> ml;
  auto MB = [&]() { ml.push_back(string(MBi, ' ')); };
  auto MC = [&](const string &t, const string &c) {
    int tw = visualWidth(t), lp = max(0, (MBi - tw) / 2);
    ml.push_back(string(lp, ' ') + c + t + rs +
                 string(max(0, MBi - lp - tw), ' '));
  };
  auto MA = [&](const string &s) {
    ml.push_back(s + string(max(0, MBi - visualWidth(s)), ' '));
  };

  MB();
  // Render banner with neon glow: first a dim shadow line, then bright
  for (auto &bl : banner) {
    int tw = visualWidth(bl), lp = max(0, (MBi - tw) / 2);
    // Neon glow effect — render with bright primary + subtle underline glow
    ml.push_back(string(lp, ' ') + pc + "\033[1m" + bl + "\033[0m" +
                 string(max(0, MBi - lp - tw), ' '));
  }
  // Neon glow underline bar
  {
    int barW = min(MBi - 4, 52), lp2 = max(0, (MBi - barW) / 2);
    ml.push_back(string(lp2, ' ') + neonGlow + repeatStr("\u2581", barW) + rs +
                 string(max(0, MBi - lp2 - barW), ' '));
  }
  MB();
  MC("\u26a1 Fast  \u2022  \u2728 Beautiful  \u2022  \U0001f916 AI Powered  "
     "\u2022  \u2699 LSP Enabled",
     sc);
  MB();
  // CWD box
  {
    int bI = min(MBi - 4, max(38, (int)cwd.size() + 10));
    int lp = max(0, (MBi - bI - 2) / 2);
    string h = "CURRENT DIRECTORY";
    MA(string(lp, ' ') + bc + "\u256d\u2500 " + rs + "\033[1m" + pc + h +
       "\033[0m" + bc + " " +
       repeatStr("\u2500", max(0, bI - (int)h.size() - 3)) + "\u256e" + rs);
    string dt = " \U0001f4c2 " + cwd, btn = " [\u2398]";
    int fl = max(1, bI - visualWidth(dt) - visualWidth(btn));
    MA(string(lp, ' ') + bc + "\u2502" + rs + pc + dt + string(fl, ' ') + rs +
       sc + btn + rs + bc + "\u2502" + rs);
    MA(string(lp, ' ') + bc + "\u2570" + repeatStr("\u2500", bI + 2) +
       "\u256f" + rs);
  }
  MB();
  // 3-col grid
  {
    int sp = 1, cw = max(12, (MBi - sp * 4) / 3 - 2);
    int mP =
        si.totalMemKB > 0 ? (int)(100LL * si.usedMemKB / si.totalMemKB) : 0;
    int dP = si.totalDiskBytes > 0
                 ? (int)(100ULL * si.usedDiskBytes / si.totalDiskBytes)
                 : 0;
    // Recent files rows
    vector<string> rec;
    if (recentFiles.empty())
      rec.push_back(ic + "  (no recent files)" + rs);
    else
      for (int i = 0; i < (int)recentFiles.size() && i < 8; ++i) {
        const string &f = recentFiles[i];
        string nm = f;
        size_t sl = f.rfind('/');
        if (sl != string::npos)
          nm = f.substr(sl + 1);
        string dp2 = "";
        if (sl != string::npos && sl > 0) {
          size_t s2 = f.rfind('/', sl - 1);
          dp2 = f.substr(s2 != string::npos ? s2 + 1 : 0,
                         sl - (s2 != string::npos ? s2 + 1 : 0)) +
                "/";
        }
        if ((int)nm.size() > cw - 8)
          nm = nm.substr(0, cw - 9) + "+";
        bool sel =
            (i == recentFileCursor && homeSection == HomeSection::RecentFiles);
        rec.push_back(
            sel ? "\033[48;2;0;70;45m" + pc + " \u25b6 " + nm + rs + ic +
                      string(max(1, cw - (int)nm.size() - (int)dp2.size() - 3),
                             ' ') +
                      dp2 + rs
                : ic + "   " + nm + rs + ic +
                      string(max(1, cw - (int)nm.size() - (int)dp2.size() - 3),
                             ' ') +
                      dp2 + rs);
      }
    rec.push_back(ic + "(j/k navigate  Enter: open)" + rs);
    // Quick actions
    vector<string> act;
    auto A = [&](const string &i2, const string &l, const string &k) -> string {
      int used = visualWidth(i2) + 1 + (int)l.size() + (int)k.size();
      return pc + i2 + " " + tc + l + string(max(1, cw - used), ' ') + ic + k +
             rs;
    };
    act.push_back(A("\u26a1", "Command Palette", "Ctrl+P"));
    act.push_back(A("\u25a1", "New File", "Ctrl+N"));
    act.push_back(A("\u25a3", "Open File", "Ctrl+O"));
    act.push_back(A("\u25a3", "Open Folder", "Ctrl+Shift+O"));
    act.push_back(A("\u25b6", "Embedded Terminal", "Ctrl+T"));
    act.push_back(A("\u25cb", "AI Assistant", "Ctrl+A"));
    act.push_back(A("\u25c6", "Switch Theme", ":theme"));
    // System info
    vector<string> inf;
    auto II = [&](const string &i2, const string &l,
                  const string &v) -> string {
      int used = visualWidth(i2) + 1 + (int)l.size() + (int)v.size();
      return sc + i2 + " " + ic + l + string(max(1, cw - used), ' ') + tc + v +
             rs;
    };
    string osS = si.osName.empty() ? "Linux" : si.osName;
    if ((int)osS.size() > cw - 8)
      osS = osS.substr(0, cw - 9) + "+";
    string cpS = si.cpuModel.empty() ? "\u2014" : si.cpuModel;
    if ((int)cpS.size() > cw - 8)
      cpS = cpS.substr(0, cw - 9) + "+";
    char mBf[40], dBf[40];
    snprintf(mBf, sizeof(mBf), "%.1f / %.1f GiB", si.usedMemMB / 1024.0f,
             si.totalMemMB / 1024.0f);
    snprintf(dBf, sizeof(dBf), "%ld / %ld GiB", si.usedDiskGB, si.totalDiskGB);
    inf.push_back(II("\u25a1", "OS      :", osS));
    inf.push_back(II("\u25a1", "Shell   :", si.shell));
    inf.push_back(II("\u25a1", "Terminal:",
                     si.terminalName.empty() ? "terminal" : si.terminalName));
    inf.push_back(II("\u25a1", "arc Editor:", "v1.0.0"));
    inf.push_back(
        II("\u25a1", "Uptime  :", si.uptime.empty() ? "\u2014" : si.uptime));
    inf.push_back(II("\u25a1", "CPU     :", cpS));
    {
      string mv = string(mBf) + " " + to_string(mP) + "%";
      inf.push_back(II("\u25a1", "Memory  :", mv));
      inf.push_back(" " + progressBar(mP, cw - 2));
    }
    {
      string dv = string(dBf) + " " + to_string(dP) + "%";
      inf.push_back(II("\u25a1", "Disk    :", dv));
      inf.push_back(" " + progressBar(dP, cw - 2));
    }
    int nR = max({(int)rec.size(), (int)act.size(), (int)inf.size()});
    while ((int)rec.size() < nR)
      rec.push_back("");
    while ((int)act.size() < nR)
      act.push_back("");
    while ((int)inf.size() < nR)
      inf.push_back("");
    string rH = "RECENT FILES" + (homeSection == HomeSection::RecentFiles
                                      ? string(" \u25ba")
                                      : string(" [-]")),
           sS(sp, ' ');
    MA(sS + cardTop(rH, cw, bc, rs) + sS +
       cardTop("QUICK ACTIONS", cw, sc, rs) + sS +
       cardTop("SYSTEM INFO", cw, pc, rs));
    for (int i = 0; i < nR; ++i)
      MA(sS + cardRow(padTo(rec[i], cw), cw, bc, rs) + sS +
         cardRow(padTo(act[i], cw), cw, sc, rs) + sS +
         cardRow(padTo(inf[i], cw), cw, pc, rs));
    MA(sS + cardBot(cw, bc, rs) + sS + cardBot(cw, sc, rs) + sS +
       cardBot(cw, pc, rs));
  }
  MB();
  MC(ic + "[ESC] Editor  [Tab] Focus  [Ctrl+B] Sidebar" + rs, "");
  while ((int)ml.size() < contentRows - 2)
    MB();

  // ── RENDER ALL ROWS ──────────────────────────────────────────

  // Row 0: Header = outer top border ╭─ ARCHIOID ... [−][□][×] ─╮
  {
    string brand = "\u25b2 arc  v1.0.0", mid = "Modern Terminal Text Editor";
    string right = "Theme: " + themes[activeThemeIdx].name,
           wins = " \u2500 [\u2212] [\u25a1] [\u00d7] \u2500\u256e";
    int bL = visualWidth(brand) + 4,
        rL = (int)right.size() + visualWidth(wins) + 2, mL = (int)mid.size();
    int av = screenCols - bL - rL - mL - 4, pL = max(2, av / 2),
        pR = max(2, av - pL);
    frame += bg + bc + "\u256d\u2500 " + pc + brand + rs + bg + bc +
             repeatStr("\u2500", pL) + " " + sc + mid + rs + bg + bc + " " +
             repeatStr("\u2500", pR) + " " + ic + right + rs + bg + bc + wins +
             rs + "\n";
  }

  // Rows 1..contentRows: content rows (each: outer-left + inner panels +
  // outer-right)
  for (int r = 0; r < contentRows; ++r) {
    frame += bg + bc + "\u2502" + rs + bg;

    if (SBW > 0) {
      if (r == 0) {
        // Sidebar top border
        string T = "PROJECT EXPLORER", B = " [-]";
        int fl = max(0, SBi - (int)T.size() - (int)B.size() - 2);
        frame += bc + "\u256d\u2500 " + rs + "\033[1m" + pc + T + rs + bc +
                 repeatStr("\u2500", fl) + ic + B + rs + bc + "\u256e" + rs;
      } else if (r == contentRows - 1) {
        frame += bc + "\u2570" + repeatStr("\u2500", SBi) + "\u256f" + rs;
      } else {
        int si2 = r - 1;
        string sbc = (si2 < (int)sbL.size()) ? sbL[si2] : "";
        int vw = visualWidth(sbc), pad = max(0, SBi - vw);
        frame +=
            bc + "\u2502" + rs + sbc + string(pad, ' ') + bc + "\u2502" + rs;
      }
      frame += string(gap, ' ');
    }

    if (r == 0) {
      frame += bc + "\u256d" + repeatStr("\u2500", MBi) + "\u256e" + rs;
    } else if (r == contentRows - 1) {
      frame += bc + "\u2570" + repeatStr("\u2500", MBi) + "\u256f" + rs;
    } else {
      int mi = r - 1;
      string mc = (mi < (int)ml.size()) ? ml[mi] : "";
      int vw = visualWidth(mc), pad = max(0, MBi - vw);
      frame += bc + "\u2502" + rs + mc + string(pad, ' ') + bc + "\u2502" + rs;
    }

    frame += bg + bc + "\u2502" + rs + "\n";
  }

  // Outer bottom border ╰──────────────────────────────────────╯
  frame += bg + bc + "\u2570" + repeatStr("\u2500", screenCols - 3) + "\u256f" +
           rs + "\n";

  // Command bar (styled background, : prompt, NORMAL badge)
  {
    string prompt;
    if (currentMode == EditorMode::Command)
      prompt = "\033[48;2;12;12;20m" + pc + ": " + rs + "\033[48;2;12;12;20m" +
               tc + commandBuffer + rs;
    else
      prompt = "\033[48;2;12;12;20m" + bc + " : " + rs + "\033[48;2;12;12;20m" +
               ic + "Type a command (:q to quit, :help for help)" + rs;
    string badge = "\033[48;2;0;90;0m\033[38;2;0;255;136m NORMAL \033[0m";
    int pvw =
        4 + (int)string("Type a command (:q to quit, :help for help)").size();
    frame += prompt + "\033[48;2;12;12;20m" +
             string(max(1, screenCols - pvw - 9), ' ') + rs + badge + "\n";
  }

  // Status bar
  {
    string NM = "\033[48;2;0;255;136m\033[38;2;0;0;0m NORMAL \033[0m";
    string bar =
        "\033[48;2;15;15;22m" + ic + " UTF-8 \u2502 C++ \u2502 Ln 1 Col 1" +
        " \u2502 LSP \u2713 \u2502 AI \u2713" + " \u2502 " +
        getCurrentTimeString() + " \u2502 " + getCurrentDateString() + " " + rs;
    string pulse = pc + " \u2500\u028c\u2500 " + rs;
    frame += NM + bar + pulse;
    if (!toastMsg.empty() && time(NULL) - toastTime < 3)
      frame += "  \033[48;2;0;122;204m\033[38;2;255;255;255m " + toastMsg +
               " \033[0m";
    frame += "\033[K" + rs;
  }
}

void refresh_screen() {
  getWindowSize(screenRows, screenCols);
  if (screenRows < 3)
    screenRows = 3;
  if (screenCols < 20)
    screenCols = 20;

  const Theme &theme = themes[activeThemeIdx];
  Layout layout =
      LayoutManager::calculate(screenRows, screenCols, showFileTree,
                               showTerminalPanel, terminalHeight, isMaximized);

  string frame = "";
  frame.reserve(screenRows * screenCols + 4096);

  if (currentView == ScreenView::Home) {
    renderHomeScreen(frame);
  } else {

  scroll();

  int textRows = layout.main.height;
  int lineNumWidth = to_string(lines.size()).size();
  if (lineNumWidth < 2)
    lineNumWidth = 2;

  // Hide cursor and reset to top — clear entire screen once for clean redraw
  frame += "\033[?25l\033[H";

  // --- Window Control & Header Bar ---
  renderHeaderBar(frame, screenCols);

  // --- Tab Bar ---
  int sidebarW = layout.sidebar.width; // 0 when sidebar hidden
  if (showFileTree && !isMaximized && sidebarW > 0) {
    int innerSB = sidebarW - 1;
    string sbHeader = " 📁 EXPLORER";
    if ((int)sbHeader.size() < innerSB) {
      sbHeader += string(innerSB - sbHeader.size(), ' ');
    } else {
      sbHeader = sbHeader.substr(0, innerSB);
    }
    frame += "\033[48;2;24;28;38m\033[38;2;0;255;136m\033[1m" + sbHeader +
             "\033[0m" + theme.secondary + "│\033[0m";
  }

  frame += "\033[48;2;30;30;30m\033[38;2;200;200;200m 📑 ";
  for (size_t t = 0; t < openTabs.size(); ++t) {
    if ((int)t == activeTabIdx) {
      frame += "\033[48;2;0;122;204m\033[38;2;255;255;255m " + openTabs[t];
      if (isModified)
        frame += " [+] ";
      else
        frame += " ";
      frame += "\033[48;2;30;30;30m\033[38;2;200;200;200m│";
    } else {
      frame += " " + openTabs[t] + " │";
    }
  }
  frame += "\033[K\033[0m\n";

  // --- Main Viewport (Sidebar | Editor Buffer | AI Panel) ---
  for (int screenRow = 0; screenRow < textRows; ++screenRow) {
    int fileRow = rowOffset + screenRow;

    // Sidebar File Explorer Tree (Uniform background & alignment)
    if (showFileTree && !isMaximized && sidebarW > 0) {
      int innerSB = sidebarW - 1; // -1 for the separator │
      bool isSelected = (screenRow == ftCursor);
      string bgCol = isSelected ? "\033[48;2;35;45;65m" : "\033[48;2;18;18;18m";
      string fgCol = isSelected ? ("\033[1m" + theme.primary) : "\033[38;2;200;205;215m";
      frame += bgCol + fgCol;

      if (screenRow < (int)flatTree.size()) {
        string arrow = flatTree[screenRow].isDir
                           ? (flatTree[screenRow].expanded ? "▼ " : "▶ ")
                           : "  ";
        string indent = string(flatTree[screenRow].depth * 2, ' ');
        string fitem = " " + arrow + indent + flatTree[screenRow].name;

        int fitemVisW = visualWidth(fitem);
        if (fitemVisW > innerSB) {
          fitem = truncateToVisualWidth(fitem, innerSB);
          fitemVisW = visualWidth(fitem);
        }
        frame += fitem;
        if (fitemVisW < innerSB) {
          frame += string(innerSB - fitemVisW, ' ');
        }
      } else {
        frame += string(innerSB, ' ');
      }
      frame += "\033[0m" + theme.secondary + "│\033[0m";
    }

    // Editor Content Buffer
    if (fileRow < (int)lines.size()) {
      string num = to_string(fileRow + 1);
      if ((int)num.size() < lineNumWidth) {
        num = string(lineNumWidth - num.size(), ' ') + num;
      }
      frame += "\033[38;2;110;110;110m" + num + " \u2502 \033[0m";
      
      int treeOffset = (showFileTree && !isMaximized && layout.sidebar.width > 0) ? layout.sidebar.width : 0;
      int maxTextWidth = screenCols - lineNumWidth - 4 - treeOffset - 1; // -1 for safety margin against right edge
      
      string rawLine = lines[fileRow];
      // Replace non-printable ASCII (like null bytes in binaries) with dots to prevent terminal corruption
      for (char &c : rawLine) {
        if ((unsigned char)c < 32 && c != '\t' && c != '\033') {
          c = '.';
        }
      }
      
      string truncatedLine = truncateToVisualWidth(rawLine, max(0, maxTextWidth));
      frame += highlightLine(truncatedLine, fileRow);
    } else {
      frame += "\033[38;2;90;90;90m~\033[0m";
    }

    // Clear rest of line to eliminate artifacts
    frame += "\033[K";

    // AI Panel (rendered after \033[K would clear it, so use absolute
    // positioning)
    if (showAiPanel && !isMaximized) {
      // Position AI panel at right edge
      int aiCol = screenCols - aiPanelWidth;
      frame +=
          "\033[" + to_string(screenRow + 3) + ";" + to_string(aiCol) + "H";
      frame +=
          "\033[38;2;175;0;215m\u2502\033[48;2;25;25;25m\033[38;2;220;220;220m";
      string aiLine = "";
      if (screenRow == 0)
        aiLine = " \u256d\u2500 Archioid AI \u2500\u256e";
      else if (screenRow == 1)
        aiLine = " \u251c1. Explain Code ";
      else if (screenRow == 2)
        aiLine = " \u251c2. Find Bug     ";
      else if (screenRow == 3)
        aiLine = " \u251c3. Optimize     ";
      else if (screenRow - 4 < (int)aiChatHistory.size())
        aiLine = " " + aiChatHistory[screenRow - 4];

      if ((int)aiLine.size() > aiPanelWidth - 1)
        aiLine = aiLine.substr(0, aiPanelWidth - 1);
      frame += aiLine;
      int aiPad = aiPanelWidth - 1 - (int)aiLine.size();
      if (aiPad > 0)
        frame += string(aiPad, ' ');
      frame += "\033[0m";
    }

    if (screenRow < textRows - 1)
      frame += "\n";
  }

  // LSP Completion Dropdown Overlay
  if (showCompletion && currentMode == EditorMode::Insert &&
      !completionList.empty()) {
    int overlayRow = cy - rowOffset + 4;
    int treeOffset = (showFileTree && !isMaximized) ? layout.sidebar.width : 0;
    int overlayCol = cx + lineNumWidth + 4 + treeOffset;

    for (size_t c = 0; c < completionList.size() && c < 5; ++c) {
      frame += "\033[" + to_string(overlayRow + c) + ";" +
               to_string(overlayCol) + "H";
      if ((int)c == completionIdx) {
        frame += "\033[48;2;0;255;136m\033[38;2;0;0;0m ▸ " + completionList[c] +
                 " \033[0m";
      } else {
        frame += "\033[48;2;45;45;48m\033[38;2;220;220;220m   " +
                 completionList[c] + " \033[0m";
      }
    }
  }

  // Interactive Command Palette Popup (Ctrl+P)
  if (currentMode == EditorMode::Palette) {
    int pWidth = 45;
    int pCol = max(1, (screenCols - pWidth) / 2);
    int pRow = 5;

    frame += "\033[" + to_string(pRow) + ";" + to_string(pCol) + "H" +
             theme.primary +
             "╭─ Command Palette ─────────────────────────╮\033[0m";
    frame += "\033[" + to_string(pRow + 1) + ";" + to_string(pCol) +
             "H\033[48;2;30;30;30m\033[38;2;255;255;255m│ > " + paletteBuffer;
    while ((int)paletteBuffer.size() < pWidth - 6)
      frame += " ";
    frame += "│\033[0m";

    for (size_t i = 0; i < filteredPalette.size() && i < 6; ++i) {
      frame += "\033[" + to_string(pRow + 2 + i) + ";" + to_string(pCol) + "H";
      if ((int)i == paletteIdx) {
        frame += "\033[48;2;0;122;204m\033[38;2;255;255;255m│ ▸ " +
                 filteredPalette[i].name + "\033[0m";
      } else {
        frame += "\033[48;2;20;20;20m\033[38;2;200;200;200m│   " +
                 filteredPalette[i].name + "\033[0m";
      }
    }
    frame += "\033[" + to_string(pRow + 8) + ";" + to_string(pCol) + "H" +
             theme.primary +
             "╰───────────────────────────────────────────╯\033[0m";
  }

  // Interactive Save File Box Popup (Ctrl+S / Ctrl+Q / Save As)
  if (currentMode == EditorMode::SavePrompt) {
    int boxWidth = 56;
    int innerW = boxWidth - 2;
    int boxCol = max(1, (screenCols - boxWidth) / 2);
    int boxRow = max(2, (screenRows - 8) / 2);

    string headerTitle = isQuittingAfterSave ? " 💾 SAVE & QUIT EDITOR " : " 💾 SAVE FILE AS ";
    int titleW = visualWidth(headerTitle);
    int fillLen = max(0, innerW - titleW);
    string leftPad = repeatStr("─", fillLen / 2);
    string rightPad = repeatStr("─", fillLen - fillLen / 2);

    string bgBox = "\033[48;2;18;22;32m";
    string fgBorder = "\033[38;2;0;255;136m";
    string fgTitle = "\033[48;2;0;122;204m\033[38;2;255;255;255m\033[1m";
    string fgLabel = "\033[38;2;0;215;255m\033[1m";
    string rst = "\033[0m";

    // Row 1: Top Border with Centered Badge
    frame += "\033[" + to_string(boxRow) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "\033[1m╭" + leftPad + fgTitle + headerTitle +
             rst + bgBox + fgBorder + "\033[1m" + rightPad + "╮" + rst;

    // Row 2: Empty Padding Line
    frame += "\033[" + to_string(boxRow + 1) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│" + string(innerW, ' ') + "│" + rst;

    // Row 3: Instruction Label Line
    string promptText = " 📄 Enter file name:";
    int promptW = visualWidth(promptText);
    string line3Pad = string(max(0, innerW - promptW), ' ');
    frame += "\033[" + to_string(boxRow + 2) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│" + fgLabel + promptText + line3Pad + fgBorder + "│" + rst;

    // Row 4: Empty Padding Line
    frame += "\033[" + to_string(boxRow + 3) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│" + string(innerW, ' ') + "│" + rst;

    // Row 5: Input Field Container Box
    string inputContent = savePromptBuffer;
    int maxInputLen = innerW - 6;
    if (visualWidth(inputContent) > maxInputLen) {
      inputContent = inputContent.substr(inputContent.size() - maxInputLen);
    }
    int inputW = visualWidth(inputContent);
    string inputPad = string(max(0, maxInputLen - inputW), ' ');

    frame += "\033[" + to_string(boxRow + 4) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│  \033[48;2;34;40;58m\033[38;2;0;255;136m ▶ \033[38;2;255;255;255m\033[1m" +
             inputContent + inputPad + " \033[0m" + bgBox + "  " + fgBorder + "│" + rst;

    // Row 6: Empty Padding Line
    frame += "\033[" + to_string(boxRow + 5) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│" + string(innerW, ' ') + "│" + rst;

    // Row 7: Divider Line
    frame += "\033[" + to_string(boxRow + 6) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "├" + repeatStr("─", innerW) + "┤" + rst;

    // Row 8: Action Control Buttons Line
    string btnSave = isQuittingAfterSave ? " Save & Quit " : " Save File ";
    string hintsText = "  \033[48;2;0;255;136m\033[38;2;0;0;0m\033[1m [Enter] " + btnSave +
                       "\033[0m" + bgBox + "   \033[48;2;50;55;75m\033[38;2;255;255;255m\033[1m [Esc] Cancel \033[0m" + bgBox;
    int hintsW = 2 + (9 + (int)btnSave.size()) + 3 + 14;
    int hintPadLen = max(0, innerW - hintsW);
    string hintLeftPad = string(hintPadLen / 2, ' ');
    string hintRightPad = string(hintPadLen - hintPadLen / 2, ' ');
    frame += "\033[" + to_string(boxRow + 7) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "│" + hintLeftPad + hintsText + hintRightPad + fgBorder + "│" + rst;

    // Row 9: Bottom Border
    frame += "\033[" + to_string(boxRow + 8) + ";" + to_string(boxCol) + "H" +
             bgBox + fgBorder + "╰" + repeatStr("─", innerW) + "╯" + rst;
  }

  // Embedded Terminal Panel Box Layout (100% Solid Opaque Box Layout - No Transparency)
  if (showTerminalPanel && !isMaximized && layout.terminal.height > 0) {
    int termStartY = layout.terminal.y;
    int termH = layout.terminal.height;
    int termW = screenCols;

    // Top Border of Box Layout
    int topFillW = max(0, termW - 35);
    string topBorder = "\033[" + to_string(termStartY) + ";1H" +
                       "\033[48;2;18;22;32m" + theme.primary + "\033[1m" +
                       "┌─ 💻 EMBEDDED TERMINAL (Ctrl+T) " + repeatStr("─", topFillW) + "┐\033[0m";
    frame += topBorder;

    int contentRows = max(1, termH - 2); // available content lines inside box
    int contentW = max(0, termW - 2);   // available content width inside box

    // Auto-scroll to show latest lines of terminal output
    int startIdx = 0;
    if ((int)terminalOutput.size() > contentRows) {
      startIdx = (int)terminalOutput.size() - contentRows;
    }

    for (int r = 0; r < contentRows; ++r) {
      int rowY = termStartY + 1 + r;
      frame += "\033[" + to_string(rowY) + ";1H";

      // Left Border
      frame += "\033[48;2;15;17;24m" + theme.primary + "│\033[0m";

      // Content with 100% solid opaque background (no transparency)
      frame += "\033[48;2;15;17;24m\033[38;2;220;225;235m";
      string tline = "";
      int outIdx = startIdx + r;
      if (outIdx >= 0 && outIdx < (int)terminalOutput.size()) {
        tline = terminalOutput[outIdx];
      }
      if ((int)tline.size() > contentW) {
        tline = tline.substr(0, contentW);
      }
      frame += tline;
      if ((int)tline.size() < contentW) {
        frame += string(contentW - tline.size(), ' ');
      }

      // Right Border
      frame += "\033[48;2;15;17;24m" + theme.primary + "│\033[0m";
    }

    // Bottom Border of Box Layout
    int bottomY = termStartY + termH - 1;
    int botFillW = max(0, termW - 2);
    string botBorder = "\033[" + to_string(bottomY) + ";1H" +
                       "\033[48;2;18;22;32m" + theme.primary + "\033[1m" +
                       "└" + repeatStr("─", botFillW) + "┘\033[0m";
    frame += botBorder;
  }

  // --- Fixed Status Bar ---
  frame += "\033[" + to_string(layout.status.y) + ";1H";

  string modeBadge = "";
  switch (currentMode) {
  case EditorMode::Normal:
    modeBadge = "\033[48;2;0;122;204m\033[38;2;255;255;255m NORMAL \033[0m";
    break;
  case EditorMode::Insert:
    modeBadge = "\033[48;2;0;255;136m\033[38;2;0;0;0m INSERT \033[0m";
    break;
  case EditorMode::Visual:
    modeBadge = "\033[48;2;0;191;255m\033[38;2;0;0;0m VISUAL \033[0m";
    break;
  case EditorMode::Command:
    modeBadge = "\033[48;2;215;135;0m\033[38;2;255;255;255m COMMAND \033[0m";
    break;
  case EditorMode::Search:
    modeBadge = "\033[48;2;240;180;0m\033[38;2;0;0;0m SEARCH \033[0m";
    break;
  case EditorMode::AiInput:
    modeBadge = "\033[48;2;175;0;215m\033[38;2;255;255;255m AI-PROMPT \033[0m";
    break;
  case EditorMode::Palette:
    modeBadge = "\033[48;2;0;215;255m\033[38;2;0;0;0m PALETTE \033[0m";
    break;
  case EditorMode::SavePrompt:
    modeBadge = "\033[48;2;0;255;136m\033[38;2;0;0;0m SAVE \033[0m";
    break;
  }

  string status = modeBadge + " \033[48;2;30;30;30m\033[38;2;220;220;220m Ln " +
                  to_string(cy + 1) + " Col " + to_string(cx + 1) +
                  " \u2502 UTF-8 \u2502 C++ \u2502 " + currentFile +
                  (isModified ? " [*]" : "") +
                  " \u2502 clangd \u2713 \u2502 AI \u2713 \u2502 " +
                  getCurrentTimeString();

  frame += "\033[" + to_string(layout.status.y) + ";1H" + status + "\033[K\033[0m";

  // --- Command Line Area ---
  string cmdArea = "";
  if (currentMode == EditorMode::Command) {
    cmdArea = theme.primary + ":\033[38;2;255;255;255m" + commandBuffer;
  } else if (currentMode == EditorMode::Search) {
    cmdArea = "\033[38;2;240;180;0mFind: \033[38;2;255;255;255m" + searchBuffer;
  } else if (currentMode == EditorMode::AiInput) {
    cmdArea = "\033[38;2;175;0;215mAI > \033[38;2;255;255;255m" + aiInputBuffer;
  } else if (currentMode == EditorMode::SavePrompt) {
    cmdArea = "\033[38;2;0;255;136mSave As > \033[38;2;255;255;255m" + savePromptBuffer;
  } else {
    cmdArea = "\033[38;2;120;120;120mPress : for commands | Ctrl+P Palette | "
              "Ctrl+F Find | Ctrl+A AI | Esc Home\033[0m";
  }

  frame += "\033[" + to_string(layout.command.y) + ";1H" + cmdArea + "\033[K\033[0m";

  // Cursor positioning — mode aware and accounts for sidebar width
  int cursorRow = cy - rowOffset + 3;
  int treeOffset = (showFileTree && !isMaximized && layout.sidebar.width > 0)
                       ? layout.sidebar.width
                       : 0;
  int cursorCol = visualWidth(lines[cy].substr(0, cx)) + lineNumWidth + 4 + treeOffset;

  if (currentMode == EditorMode::Command) {
    cursorRow = layout.command.y;
    cursorCol = 2 + visualWidth(commandBuffer);
  } else if (currentMode == EditorMode::Search) {
    cursorRow = layout.command.y;
    cursorCol = 7 + visualWidth(searchBuffer);
  } else if (currentMode == EditorMode::AiInput) {
    cursorRow = layout.command.y;
    cursorCol = 6 + visualWidth(aiInputBuffer);
  } else if (currentMode == EditorMode::Palette) {
    int pWidth = 45;
    int pCol = max(1, (screenCols - pWidth) / 2);
    int pRow = 5;
    cursorRow = pRow + 1;
    cursorCol = pCol + 4 + visualWidth(paletteBuffer);
  } else if (currentMode == EditorMode::SavePrompt) {
    int boxWidth = 56;
    int innerW = boxWidth - 2;
    int boxCol = max(1, (screenCols - boxWidth) / 2);
    int boxRow = max(2, (screenRows - 8) / 2);
    cursorRow = boxRow + 4;
    string inputContent = savePromptBuffer;
    int maxInputLen = innerW - 6;
    if (visualWidth(inputContent) > maxInputLen) {
      inputContent = inputContent.substr(inputContent.size() - maxInputLen);
    }
    cursorCol = boxCol + 6 + visualWidth(inputContent);
  }

  // Right-Click Context Menu Overlay
  if (showContextMenu) {
    int cmW = 24;
    int cmH = 7;
    int pCol = max(1, min(screenCols - cmW + 1, contextMenuX));
    int pRow = max(1, min(screenRows - cmH + 1, contextMenuY));

    int hoverIdx = -1;
    if (lastMouseEvent.x >= pCol && lastMouseEvent.x < pCol + cmW &&
        lastMouseEvent.y >= pRow && lastMouseEvent.y < pRow + cmH) {
      hoverIdx = lastMouseEvent.y - pRow;
    }

    string bgBox = "\033[48;2;22;27;38m";
    string fgBorder = "\033[38;2;0;215;255m\033[1m";
    string fgTitle = "\033[38;2;0;255;136m\033[1m";
    string fgNormal = "\033[38;2;240;240;240m";
    string fgHover = "\033[48;2;0;122;204m\033[38;2;255;255;255m\033[1m";
    string rst = "\033[0m";

    // Row 0: Top Border
    frame += "\033[" + to_string(pRow) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "╭─ " + fgTitle + "ContextMenu" + fgBorder + " ─────╮" + rst;

    // Row 1: Copy
    string line1Style = (hoverIdx == 1) ? fgHover : (bgBox + fgNormal);
    frame += "\033[" + to_string(pRow + 1) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "│ " + line1Style + "📋  Copy               " + bgBox + fgBorder + "│" + rst;

    // Row 2: Cut
    string line2Style = (hoverIdx == 2) ? fgHover : (bgBox + fgNormal);
    frame += "\033[" + to_string(pRow + 2) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "│ " + line2Style + "✂️   Cut                " + bgBox + fgBorder + "│" + rst;

    // Row 3: Paste
    string line3Style = (hoverIdx == 3) ? fgHover : (bgBox + fgNormal);
    frame += "\033[" + to_string(pRow + 3) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "│ " + line3Style + "📥  Paste              " + bgBox + fgBorder + "│" + rst;

    // Row 4: Divider
    frame += "\033[" + to_string(pRow + 4) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "├──────────────────────┤" + rst;

    // Row 5: Quit
    string line5Style = (hoverIdx == 5) ? fgHover : (bgBox + "\033[38;2;255;85;85m\033[1m");
    frame += "\033[" + to_string(pRow + 5) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "│ " + line5Style + "❌  Quit               " + bgBox + fgBorder + "│" + rst;

    // Row 6: Bottom Border
    frame += "\033[" + to_string(pRow + 6) + ";" + to_string(pCol) + "H" +
             bgBox + fgBorder + "╰──────────────────────╯" + rst;
  }

  if (cursorRow >= 1 && cursorRow <= screenRows) {
    frame += "\033[" + to_string(cursorRow) + ";" + to_string(cursorCol) + "H";
  }

  frame += "\033[?25h";

  }

  ssize_t res = write(STDOUT_FILENO, frame.c_str(), frame.size());
  (void)res;
}

//------------------------------------------------------------
// File Operations
//------------------------------------------------------------

void savefile(const string &filename) {
  autoFormatCode();
  ofstream file(filename);
  if (!file.is_open())
    return;

  for (size_t i = 0; i < lines.size(); ++i) {
    file << lines[i];
    if (i < lines.size() - 1)
      file << "\n";
  }

  currentFile = filename;
  isModified = false;
  showToast("Saved file: " + filename);
}

void openfile(const string &filename) {
  ifstream file(filename);
  string line;

  lines.clear();
  if (!file.is_open()) {
    lines.push_back("");
    currentFile = filename;
    isModified = false;
    return;
  }

  while (getline(file, line))
    lines.push_back(line);
  if (lines.empty())
    lines.push_back("");

  currentFile = filename;
  isModified = false;
  cx = 0;
  cy = 0;
  rowOffset = 0;

  if (find(openTabs.begin(), openTabs.end(), filename) == openTabs.end())
    openTabs.push_back(filename);
  for (size_t i = 0; i < openTabs.size(); ++i)
    if (openTabs[i] == filename)
      activeTabIdx = i;

  // Track in recent files (deduplicate, prepend, max 10)
  recentFiles.erase(remove(recentFiles.begin(), recentFiles.end(), filename),
                    recentFiles.end());
  recentFiles.insert(recentFiles.begin(), filename);
  if ((int)recentFiles.size() > 10)
    recentFiles.resize(10);
  saveRecentFiles();

  showToast("Opened: " + filename);
}

void createNewFile() {
  lines.clear();
  lines.push_back("");
  currentFile = "Untitled";
  if (find(openTabs.begin(), openTabs.end(), "Untitled") == openTabs.end()) {
    openTabs.push_back("Untitled");
  }
  for (size_t i = 0; i < openTabs.size(); ++i) {
    if (openTabs[i] == "Untitled") {
      activeTabIdx = i;
    }
  }
  isModified = false;
  cx = 0;
  cy = 0;
  rowOffset = 0;
  newFileAnimTimer = 25;
  currentMode = EditorMode::Normal;
  currentView = ScreenView::Editor;
  showToast("Created new file");
}

void saveRecentFiles() {
  const char *home = getenv("HOME");
  if (!home)
    return;
  ofstream f(string(home) + "/.arc_recent");
  for (auto &r : recentFiles)
    f << r << "\n";
}

void loadRecentFiles() {
  const char *home = getenv("HOME");
  if (!home)
    return;
  ifstream f(string(home) + "/.arc_recent");
  string line;
  recentFiles.clear();
  while (getline(f, line)) {
    if (!line.empty()) {
      // Only add if file still exists
      struct stat st;
      if (stat(line.c_str(), &st) == 0)
        recentFiles.push_back(line);
    }
    if ((int)recentFiles.size() >= 10)
      break;
  }
}

void executeCommand(const string &cmd) {
  string cleanCmd = cmd;
  while (!cleanCmd.empty() && (cleanCmd[0] == ':' || cleanCmd[0] == ' ')) {
    cleanCmd.erase(0, 1);
  }
  while (!cleanCmd.empty() && cleanCmd.back() == ' ') {
    cleanCmd.pop_back();
  }

  if (cleanCmd.empty()) return;

  if (cleanCmd == "new" || cleanCmd == "newfile") {
    createNewFile();
    return;
  }

  // Open / Edit file command: open <file>, edit <file>, e <file>, o <file>
  if (cleanCmd.rfind("open ", 0) == 0 || cleanCmd.rfind("edit ", 0) == 0 ||
      cleanCmd.rfind("e ", 0) == 0 || cleanCmd.rfind("o ", 0) == 0) {
    size_t sp = cleanCmd.find(' ');
    string filename = cleanCmd.substr(sp + 1);
    while (!filename.empty() && filename[0] == ' ') filename.erase(0, 1);
    while (!filename.empty() && filename.back() == ' ') filename.pop_back();
    if (!filename.empty()) {
      openfile(filename);
      currentView = ScreenView::Editor;
    }
    return;
  }

  // Write / Save file command: w, write, save, w <file>, write <file>, save <file>
  if (cleanCmd == "w" || cleanCmd == "write" || cleanCmd == "save" ||
      cleanCmd.rfind("w ", 0) == 0 || cleanCmd.rfind("write ", 0) == 0 || cleanCmd.rfind("save ", 0) == 0) {
    string fnameArg;
    if (cleanCmd == "w" || cleanCmd == "write" || cleanCmd == "save") {
      fnameArg = currentFile.empty() ? "text.txt" : currentFile;
    } else {
      size_t sp = cleanCmd.find(' ');
      fnameArg = cleanCmd.substr(sp + 1);
      while (!fnameArg.empty() && fnameArg[0] == ' ') fnameArg.erase(0, 1);
      while (!fnameArg.empty() && fnameArg.back() == ' ') fnameArg.pop_back();
    }
    if (fnameArg.empty()) fnameArg = "text.txt";

    savefile(fnameArg);
    currentFile = fnameArg;
    if (lines.empty()) lines.push_back("");
    if (find(openTabs.begin(), openTabs.end(), fnameArg) == openTabs.end()) {
      openTabs.push_back(fnameArg);
    }
    for (size_t i = 0; i < openTabs.size(); ++i) {
      if (openTabs[i] == fnameArg) activeTabIdx = (int)i;
    }
    loadFileList();
    showToast("File saved: " + fnameArg);
    return;
  }

  // Write & Quit command: wq, wq <file>, x, x <file>
  if (cleanCmd == "wq" || cleanCmd.rfind("wq ", 0) == 0 || cleanCmd == "x" || cleanCmd.rfind("x ", 0) == 0) {
    string fnameArg;
    if (cleanCmd == "wq" || cleanCmd == "x") {
      fnameArg = currentFile.empty() ? "text.txt" : currentFile;
    } else {
      size_t sp = cleanCmd.find(' ');
      fnameArg = cleanCmd.substr(sp + 1);
      while (!fnameArg.empty() && fnameArg[0] == ' ') fnameArg.erase(0, 1);
      while (!fnameArg.empty() && fnameArg.back() == ' ') fnameArg.pop_back();
    }
    if (fnameArg.empty()) fnameArg = "text.txt";

    savefile(fnameArg);
    currentFile = fnameArg;
    shouldExitApp = true;
    return;
  }

  // Quit command: q, q!
  if (cleanCmd == "q" || cleanCmd == "q!") {
    shouldExitApp = true;
    return;
  }

  // Clear command: clear, cls, c, clear errors, clear term, clear all
  if (cleanCmd == "clear" || cleanCmd == "cls" || cleanCmd == "c" ||
      cleanCmd.rfind("clear", 0) == 0 || cleanCmd.rfind("cls", 0) == 0) {
    terminalOutput.clear();
    toastMsg.clear();
    showTerminalPanel = true;
    terminalOutput.push_back(" 💻 [ARCHIOID Terminal Output Cleared]");
    showToast("Cleared terminal output");
    return;
  }

  // Tree toggle: tree
  if (cleanCmd == "tree") {
    showFileTree = !showFileTree;
    if (showFileTree) loadFileList();
    return;
  }

  // Terminal toggle: term, terminal
  if (cleanCmd == "term" || cleanCmd == "terminal") {
    showTerminalPanel = !showTerminalPanel;
    return;
  }

  // AI panel toggle: ai
  if (cleanCmd == "ai") {
    showAiPanel = !showAiPanel;
    return;
  }

  // Theme command: theme, theme <name>
  if (cleanCmd == "theme" || cleanCmd.rfind("theme ", 0) == 0) {
    string arg = (cleanCmd == "theme") ? "" : cleanCmd.substr(6);
    while (!arg.empty() && arg[0] == ' ') arg.erase(0, 1);
    while (!arg.empty() && arg.back() == ' ') arg.pop_back();

    if (arg.empty()) {
      activeThemeIdx = (activeThemeIdx + 1) % themes.size();
    } else {
      string argLower = arg;
      for (auto &c : argLower) c = tolower(c);
      if (argLower == "minimal" || argLower == "min" || argLower == "mono" || argLower == "minimalist") {
        activeThemeIdx = 6;
      } else {
        bool found = false;
        for (size_t i = 0; i < themes.size(); ++i) {
          string nameLower = themes[i].name;
          for (auto &c : nameLower) c = tolower(c);
          if (nameLower.find(argLower) != string::npos || to_string(i + 1) == argLower) {
            activeThemeIdx = (int)i;
            found = true;
            break;
          }
        }
        if (!found) {
          activeThemeIdx = (activeThemeIdx + 1) % themes.size();
        }
      }
    }
    showToast("Theme: " + themes[activeThemeIdx].name);
    return;
  }

  // Paste command: paste, p
  if (cleanCmd == "paste" || cleanCmd == "p") {
    string sysClip = getSystemClipboard();
    if (!sysClip.empty()) {
      insertTextAtCursor(sysClip);
      showToast("Pasted from system clipboard");
    } else if (!yankBuffer.empty()) {
      insertTextAtCursor(yankBuffer);
      showToast("Pasted from internal buffer");
    } else {
      showToast("Clipboard is empty");
    }
    return;
  }

  // cd command: cd, cd <path>, cd.., cd~, cd/path
  if (cleanCmd == "cd" || cleanCmd.rfind("cd ", 0) == 0 || cleanCmd.rfind("cd/", 0) == 0 ||
      cleanCmd == "cd.." || cleanCmd.rfind("cd..", 0) == 0 || cleanCmd == "cd~" || cleanCmd.rfind("cd~", 0) == 0) {
    string path = "";
    if (cleanCmd == "cd" || cleanCmd == "cd~") {
      path = "~";
    } else if (cleanCmd == "cd..") {
      path = "..";
    } else if (cleanCmd.rfind("cd ", 0) == 0) {
      path = cleanCmd.substr(3);
    } else if (cleanCmd.rfind("cd/", 0) == 0) {
      path = cleanCmd.substr(2);
    } else if (cleanCmd.rfind("cd..", 0) == 0) {
      path = cleanCmd.substr(2);
    } else if (cleanCmd.rfind("cd~", 0) == 0) {
      path = cleanCmd.substr(2);
    }

    while (!path.empty() && path[0] == ' ') path.erase(0, 1);
    while (!path.empty() && path.back() == ' ') path.pop_back();

    if (path.empty() || path == "~") {
      const char *home = getenv("HOME");
      if (home) path = home;
    } else if (!path.empty() && path[0] == '~') {
      const char *home = getenv("HOME");
      if (home) path = string(home) + path.substr(1);
    }

    std::error_code ec;
    std::filesystem::current_path(path, ec);
    if (!ec || chdir(path.c_str()) == 0) {
      char cwdBuf[1024];
      string newCwd = (getcwd(cwdBuf, sizeof(cwdBuf)) != NULL) ? string(cwdBuf) : path;
      ftCursor = 0;
      ftScrollOff = 0;
      loadFileList();
      loadRecentFiles();
      showToast("📁 Changed Dir: " + newCwd);
    } else {
      showToast("❌ Failed to change dir: " + path + " (" + ec.message() + ")");
    }
    return;
  }

  // Move / Rename file command: mv <source> <target>, rename <source> <target>
  if (cleanCmd.rfind("mv ", 0) == 0 || cleanCmd.rfind("rename ", 0) == 0) {
    size_t sp = cleanCmd.find(' ');
    string args = cleanCmd.substr(sp + 1);
    while (!args.empty() && args[0] == ' ') args.erase(0, 1);
    size_t sp2 = args.find(' ');
    if (sp2 != string::npos) {
      string src = args.substr(0, sp2);
      string dst = args.substr(sp2 + 1);
      while (!dst.empty() && dst[0] == ' ') dst.erase(0, 1);
      while (!dst.empty() && dst.back() == ' ') dst.pop_back();

      if (!src.empty() && !dst.empty()) {
        std::error_code ec;
        std::filesystem::rename(src, dst, ec);
        if (!ec) {
          showToast("Renamed: " + src + " -> " + dst);
          if (currentFile == src) {
            currentFile = dst;
          }
          for (size_t i = 0; i < openTabs.size(); ++i) {
            if (openTabs[i] == src) openTabs[i] = dst;
          }
          loadFileList();
        } else {
          showToast("Rename error: " + ec.message());
        }
      }
    } else {
      showToast("Usage: :mv <source> <target>");
    }
    return;
  }

  // Save As command: saveas <new_filename>
  if (cleanCmd.rfind("saveas ", 0) == 0) {
    size_t sp = cleanCmd.find(' ');
    string newName = cleanCmd.substr(sp + 1);
    while (!newName.empty() && newName[0] == ' ') newName.erase(0, 1);
    while (!newName.empty() && newName.back() == ' ') newName.pop_back();
    if (!newName.empty()) {
      savefile(newName);
      currentFile = newName;
      if (find(openTabs.begin(), openTabs.end(), newName) == openTabs.end()) {
        openTabs.push_back(newName);
      }
      for (size_t i = 0; i < openTabs.size(); ++i) {
        if (openTabs[i] == newName) activeTabIdx = (int)i;
      }
      loadFileList();
      showToast("Saved As: " + newName);
    }
    return;
  }

  // Run / Build / Compile command: brun, run, build, compile, r, b, g++ (with or without file argument)
  if (cleanCmd == "brun" || cleanCmd == "run" || cleanCmd == "build" || cleanCmd == "compile" ||
      cleanCmd == "r" || cleanCmd == "b" || cleanCmd == "g++" ||
      cleanCmd.rfind("brun ", 0) == 0 || cleanCmd.rfind("run ", 0) == 0 ||
      cleanCmd.rfind("build ", 0) == 0 || cleanCmd.rfind("compile ", 0) == 0 ||
      cleanCmd.rfind("r ", 0) == 0 || cleanCmd.rfind("b ", 0) == 0 || cleanCmd.rfind("g++ ", 0) == 0) {

    string targetFile = currentFile;
    size_t sp = cleanCmd.find(' ');
    if (sp != string::npos) {
      string argFile = cleanCmd.substr(sp + 1);
      while (!argFile.empty() && argFile[0] == ' ') argFile.erase(0, 1);
      while (!argFile.empty() && argFile.back() == ' ') argFile.pop_back();
      if (!argFile.empty()) {
        targetFile = argFile;
        openfile(targetFile);
        currentView = ScreenView::Editor;
      }
    }

    if (targetFile.empty()) {
      if (!flatTree.empty() && ftCursor >= 0 && ftCursor < (int)flatTree.size() && !flatTree[ftCursor].isDir) {
        targetFile = flatTree[ftCursor].fullPath;
        openfile(targetFile);
        currentView = ScreenView::Editor;
      } else if (!recentFiles.empty()) {
        targetFile = recentFiles[0];
        openfile(targetFile);
        currentView = ScreenView::Editor;
      }
    }

    executeLanguageFile(targetFile);
    return;
  }

  // Direct shell command prefix !<cmd> or fallback to shell command execution
  if (cleanCmd.rfind("!", 0) == 0) {
    runShellCommand(cleanCmd.substr(1));
  } else {
    runShellCommand(cleanCmd);
  }
}

string promptSaveAs() {
  currentMode = EditorMode::SavePrompt;
  savePromptBuffer = currentFile.empty() ? "text.txt" : currentFile;
  return savePromptBuffer;
}

void cleanupScreen() {
  cout << "\033[0m\033[?1000l\033[?1002l\033[?1003l\033[?1006l\033[?1015l\033[?2004l\033[?1049l\033[2J\033[H\033[?25h" << flush;
}

//------------------------------------------------------------
// Comprehensive Mouse Click Hit Testing Matrix
//------------------------------------------------------------

void handleMouseClick(int mx, int my, int btn, char type) {
  try {
    // Right-Click Press (btn == 2, type == 'M'): Open Context Menu
    if (btn == 2 && type == 'M') {
      showContextMenu = true;
      int cmW = 24;
      int cmH = 7;
      contextMenuX = max(1, min(screenCols - cmW + 1, mx));
      contextMenuY = max(1, min(screenRows - cmH + 1, my));
      return;
    }

    // Handle Left Click when Context Menu is active
    if (showContextMenu && type == 'M') {
      int cmW = 24;
      int cmH = 7;
      int pCol = max(1, min(screenCols - cmW + 1, contextMenuX));
      int pRow = max(1, min(screenRows - cmH + 1, contextMenuY));

      if (mx >= pCol && mx < pCol + cmW && my >= pRow && my < pRow + cmH) {
        int r = my - pRow;
        if (r == 1) {
          executeContextMenuCopy();
        } else if (r == 2) {
          executeContextMenuCut();
        } else if (r == 3) {
          executeContextMenuPaste();
        } else if (r == 5) {
          executeContextMenuQuit();
        }
        showContextMenu = false;
        return;
      } else {
        // Clicked outside context menu -> dismiss menu
        showContextMenu = false;
      }
    }

    // Window Controls Hit Test (Row 1, right side)
    if (my == 1) {
      if (mx >= screenCols - 5) {
        // [ x ] Close IDE
        disableRawMode();
        cleanupScreen();
        exit(0);
      } else if (mx >= screenCols - 10 && mx < screenCols - 5) {
        // [ □ ] Maximize / Restore Toggle
        isMaximized = !isMaximized;
        showToast(isMaximized ? "Window Maximized" : "Window Restored");
        return;
      } else if (mx >= screenCols - 15 && mx < screenCols - 10) {
        // [ - ] Minimize / Compact View Toggle
        showFileTree = !showFileTree;
        showTerminalPanel = false;
        showAiPanel = false;
        showToast(showFileTree ? "Normal View" : "Compact Mode");
        return;
      }
    }

    // Home Dashboard Click Dispatcher
    if (currentView == ScreenView::Home) {
      int sidebarW = isMaximized ? 0 : max(26, (int)(screenCols * 0.24));

      // Left Sidebar Clicks on Home
      if (sidebarW > 0 && mx <= sidebarW) {
        int r = my - 2; // relative to content row 0
        int ftIdx = r - 2 + ftScrollOff; // row 0=title, row 1=path, row 2+ = tree
        if (r >= 2 && ftIdx >= 0 && ftIdx < (int)flatTree.size()) {
          ftCursor = ftIdx;
          const FlatEntry &e = flatTree[ftIdx];
          if (e.isDir) {
            ftToggle(ftRoot, e.fullPath);
            ftRebuild();
          } else {
            openfile(e.fullPath);
            currentView = ScreenView::Editor;
          }
        }
        return;
      }

      // Main Area Clicks
      int relY = my - 2;

      if (relY == 9) {
        showToast("Directory path copied to clipboard!");
        return;
      }

      // Grid Row 1 (Quick Actions & Tips Carousel)
      if (relY >= 12 && relY <= 16) {
        int cardIdx = relY - 12;
        int mInnerW = screenCols - sidebarW;
        int relX = mx - sidebarW;
        int colSection = (relX * 3) / max(1, mInnerW);

        if (colSection == 0) { // Quick Actions
          if (cardIdx == 0) {
            currentMode = EditorMode::Palette;
            paletteBuffer.clear();
            filteredPalette = paletteActions;
          } else if (cardIdx == 1) {
            createNewFile();
          } else if (cardIdx == 2) {
            currentMode = EditorMode::Palette;
            paletteBuffer.clear();
            filteredPalette = paletteActions;
          } else if (cardIdx == 3) {
            showTerminalPanel = !showTerminalPanel;
          } else if (cardIdx == 4) {
            showAiPanel = !showAiPanel;
            if (showAiPanel)
              currentMode = EditorMode::AiInput;
          }
          return;
        }
      }
      return;
    }

    // Editor View Click Dispatcher
    if (currentView == ScreenView::Editor) {
      if (my == 2) { // Tabs bar
        if (mx > 5 && mx < 40 && openTabs.size() > 1) {
          activeTabIdx = (activeTabIdx + 1) % openTabs.size();
          openfile(openTabs[activeTabIdx]);
        }
        return;
      }

      int sidebarWidth =
          (showFileTree && !isMaximized) ? max(26, (int)(screenCols * 0.24)) : 0;
      if (showFileTree && !isMaximized && mx <= sidebarWidth) {
        int clickedRow = my - 3 + ftScrollOff;
        if (clickedRow >= 0 && clickedRow < (int)flatTree.size()) {
          ftCursor = clickedRow;
          const FlatEntry &e = flatTree[clickedRow];
          if (e.isDir) {
            ftToggle(ftRoot, e.fullPath);
            ftRebuild();
          } else {
            openfile(e.fullPath);
            currentView = ScreenView::Editor;
          }
        }
        return;
      }

      int targetRow = rowOffset + (my - 3);
      int targetCol = max(0, mx - sidebarWidth - 6);

      if (targetRow >= 0 && targetRow < (int)lines.size()) {
        cy = targetRow;
        cx = min(targetCol, (int)lines[cy].size());
        updateBraceMatching();
      }
    }
  } catch (...) {
    // Suppress any out-of-bounds mouse click exceptions
  }
}

//------------------------------------------------------------
// Main Event Loop
//------------------------------------------------------------

void processKey(int key);
int legacy_main() {
  loadFileList();
  loadRecentFiles();
  enableRawMode();

  currentView = ScreenView::Home;

  while (true) {
    refresh_screen();

    int key = readkey();
    processKey(key);
  }
  cleanupScreen();
  return 0;
}

void processKey(int key) {

    if (key == -1 || key == -2)
      return;

    // Esc dismisses context menu if open
    if (showContextMenu && key == '\x1b') {
      showContextMenu = false;
      return;
    }

    // Global Command Mode handling (Home Screen or Editor)
    if (currentMode == EditorMode::Command) {
      if (key == '\x1b') { // Esc exits Command mode
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '\r') { // Enter executes command
        executeCommand(commandBuffer);
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!commandBuffer.empty())
          commandBuffer.pop_back();
        return;
      }
      if (key >= 32 && key <= 126) {
        commandBuffer.push_back((char)key);
      }
      return;
    }

    // Global Ctrl+T toggle embedded terminal
    if (key == 20) {
      showTerminalPanel = !showTerminalPanel;
      return;
    }

    // Home Screen Keyboard Controls
    if (currentView == ScreenView::Home) {
      if (key == 17) { // Ctrl+Q -> Exit app cleanly to terminal
        shouldExitApp = true;
        disableRawMode();
        cleanupScreen();
        exit(0);
      }
      if (key == ':') { // Pressing : in Home screen enters Command Mode
        currentMode = EditorMode::Command;
        commandBuffer.clear();
        return;
      }
      // ESC: go to editor (if file open), or do nothing
      if (key == '\x1b') {
        if (!currentFile.empty())
          currentView = ScreenView::Editor;
        return;
      }
      // Tab: cycle through active panels
      if (key == '\t') {
        if (homeSection == HomeSection::Explorer) homeSection = HomeSection::RecentFiles;
        else if (homeSection == HomeSection::RecentFiles) homeSection = HomeSection::QuickActions;
        else if (homeSection == HomeSection::QuickActions) homeSection = HomeSection::SystemInfo;
        else homeSection = HomeSection::Explorer;
        return;
      }
      // Arrow Right: move to next panel
      if (key == ARROW_RIGHT) {
        if (homeSection == HomeSection::Explorer) homeSection = HomeSection::RecentFiles;
        else if (homeSection == HomeSection::RecentFiles) homeSection = HomeSection::QuickActions;
        else if (homeSection == HomeSection::QuickActions) homeSection = HomeSection::SystemInfo;
        return;
      }
      // Arrow Left: move to previous panel
      if (key == ARROW_LEFT) {
        if (homeSection == HomeSection::SystemInfo) homeSection = HomeSection::QuickActions;
        else if (homeSection == HomeSection::QuickActions) homeSection = HomeSection::RecentFiles;
        else if (homeSection == HomeSection::RecentFiles) homeSection = HomeSection::Explorer;
        return;
      }
      // j / arrow-down: move cursor in focused section
      if (key == ARROW_DOWN || key == 'j') {
        if (homeSection == HomeSection::Explorer) {
          if (ftCursor < (int)flatTree.size() - 1)
            ftCursor++;
        } else if (homeSection == HomeSection::RecentFiles) {
          if (recentFileCursor < (int)recentFiles.size() - 1)
            recentFileCursor++;
        } else if (homeSection == HomeSection::QuickActions) {
          if (quickActionCursor < 9)
            quickActionCursor++;
        }
        return;
      }
      // k / arrow-up: move cursor in focused section
      if (key == ARROW_UP || key == 'k') {
        if (homeSection == HomeSection::Explorer) {
          if (ftCursor > 0)
            ftCursor--;
        } else if (homeSection == HomeSection::RecentFiles) {
          if (recentFileCursor > 0)
            recentFileCursor--;
        } else if (homeSection == HomeSection::QuickActions) {
          if (quickActionCursor > 0)
            quickActionCursor--;
        }
        return;
      }
      // Enter: open selected item or execute action
      if (key == '\r') {
        if (homeSection == HomeSection::Explorer) {
          if (ftCursor < (int)flatTree.size()) {
            const FlatEntry &e = flatTree[ftCursor];
            if (e.isDir) {
              ftToggle(ftRoot, e.fullPath);
              ftRebuild();
            } else {
              openfile(e.fullPath);
              currentView = ScreenView::Editor;
            }
          }
        } else if (homeSection == HomeSection::RecentFiles) {
          if (recentFileCursor < (int)recentFiles.size()) {
            openfile(recentFiles[recentFileCursor]);
            currentView = ScreenView::Editor;
          }
        } else if (homeSection == HomeSection::QuickActions) {
          if (quickActionCursor == 0) {
            executeCommand("run");
          } else if (quickActionCursor == 1) {
            commandBuffer = "g++ ";
            currentMode = EditorMode::Command;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 2) {
            commandBuffer = "./";
            currentMode = EditorMode::Command;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 3) {
            showTerminalPanel = !showTerminalPanel;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 4) {
            currentMode = EditorMode::Palette;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 5) {
            createNewFile();
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 6) {
            currentMode = EditorMode::Palette;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 7) {
            showFileTree = !showFileTree;
          } else if (quickActionCursor == 8) {
            showAiPanel = !showAiPanel;
            currentView = ScreenView::Editor;
          } else if (quickActionCursor == 9) {
            activeThemeIdx = (activeThemeIdx + 1) % themes.size();
          }
        }
        return;
      }
      // Backspace: navigate up one directory in explorer
      if (key == 127 && homeSection == HomeSection::Explorer) {
        string parent = ftRoot.fullPath;
        size_t sl = parent.rfind('/');
        if (sl != string::npos && sl > 0) {
          chdir(parent.substr(0, sl).c_str());
          loadFileList();
        }
        return;
      }
      // Ctrl+B: toggle sidebar
      if (key == 2) {
        showFileTree = !showFileTree;
        return;
      }
      
      // Ctrl+N: create new file
      if (key == 14) {
        createNewFile();
        return;
      }
      
      // Do not fall through to editor key handlers
      if (key != MOUSE_EVENT) {
        return;
      }
    }

    // Mouse Event Processor
    if (key == MOUSE_EVENT) {
      handleMouseClick(lastMouseEvent.x, lastMouseEvent.y, lastMouseEvent.btn,
                       lastMouseEvent.type);
      return;
    }

    // Save Prompt Box Mode (Ctrl+S / Ctrl+Q / Save As)
    if (currentMode == EditorMode::SavePrompt) {
      if (key == '\x1b') { // Esc -> Cancel
        currentMode = EditorMode::Normal;
        savePromptBuffer.clear();
        isQuittingAfterSave = false;
        return;
      }
      if (key == '\r') { // Enter -> Save
        string filename = savePromptBuffer;
        if (filename.empty())
          filename = currentFile.empty() ? "text.txt" : currentFile;

        savefile(filename);
        currentFile = filename;
        savePromptBuffer.clear();
        currentMode = EditorMode::Normal;

        if (isQuittingAfterSave) {
          isQuittingAfterSave = false;
          disableRawMode();
          cleanupScreen();
          exit(0);
        }
        return;
      }
      if (key == 127 || key == DEL_KEY) { // Backspace
        if (!savePromptBuffer.empty())
          savePromptBuffer.pop_back();
        return;
      }
      if (key >= 32 && key <= 126) { // Printables
        savePromptBuffer.push_back((char)key);
        return;
      }
      return;
    }

    // Command Palette Mode
    if (currentMode == EditorMode::Palette) {
      if (key == '\x1b') {
        currentMode = EditorMode::Normal;
        paletteBuffer.clear();
        return;
      }
      if (key == ARROW_UP || key == 'k') {
        if (paletteIdx > 0)
          paletteIdx--;
        return;
      }
      if (key == ARROW_DOWN || key == 'j') {
        if (paletteIdx < (int)filteredPalette.size() - 1)
          paletteIdx++;
        return;
      }
      if (key == '\r') {
        if (!filteredPalette.empty() &&
            paletteIdx < (int)filteredPalette.size()) {
          executeCommand(filteredPalette[paletteIdx].action);
        }
        paletteBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!paletteBuffer.empty())
          paletteBuffer.pop_back();
      } else if (key >= 32 && key <= 126) {
        paletteBuffer.push_back((char)key);
      }

      filteredPalette.clear();
      for (const auto &item : paletteActions) {
        if (paletteBuffer.empty() ||
            item.name.find(paletteBuffer) != string::npos) {
          filteredPalette.push_back(item);
        }
      }
      paletteIdx = 0;
      return;
    }

    // AI Mode
    if (currentMode == EditorMode::AiInput) {
      if (key == '\x1b') {
        aiInputBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '1') {
        aiChatHistory.push_back("User: Explain current function");
        aiChatHistory.push_back("AI: Analyzed function at line " +
                                to_string(cy + 1) +
                                ". Performs buffer rendering.");
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '2') {
        aiChatHistory.push_back("User: Find bug & optimize");
        aiChatHistory.push_back(
            "AI: Memory allocations optimal. No memory leaks detected.");
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '\r') {
        showAiPanel = true;
        aiChatHistory.push_back("User: " + aiInputBuffer);
        aiChatHistory.push_back("AI: Refactored your snippet successfully.");
        aiInputBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!aiInputBuffer.empty())
          aiInputBuffer.pop_back();
        return;
      }
      if (key >= 32 && key <= 126) {
        aiInputBuffer.push_back((char)key);
      }
      return;
    }

    // Command Mode
    if (currentMode == EditorMode::Command) {
      if (key == '\x1b') {
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '\r') {
        executeCommand(commandBuffer);
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!commandBuffer.empty())
          commandBuffer.pop_back();
        return;
      }
      if (key >= 32 && key <= 126) {
        commandBuffer.push_back((char)key);
      }
      return;
    }

    // Search Mode
    if (currentMode == EditorMode::Search) {
      if (key == '\x1b') {
        searchBuffer.clear();
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == '\r') {
        currentMode = EditorMode::Normal;
        return;
      }
      if (key == 'n') {
        if (!searchBuffer.empty()) {
          for (size_t r = cy + 1; r < lines.size(); ++r) {
            size_t pos = lines[r].find(searchBuffer);
            if (pos != string::npos) {
              cy = r;
              cx = pos;
              exit(0);
            }
          }
        }
        return;
      }
      if (key == 'N') {
        if (!searchBuffer.empty() && cy > 0) {
          for (int r = cy - 1; r >= 0; --r) {
            size_t pos = lines[r].find(searchBuffer);
            if (pos != string::npos) {
              cy = r;
              cx = pos;
              exit(0);
            }
          }
        }
        return;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!searchBuffer.empty())
          searchBuffer.pop_back();
      } else if (key >= 32 && key <= 126) {
        searchBuffer.push_back((char)key);
      }

      if (!searchBuffer.empty()) {
        for (size_t r = 0; r < lines.size(); ++r) {
          size_t pos = lines[r].find(searchBuffer);
          if (pos != string::npos) {
            cy = r;
            cx = pos;
            exit(0);
          }
        }
      }
      return;
    }

    // Escape Key -> Return Home
    if (key == '\x1b') {
      currentView = ScreenView::Home;
      return;
    }

    if (key == 16) { // Ctrl+P -> Command Palette
      currentMode = EditorMode::Palette;
      paletteBuffer.clear();
      filteredPalette = paletteActions;
      paletteIdx = 0;
      return;
    }

    if (key == 1) { // Ctrl+A -> Toggle AI Panel
      showAiPanel = !showAiPanel;
      if (showAiPanel) {
        currentMode = EditorMode::AiInput;
        aiInputBuffer.clear();
      }
      return;
    }

    if (key == 2) { // Ctrl+B -> Toggle Sidebar File Tree
      showFileTree = !showFileTree;
      if (showFileTree)
        loadFileList();
      return;
    }

    if (key == 6) { // Ctrl+F -> Toggle Incremental Find
      currentMode = EditorMode::Search;
      searchBuffer.clear();
      return;
    }

    if (key == 20) { // Ctrl+T -> Toggle Embedded Terminal
      showTerminalPanel = !showTerminalPanel;
      return;
    }

    if (key == 14) { // Ctrl+N -> New File
      createNewFile();
      return;
    }

    if (key == 17) { // Ctrl+Q -> Return to Home Screen (or Exit app if on Home)
      if (currentView == ScreenView::Editor) {
        currentView = ScreenView::Home;
        currentMode = EditorMode::Normal;
        showToast("Returned to Home Screen");
      } else {
        shouldExitApp = true;
        disableRawMode();
        cleanupScreen();
        exit(0);
      }
      return;
    }

    if (key == 19) { // Ctrl+S -> Quick Save / Save As
      if (currentFile.empty() || currentFile == "untitled.cpp" || currentFile == "text.txt") {
        promptSaveAs();
      } else {
        savefile(currentFile);
        showToast("File saved: " + currentFile);
      }
      return;
    }

    if (key == ARROW_LEFT) {
      moveLeft();
      return;
    }
    if (key == ARROW_RIGHT) {
      moveRight();
      return;
    }
    if (key == ARROW_UP) {
      if (showCompletion) {
        completionIdx =
            (completionIdx > 0) ? completionIdx - 1 : completionList.size() - 1;
      } else if (showFileTree && ftCursor > 0) {
        ftCursor--;
      } else {
        moveUp();
      }
      return;
    }
    if (key == ARROW_DOWN) {
      if (showCompletion) {
        completionIdx = (completionIdx + 1) % completionList.size();
      } else if (showFileTree && ftCursor < (int)flatTree.size() - 1) {
        ftCursor++;
      } else {
        moveDown();
      }
      return;
    }

    // Normal & Visual Modes
    if (currentMode == EditorMode::Normal ||
        currentMode == EditorMode::Visual) {
      if (key == 'i') {
        currentMode = EditorMode::Insert;
        pendingJ = false;
        return;
      }
      if (key == 'a') {
        if (cx < (int)lines[cy].size())
          cx++;
        currentMode = EditorMode::Insert;
        pendingJ = false;
        return;
      }

      if (key == 'v') {
        if (currentMode == EditorMode::Visual)
          currentMode = EditorMode::Normal;
        else {
          currentMode = EditorMode::Visual;
          selStartX = cx;
          selStartY = cy;
        }
        return;
      }

      if (key == ')' || key == '0') {
        if (currentMode == EditorMode::Visual) {
          selStartX = 0;
          cx = lines[cy].size();
        } else {
          cx = 0;
        }
        return;
      }

      if (key == 'e') {
        moveWordEnd();
        return;
      }
      if (key == ':') {
        currentMode = EditorMode::Command;
        commandBuffer.clear();
        return;
      }
      if (key == '/') {
        currentMode = EditorMode::Search;
        searchBuffer.clear();
        return;
      }

      if (key == 'u') {
        undoManager.undo(lines, cx, cy);
        return;
      }
      if (key == 18) {
        undoManager.redo(lines, cx, cy);
        return;
      }

      if (key == 'h') {
        moveLeft();
        return;
      }
      if (key == 'l') {
        moveRight();
        return;
      }
      if (key == 'k') {
        if (showFileTree && ftCursor > 0)
          ftCursor--;
        else
          moveUp();
        return;
      }
      if (key == 'j') {
        if (showFileTree && ftCursor < (int)flatTree.size() - 1)
          ftCursor++;
        else
          moveDown();
        return;
      }
      if (key == '\r' && showFileTree) {
        if (ftCursor < (int)flatTree.size()) {
          const FlatEntry &e = flatTree[ftCursor];
          if (e.isDir) {
            ftToggle(ftRoot, e.fullPath);
            ftRebuild();
          } else {
            openfile(e.fullPath);
            currentView = ScreenView::Editor;
          }
        }
        return;
      }

      if (key == 'w') {
        moveWordForward();
        return;
      }
      if (key == 'b') {
        moveWordBackward();
        return;
      }
      if (key == '$') {
        cx = lines[cy].size();
        return;
      }
      if (key == 'g') {
        if (lastKeyChar == 'g') {
          cy = 0;
          cx = 0;
          lastKeyChar = 0;
        } else {
          lastKeyChar = 'g';
        }
        return;
      }
      if (key == 'G') {
        cy = lines.size() - 1;
        cx = 0;
        return;
      }

      if (key == 'x') {
        if (currentMode == EditorMode::Visual)
          deleteSelection();
        else if (cx < (int)lines[cy].size()) {
          undoManager.saveSnapshot(lines, cx, cy);
          lines[cy].erase(cx, 1);
          isModified = true;
        }
        return;
      }

      if (key == 'd') {
        if (currentMode == EditorMode::Visual)
          deleteSelection();
        else if (lastKeyChar == 'd') {
          undoManager.saveSnapshot(lines, cx, cy);
          yankBuffer = lines[cy];
          lines.erase(lines.begin() + cy);
          if (lines.empty())
            lines.push_back("");
          if (cy >= (int)lines.size())
            cy = lines.size() - 1;
          isModified = true;
          lastKeyChar = 0;
        } else {
          lastKeyChar = 'd';
        }
        return;
      }

      if (key == 'y') {
        if (currentMode == EditorMode::Visual)
          yankSelection();
        else if (lastKeyChar == 'y') {
          yankBuffer = lines[cy];
          lastKeyChar = 0;
        } else {
          lastKeyChar = 'y';
        }
        return;
      }

      if (key == 'p') {
        if (!yankBuffer.empty()) {
          undoManager.saveSnapshot(lines, cx, cy);
          lines.insert(lines.begin() + cy + 1, yankBuffer);
          cy++;
          isModified = true;
        }
        return;
      }

      // Ctrl+C (3): Copy
      if (key == 3) {
        if (currentMode == EditorMode::Visual) {
          yankSelection();
          showToast("Copied selection");
        } else {
          yankBuffer = lines[cy];
          showToast("Copied line");
        }
        return;
      }

      // Ctrl+V (22): Paste
      if (key == 22) {
        string sysClip = getSystemClipboard();
        if (!sysClip.empty()) {
          insertTextAtCursor(sysClip);
          showToast("Pasted from system clipboard");
        } else if (!yankBuffer.empty()) {
          insertTextAtCursor(yankBuffer);
          showToast("Pasted from internal buffer");
        } else {
          showToast("Clipboard is empty");
        }
        return;
      }

      lastKeyChar = key;
      return;
    }

    // Insert Mode
    if (currentMode == EditorMode::Insert) {
      if (key == '\x1b') {
        currentMode = EditorMode::Normal;
        pendingJ = false;
        showCompletion = false;
        return;
      }

      // Ctrl+V (22): Paste in Insert mode
      if (key == 22) {
        string sysClip = getSystemClipboard();
        if (!sysClip.empty()) {
          insertTextAtCursor(sysClip);
          showToast("Pasted from system clipboard");
        } else if (!yankBuffer.empty()) {
          insertTextAtCursor(yankBuffer);
          showToast("Pasted from internal buffer");
        }
        return;
      }

      if (key == '\t' || key == '\r') {
        if (showCompletion && !completionList.empty()) {
          undoManager.saveSnapshot(lines, cx, cy);
          string selectedComp = completionList[completionIdx];

          size_t wordStart = cx;
          while (wordStart > 0 && (isalnum(lines[cy][wordStart - 1]) ||
                                   lines[cy][wordStart - 1] == '_' ||
                                   lines[cy][wordStart - 1] == ':' ||
                                   lines[cy][wordStart - 1] == '#' ||
                                   lines[cy][wordStart - 1] == '<' ||
                                   lines[cy][wordStart - 1] == '>' ||
                                   lines[cy][wordStart - 1] == '.' ||
                                   lines[cy][wordStart - 1] == '/' ||
                                   lines[cy][wordStart - 1] == '"' ||
                                   lines[cy][wordStart - 1] == '-')) {
            wordStart--;
          }

          lines[cy].replace(wordStart, cx - wordStart, selectedComp);
          cx = wordStart + selectedComp.size();

          showCompletion = false;
          isModified = true;
          return;
        }
      }

      if (pendingJ) {
        if (key == 'k') {
          pendingJ = false;
          currentMode = EditorMode::Normal;
          showCompletion = false;
          return;
        } else {
          undoManager.saveSnapshot(lines, cx, cy);
          lines[cy].insert(cx, 1, 'j');
          cx++;
          pendingJ = false;
          isModified = true;
        }
      }

      if (key == 'j') {
        pendingJ = true;
        return;
      }

      if (key == 127 || key == DEL_KEY) {
        undoManager.saveSnapshot(lines, cx, cy);
        if (cx > 0) {
          lines[cy].erase(cx - 1, 1);
          cx--;
        } else if (cy > 0) {
          int prevLen = lines[cy - 1].size();
          lines[cy - 1] += lines[cy];
          lines.erase(lines.begin() + cy);
          cy--;
          cx = prevLen;
        }
        isModified = true;
        showCompletion = false;
        return;
      }

      if (key == '\r') {
        undoManager.saveSnapshot(lines, cx, cy);
        string current = lines[cy];
        string left = current.substr(0, cx);
        string right = current.substr(cx);

        lines[cy] = left;
        lines.insert(lines.begin() + cy + 1, right);

        cy++;
        cx = 0;
        isModified = true;
        showCompletion = false;
        return;
      }

      if (key >= 32 && key <= 126) {
        undoManager.saveSnapshot(lines, cx, cy);
        lines[cy].insert(cx, 1, (char)key);
        cx++;
        isModified = true;

        size_t wordStart = cx;
        while (wordStart > 0 && (isalnum(lines[cy][wordStart - 1]) ||
                                 lines[cy][wordStart - 1] == '_' ||
                                 lines[cy][wordStart - 1] == ':' ||
                                 lines[cy][wordStart - 1] == '#' ||
                                 lines[cy][wordStart - 1] == '<' ||
                                 lines[cy][wordStart - 1] == '>' ||
                                 lines[cy][wordStart - 1] == '.' ||
                                 lines[cy][wordStart - 1] == '/' ||
                                 lines[cy][wordStart - 1] == '"' ||
                                 lines[cy][wordStart - 1] == '-')) {
          wordStart--;
        }
        string currentToken = lines[cy].substr(wordStart, cx - wordStart);

        if (currentToken.size() >= 2) {
          completionList.clear();
          for (const auto &sym : lspSymbols) {
            if (sym.find(currentToken) != string::npos) {
              completionList.push_back(sym);
            }
          }
          showCompletion = !completionList.empty();
          completionIdx = 0;
        } else {
          showCompletion = false;
        }
      }
    }
  }


