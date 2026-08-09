#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

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
  Palette
};
enum class HomeSection {
  Explorer,
  RecentFiles
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

    if (maximized) {
      l.sidebar = {0, 0, 0, 0};
      l.main = {1, 3, screenCols,
                screenRows - 4 - (showTerminal ? termHeight : 0)};
    } else {
      int sidebarWidth = showSidebar ? max(26, (int)(screenCols * 0.24)) : 0;
      l.sidebar = {1, 3, sidebarWidth,
                   screenRows - 4 - (showTerminal ? termHeight : 0)};

      int mainX = 1 + sidebarWidth;
      int mainWidth = screenCols - sidebarWidth;
      int mainHeight = screenRows - 4 - (showTerminal ? termHeight : 0);
      l.main = {mainX, 3, mainWidth, mainHeight};
    }

    if (showTerminal && !maximized) {
      l.terminal = {1, 3 + l.main.height, screenCols, termHeight};
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
  string primary;   // Neon Green Accent
  string secondary; // Cyan Sub Accent
  string text;      // Body Text
  string inactive;  // Muted Gray
  string border;    // Border Box
  string warning;   // Orange
  string error;     // Red
};

static const vector<Theme> themes = {
    {"Cyberpunk Neon", "\033[38;2;0;255;136m", "\033[38;2;0;191;255m",
     "\033[38;2;255;255;255m", "\033[38;2;120;120;120m", "\033[38;2;0;200;120m",
     "\033[38;2;255;165;0m", "\033[38;2;255;85;85m"},
    {"Matrix Green", "\033[38;2;0;255;0m", "\033[38;2;0;180;0m",
     "\033[38;2;220;255;220m", "\033[38;2;80;120;80m", "\033[38;2;40;180;40m",
     "\033[38;2;255;200;0m", "\033[38;2;255;50;50m"},
    {"Dracula", "\033[38;2;189;147;249m", "\033[38;2;255;121;198m",
     "\033[38;2;248;248;242m", "\033[38;2;98;114;164m",
     "\033[38;2;139;117;219m", "\033[38;2;241;250;140m",
     "\033[38;2;255;85;85m"},
    {"Nord", "\033[38;2;136;192;208m", "\033[38;2;129;161;193m",
     "\033[38;2;236;239;244m", "\033[38;2;76;86;106m", "\033[38;2;96;152;168m",
     "\033[38;2;235;203;139m", "\033[38;2;191;97;106m"},
    {"Catppuccin", "\033[38;2;203;166;247m", "\033[38;2;137;180;250m",
     "\033[38;2;205;214;244m", "\033[38;2;108;112;134m",
     "\033[38;2;163;126;207m", "\033[38;2;249;226;175m",
     "\033[38;2;243;139;168m"},
    {"Gruvbox", "\033[38;2;250;189;47m", "\033[38;2;254;128;25m",
     "\033[38;2;235;219;178m", "\033[38;2;146;131;116m",
     "\033[38;2;210;149;27m", "\033[38;2;215;153;33m", "\033[38;2;204;36;29m"}};

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

// Sidebar / Section collapse state
bool collapseExplorer = false;
bool collapseRecent = false;

vector<string> lines(1);
int cx = 0;
int cy = 0;
int rowOffset = 0;

int screenRows = 24;
int screenCols = 80;

string commandBuffer;
string searchBuffer;
string paletteBuffer;
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

// Notification Toast
string toastMsg = "";
time_t toastTime = 0;

void showToast(const string &msg) {
  toastMsg = msg;
  toastTime = time(NULL);
}

// Visual Mode Selection & Mouse Drag State
int selStartX = 0;
int selStartY = 0;
bool isMouseDragging = false;
string yankBuffer = "";

// Matching Brace Highlight Coordinates
int matchRow = -1;
int matchCol = -1;

// Sidebar File Tree State
bool showFileTree = true;

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

// Helper: Trim ANSI escape sequences to compute visual width
int visualWidth(const string &str) {
  int width = 0;
  bool inEscape = false;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '\033') {
      inEscape = true;
    } else if (inEscape) {
      if (isalpha(str[i]))
        inEscape = false;
    } else {
      width++;
    }
  }
  return width;
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
  const char disableMouse[] = "\033[?1000l\033[?1006l";
  ssize_t res = write(STDOUT_FILENO, disableMouse, sizeof(disableMouse) - 1);
  (void)res;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cflag |= CS8;

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

  const char enableMouse[] = "\033[?1000h\033[?1006h";
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

void runShellCommand(const string &cmd) {
  showTerminalPanel = true;
  terminalOutput.clear();
  terminalOutput.push_back(" 💻 [ARCHIOID Shell]: " + cmd);

  FILE *fp = popen((cmd + " 2>&1").c_str(), "r");
  if (!fp) {
    terminalOutput.push_back(" ❌ Command execution failed.");
    return;
  }

  char buf[256];
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    string line(buf);
    if (!line.empty() && line.back() == '\n')
      line.pop_back();
    terminalOutput.push_back(" " + line);
  }
  pclose(fp);

  if (terminalOutput.size() == 1) {
    terminalOutput.push_back(" ✅ Process finished successfully.");
  }
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

  string brand = "\u25b2 ARCHIOID  v0.5.0";
  string mid = "Modern Terminal IDE";
  string right = "Theme: " + themes[activeThemeIdx].name;
  // Window controls — [ − ] [ □ ] [ × ]
  string wins = " \u2500 [\u2212] [\u25a1] [\u00d7] \u2500\u256e";

  // Calculate padding so all three sections are evenly spaced
  int brandL = (int)brand.size() + 4; // "╭─ " + brand
  int rightL = (int)right.size() + (int)wins.size() + 2;
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

  frame += "\033[2J\033[H\033[?25l";
  renderHeaderBar(frame, screenCols);

  // Layout: sidebar + main area; reserve 3 rows (header, cmdbar, statusbar)
  int SB =
      (!showFileTree || isMaximized) ? 0 : max(26, (int)(screenCols * 0.22));
  int MW = screenCols - SB;
  int rows = screenRows - 3;

  char cwdBuf[512];
  string cwd = (getcwd(cwdBuf, sizeof(cwdBuf)) != NULL) ? string(cwdBuf) : ".";

  // Cached sysinfo
  static SystemInfo si;
  static time_t siTime = 0;
  if (time(NULL) - siTime > 5) {
    si = getSystemInfo();
    siTime = time(NULL);
  }

  // ── BANNER ───────────────────────────────────────────────
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
  string subtitle = "MODERN TERMINAL IDE FOR DEVELOPERS WHO BUILD THE FUTURE";

  // ── SIDEBAR: Project Explorer ─────────────────────────────
  int sbInner = SB > 2 ? SB - 2 : 0;
  vector<string> sb;
  if (SB > 0) {
    sb.push_back("\033[1m" + pc + "PROJECT EXPLORER" + rs + ic + " [-]" + rs);
    // Show truncated cwd path
    string dispPath = cwd;
    if ((int)dispPath.size() > sbInner - 1)
      dispPath = "~" + dispPath.substr(dispPath.size() - (sbInner - 2));
    sb.push_back(sc + dispPath + rs);

    // File tree entries
    int maxSbRows = rows - 4;
    int shown = 0;
    for (int i = ftScrollOff; i < (int)flatTree.size() && shown < maxSbRows;
         ++i, ++shown) {
      const FlatEntry &e = flatTree[i];
      string indent(e.depth * 2, ' ');
      string icon = e.isDir ? (e.expanded ? "\u25bc " : "\u25b6 ") : "\u25a1 ";
      string label = indent + icon + e.name;
      int maxLbl = sbInner - 2;
      if ((int)label.size() > maxLbl)
        label = label.substr(0, maxLbl - 1) + "+";
      bool sel = (i == ftCursor && homeSection == HomeSection::Explorer);
      bool isCur = !currentFile.empty() && e.fullPath == currentFile;
      if (sel)
        sb.push_back("\033[48;2;0;60;40m" + pc + " " + label + rs);
      else if (isCur && !e.isDir)
        sb.push_back("\033[48;2;0;40;25m" + pc + " " + label + rs);
      else if (e.isDir)
        sb.push_back(sc + " " + label + rs);
      else
        sb.push_back(ic + " " + label + rs);
    }

    while ((int)sb.size() < rows - 2)
      sb.push_back("");
    sb.push_back(ic + "\u2191/k \u2193/j  Enter: Open  Backspace: Up" + rs);
    while ((int)sb.size() < rows)
      sb.push_back("");
  }

  // ── MAIN AREA ─────────────────────────────────────────────
  vector<string> ml;
  auto blank = [&]() { ml.push_back(string(MW, ' ')); };
  auto center = [&](const string &txt, const string &col) {
    int tw = visualWidth(txt), lp = max(0, (MW - tw) / 2);
    ml.push_back(string(lp, ' ') + col + txt + rs +
                 string(max(0, MW - lp - tw), ' '));
  };
  auto addL = [&](const string &s, int vw) {
    ml.push_back(s + string(max(0, MW - vw), ' '));
  };

  blank();
  for (auto &bl : banner)
    center(bl, pc);
  blank();
  center(subtitle, sc);
  blank();

  // Current Directory box
  {
    int bInner = min(MW - 4, max(46, (int)cwd.size() + 10));
    int lp = max(0, (MW - bInner - 2) / 2);
    string hdr = "CURRENT DIRECTORY";
    string top = bc + "\u256d\u2500 " + rs + "\033[1m" + hdr + "\033[0m" + bc +
                 " " +
                 repeatStr("\u2500", max(0, bInner - (int)hdr.size() - 3)) +
                 "\u256e" + rs;
    string bot =
        bc + "\u2570" + repeatStr("\u2500", bInner + 2) + "\u256f" + rs;
    string dirTxt = " \U0001f4c2 " + cwd;
    string btn = " [\u2398]";
    int fill = max(1, bInner - (int)cwd.size() - 5 - (int)btn.size());
    string mid = bc + "\u2502" + rs + pc + dirTxt + string(fill, ' ') + rs +
                 sc + btn + rs + bc + "\u2502" + rs;
    addL(string(lp, ' ') + top, lp + bInner + 2);
    addL(string(lp, ' ') + mid, MW);
    addL(string(lp, ' ') + bot, lp + bInner + 2);
  }
  blank();

  // ── Three-column grid: RECENT FILES | QUICK ACTIONS | SYSTEM INFO
  {
    int sp = 1;
    int cw = max(16, (MW - sp * 4) / 3 - 2);

    int memPct =
        si.totalMemKB > 0 ? (int)(100LL * si.usedMemKB / si.totalMemKB) : 0;
    int diskPct = si.totalDiskBytes > 0
                      ? (int)(100ULL * si.usedDiskBytes / si.totalDiskBytes)
                      : 0;

    // --- Recent Files ---
    vector<string> recRows;
    if (recentFiles.empty()) {
      recRows.push_back(ic + "  (no recent files)" + rs);
    } else {
      for (int i = 0; i < (int)recentFiles.size() && i < 8; ++i) {
        const string &f = recentFiles[i];
        string nm = f;
        size_t sl = f.rfind('/');
        if (sl != string::npos)
          nm = f.substr(sl + 1);
        string dirPart = "";
        if (sl != string::npos && sl > 0) {
          size_t sl2 = f.rfind('/', sl - 1);
          dirPart = f.substr(sl2 != string::npos ? sl2 + 1 : 0,
                             sl - (sl2 != string::npos ? sl2 + 1 : 0)) +
                    "/";
        }
        if ((int)nm.size() > cw - 8)
          nm = nm.substr(0, cw - 9) + "+";
        bool sel =
            (i == recentFileCursor && homeSection == HomeSection::RecentFiles);
        string row;
        if (sel)
          row = "\033[48;2;0;60;40m" + pc + " \u25b6 " + nm + rs + ic +
                string(max(1, cw - (int)nm.size() - (int)dirPart.size() - 4),
                       ' ') +
                dirPart + rs;
        else
          row = ic + "   " + nm + rs + ic +
                string(max(1, cw - (int)nm.size() - (int)dirPart.size() - 3),
                       ' ') +
                dirPart + rs;
        recRows.push_back(row);
      }
    }
    recRows.push_back(ic + "(j/k navigate  Enter: open)" + rs);

    // --- Quick Actions ---
    vector<string> actRows;
    auto act = [&](const string &icon, const string &lbl,
                   const string &key) -> string {
      int fill = max(1, cw - (int)lbl.size() - (int)key.size() - 3);
      return pc + icon + " " + tc + lbl + string(fill, ' ') + ic + key + rs;
    };
    actRows.push_back(act("\u26a1", "Command Palette", "Ctrl+P"));
    actRows.push_back(act("\u25a1", "New File", "Ctrl+N"));
    actRows.push_back(act("\u25a3", "Open File", "Ctrl+O"));
    actRows.push_back(act("\u25a3", "Open Folder", "Ctrl+Shift+O"));
    actRows.push_back(act("\u25b6", "Embedded Terminal", "Ctrl+T"));
    actRows.push_back(act("\u25cb", "AI Assistant", "Ctrl+A"));
    actRows.push_back(act("\u25c6", "Switch Theme", ":theme"));

    // --- System Info ---
    vector<string> infRows;
    auto inf = [&](const string &icon, const string &lbl,
                   const string &val) -> string {
      int fill = max(1, cw - (int)lbl.size() - (int)val.size() - 3);
      return sc + icon + " " + ic + lbl + string(fill, ' ') + tc + val + rs;
    };
    string osS = si.osName.empty() ? "Linux" : si.osName;
    if ((int)osS.size() > cw - 8)
      osS = osS.substr(0, cw - 9) + "+";
    string cpuS = si.cpuModel.empty() ? "\u2014" : si.cpuModel;
    if ((int)cpuS.size() > cw - 8)
      cpuS = cpuS.substr(0, cw - 9) + "+";
    char memBuf[40];
    snprintf(memBuf, sizeof(memBuf), "%.1f / %.1f GiB", si.usedMemMB / 1024.0f,
             si.totalMemMB / 1024.0f);
    char dskBuf[40];
    snprintf(dskBuf, sizeof(dskBuf), "%ld / %ld GiB", si.usedDiskGB,
             si.totalDiskGB);

    infRows.push_back(inf("\u25a1", "OS      :", osS));
    infRows.push_back(inf("\u25a1", "Shell   :", si.shell));
    infRows.push_back(inf(
        "\u25a1",
        "Terminal:", si.terminalName.empty() ? "terminal" : si.terminalName));
    infRows.push_back(inf("\u25a1", "Archioid:", "v0.5.0"));
    infRows.push_back(
        inf("\u25a1", "Uptime  :", si.uptime.empty() ? "\u2014" : si.uptime));
    infRows.push_back(inf("\u25a1", "CPU     :", cpuS));
    // Memory with bar
    {
      string mv = string(memBuf) + "  " + to_string(memPct) + "%";
      int fill =
          max(1, cw - (int)string("Memory  :").size() - (int)mv.size() - 3);
      infRows.push_back(sc + "\u25a1 " + ic + "Memory  :" + string(fill, ' ') +
                        tc + mv + rs);
      infRows.push_back(" " + progressBar(memPct, cw - 2));
    }
    // Disk with bar
    {
      string dv = string(dskBuf) + "  " + to_string(diskPct) + "%";
      int fill =
          max(1, cw - (int)string("Disk    :").size() - (int)dv.size() - 3);
      infRows.push_back(sc + "\u25a1 " + ic + "Disk    :" + string(fill, ' ') +
                        tc + dv + rs);
      infRows.push_back(" " + progressBar(diskPct, cw - 2));
    }

    int nR =
        max({(int)recRows.size(), (int)actRows.size(), (int)infRows.size()});
    while ((int)recRows.size() < nR)
      recRows.push_back("");
    while ((int)actRows.size() < nR)
      actRows.push_back("");
    while ((int)infRows.size() < nR)
      infRows.push_back("");

    string recHdr = "RECENT FILES" + (homeSection == HomeSection::RecentFiles
                                          ? string(" \u25ba")
                                          : string(" [-]"));
    string sp2(sp, ' ');
    addL(sp2 + cardTop(recHdr, cw, bc, rs) + sp2 +
             cardTop("QUICK ACTIONS", cw, sc, rs) + sp2 +
             cardTop("SYSTEM INFO", cw, pc, rs),
         sp + (cw + 2) * 3 + sp * 2);
    for (int i = 0; i < nR; ++i) {
      addL(sp2 + cardRow(padTo(recRows[i], cw), cw, bc, rs) + sp2 +
               cardRow(padTo(actRows[i], cw), cw, sc, rs) + sp2 +
               cardRow(padTo(infRows[i], cw), cw, pc, rs),
           MW);
    }
    addL(sp2 + cardBot(cw, bc, rs) + sp2 + cardBot(cw, sc, rs) + sp2 +
             cardBot(cw, pc, rs),
         sp + (cw + 2) * 3 + sp * 2);
  }
  blank();
  center(ic + "[ESC] Editor  [Tab] Toggle Focus  [Ctrl+B] Toggle Sidebar" + rs,
         "");
  while ((int)ml.size() < rows)
    blank();

  // ── Stitch sidebar + main ─────────────────────────────────
  for (int r = 0; r < rows; ++r) {
    if (SB > 0) {
      string sbc = (r < (int)sb.size()) ? sb[r] : "";
      int vw = visualWidth(sbc), pad = max(0, sbInner - vw);
      frame += bc + "\u2502" + rs + sbc + string(pad, ' ') + bc + "\u2502" + rs;
    }
    frame += ((r < (int)ml.size()) ? ml[r] : string(MW, ' ')) + "\n";
  }

  // ── COMMAND BAR ───────────────────────────────────────────
  {
    string prompt;
    if (currentMode == EditorMode::Command)
      prompt = pc + ": " + rs + tc + commandBuffer + rs;
    else
      prompt = bc + ": " + rs + ic +
               "Type a command (:q to quit, :help for help)" + rs;
    string badge = "\033[48;2;0;100;0m\033[38;2;0;255;136m NORMAL \033[0m";
    int pvw =
        2 + (int)string("Type a command (:q to quit, :help for help)").size();
    frame += prompt + string(max(2, screenCols - pvw - 9), ' ') + badge + "\n";
  }

  // ── STATUS BAR ────────────────────────────────────────────
  {
    string mode = "\033[48;2;0;255;136m\033[38;2;0;0;0m NORMAL \033[0m";
    string bar = "\033[48;2;15;15;20m" + ic + " UTF-8 \u2502 C++" +
                 " \u2502 Ln 1 Col 1" + " \u2502 LSP \u2713" +
                 " \u2502 AI \u2713" + " \u2502 " + getCurrentTimeString() +
                 " \u2502 " + getCurrentDateString() + " " + rs;
    string pulse = pc + " ~\u028c~ " + rs;
    frame += mode + bar + pulse;
    if (!toastMsg.empty() && time(NULL) - toastTime < 3)
      frame += "  \033[48;2;0;122;204m\033[38;2;255;255;255m " + toastMsg +
               " \033[0m";
    frame += "\033[K" + rs;
  }

  ssize_t res = write(STDOUT_FILENO, frame.c_str(), frame.size());
  (void)res;
}

//------------------------------------------------------------
// TUI Screen Refresh Engine with Layout Manager
//------------------------------------------------------------

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
    return;
  }

  scroll();

  int textRows = layout.main.height;
  int lineNumWidth = to_string(lines.size()).size();
  if (lineNumWidth < 2)
    lineNumWidth = 2;

  frame += "\033[?25l\033[H";

  // --- Window Control & Header Bar ---
  renderHeaderBar(frame, screenCols);

  // --- Tab Bar ---
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

    // Sidebar File Explorer Tree
    if (showFileTree && !isMaximized) {
      frame += "\033[48;2;18;18;18m\033[38;2;180;180;180m";
      if (screenRow < (int)flatTree.size()) {
        string fitem = (flatTree[screenRow].isDir
                            ? (flatTree[screenRow].expanded ? " ▼ " : " ▶ ")
                            : "   ") +
                       string(flatTree[screenRow].depth * 2, ' ') +
                       flatTree[screenRow].name;
        if (screenRow == ftCursor) {
          frame += "\033[48;2;45;45;48m" + theme.primary;
        }
        if ((int)fitem.size() > layout.sidebar.width - 2) {
          fitem = fitem.substr(0, layout.sidebar.width - 2);
        }
        frame += fitem;
        while ((int)fitem.size() < layout.sidebar.width - 1)
          fitem += " ";
      } else {
        frame += string(layout.sidebar.width - 1, ' ');
      }
      frame += theme.secondary + "│\033[0m";
    }

    // Editor Content Buffer
    if (fileRow < (int)lines.size()) {
      string num = to_string(fileRow + 1);
      if ((int)num.size() < lineNumWidth) {
        num = string(lineNumWidth - num.size(), ' ') + num;
      }
      frame += "\033[38;2;110;110;110m" + num + " │ \033[0m";
      frame += highlightLine(lines[fileRow], fileRow);
    } else {
      frame += "\033[38;2;90;90;90m~\033[0m";
    }

    frame += "\033[K";

    // AI Panel
    if (showAiPanel && !isMaximized) {
      frame += "\033[38;2;175;0;215m│\033[48;2;25;25;25m\033[38;2;220;220;220m";
      string aiLine = "";
      if (screenRow == 0)
        aiLine = " ╭─ Archioid AI ─╮";
      else if (screenRow == 1)
        aiLine = " ├1. Explain Code ";
      else if (screenRow == 2)
        aiLine = " ├2. Find Bug     ";
      else if (screenRow == 3)
        aiLine = " ├3. Optimize     ";
      else if (screenRow - 4 < (int)aiChatHistory.size())
        aiLine = " " + aiChatHistory[screenRow - 4];

      if ((int)aiLine.size() > aiPanelWidth - 1)
        aiLine = aiLine.substr(0, aiPanelWidth - 1);
      frame += aiLine;
      while ((int)aiLine.size() < aiPanelWidth - 1)
        aiLine += " ";
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

  // Embedded Terminal Panel
  if (showTerminalPanel && !isMaximized) {
    frame += "\n\033[48;2;15;15;15m" + theme.primary +
             "├─ 💻 Embedded Terminal (Ctrl+T) ─────────────────────\033[0m\n";
    for (int t = 0; t < terminalHeight - 1; ++t) {
      frame += "\033[48;2;10;10;10m\033[38;2;200;200;200m";
      string tline = "";
      if (t < (int)terminalOutput.size())
        tline = terminalOutput[t];
      if ((int)tline.size() > screenCols)
        tline = tline.substr(0, screenCols);
      frame += tline;
      while ((int)tline.size() < screenCols)
        tline += " ";
      frame += "\033[0m";
      if (t < terminalHeight - 2)
        frame += "\n";
    }
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
  }

  string status = modeBadge + " \033[48;2;30;30;30m\033[38;2;220;220;220m Ln " +
                  to_string(cy + 1) + " Col " + to_string(cx + 1) +
                  " │ UTF-8 │ C++ │ " + currentFile +
                  (isModified ? " [*]" : "") +
                  " │ clangd ✓ │ AI ✓ │ Git main │ " + getCurrentTimeString();

  frame += status + "\033[K\033[0m\n";

  // --- Separated Command Line Area ---
  frame +=
      theme.border +
      "────────────────────────────────────────────────────────────\033[0m\n";
  string cmdArea = "";
  if (currentMode == EditorMode::Command) {
    cmdArea = theme.primary + ":\033[38;2;255;255;255m" + commandBuffer;
  } else if (currentMode == EditorMode::Search) {
    cmdArea = "\033[38;2;240;180;0mFind: \033[38;2;255;255;255m" + searchBuffer;
  } else if (currentMode == EditorMode::AiInput) {
    cmdArea = "\033[38;2;175;0;215mAI > \033[38;2;255;255;255m" + aiInputBuffer;
  } else {
    cmdArea = "\033[38;2;120;120;120mPress : for commands | Ctrl+P Palette | "
              "Ctrl+F Find | Ctrl+A AI | Esc Home\033[0m";
  }

  frame += cmdArea + "\033[K\033[0m";

  int cursorRow = cy - rowOffset + 4;
  int treeOffset = (showFileTree && !isMaximized) ? layout.sidebar.width : 0;
  int cursorCol = cx + lineNumWidth + 4 + treeOffset;

  if (cursorRow >= 4 && cursorRow <= textRows + 3) {
    frame += "\033[" + to_string(cursorRow) + ";" + to_string(cursorCol) + "H";
  }

  frame += "\033[?25h";
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

void saveRecentFiles() {
  const char *home = getenv("HOME");
  if (!home)
    return;
  ofstream f(string(home) + "/.archioid_recent");
  for (auto &r : recentFiles)
    f << r << "\n";
}

void loadRecentFiles() {
  const char *home = getenv("HOME");
  if (!home)
    return;
  ifstream f(string(home) + "/.archioid_recent");
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
  if (cmd.rfind("open ", 0) == 0 || cmd.rfind("e ", 0) == 0) {
    string filename = cmd.substr(cmd.find(' ') + 1);
    if (!filename.empty()) {
      openfile(filename);
      currentView = ScreenView::Editor;
    }
  } else if (cmd == "w") {
    savefile(currentFile);
  } else if (cmd == "wq") {
    savefile(currentFile);
    disableRawMode();
    cleanupScreen();
    exit(0);
  } else if (cmd == "q" || cmd == "q!") {
    disableRawMode();
    cleanupScreen();
    exit(0);
  } else if (cmd == "tree") {
    showFileTree = !showFileTree;
    if (showFileTree)
      loadFileList();
  } else if (cmd == "term" || cmd == "terminal") {
    showTerminalPanel = !showTerminalPanel;
  } else if (cmd == "ai") {
    showAiPanel = !showAiPanel;
  } else if (cmd == "theme") {
    activeThemeIdx = (activeThemeIdx + 1) % themes.size();
    showToast("Theme: " + themes[activeThemeIdx].name);
  } else if (cmd.rfind("g++", 0) == 0 || cmd.rfind("gcc", 0) == 0 ||
             cmd.rfind("make", 0) == 0) {
    runShellCommand(cmd);
  }
}

string promptSaveAs() {
  disableRawMode();
  cout << "\033[?25h\033[0m\nSave as [" << currentFile << "]: " << flush;
  string filename;
  getline(cin, filename);
  if (filename.empty())
    filename = currentFile;
  enableRawMode();
  return filename;
}

void cleanupScreen() { cout << "\033[0m\033[2J\033[H\033[?25h" << flush; }

//------------------------------------------------------------
// Comprehensive Mouse Click Hit Testing Matrix
//------------------------------------------------------------

void handleMouseClick(int mx, int my, int btn, char type) {
  (void)btn;
  (void)type;

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
      // Click on a file tree row
      int ftIdx = r - 2 + ftScrollOff; // row 0=title, row 1=path, row 2+ = tree
      if (r >= 2 && ftIdx < (int)flatTree.size()) {
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

    // Current Directory Copy Button Click
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
          openfile("untitled.cpp");
          currentView = ScreenView::Editor;
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
      int clickedRow = my - 3;
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
}

//------------------------------------------------------------
// Main Event Loop
//------------------------------------------------------------

int main() {
  loadFileList();
  loadRecentFiles();
  enableRawMode();

  currentView = ScreenView::Home;

  while (true) {
    refresh_screen();

    int key = readkey();
    if (key == -1)
      continue;

    // Home Screen Keyboard Controls
    if (currentView == ScreenView::Home) {
      // ESC: go to editor (if file open), or do nothing
      if (key == '\x1b') {
        if (!currentFile.empty())
          currentView = ScreenView::Editor;
        continue;
      }
      // Tab: switch focus between Explorer and Recent Files
      if (key == '\t') {
        homeSection = (homeSection == HomeSection::Explorer)
                          ? HomeSection::RecentFiles
                          : HomeSection::Explorer;
        continue;
      }
      // j / arrow-down: move cursor in focused section
      if (key == ARROW_DOWN || key == 'j') {
        if (homeSection == HomeSection::Explorer) {
          if (ftCursor < (int)flatTree.size() - 1)
            ftCursor++;
        } else {
          if (recentFileCursor < (int)recentFiles.size() - 1)
            recentFileCursor++;
        }
        continue;
      }
      // k / arrow-up: move cursor in focused section
      if (key == ARROW_UP || key == 'k') {
        if (homeSection == HomeSection::Explorer) {
          if (ftCursor > 0)
            ftCursor--;
        } else {
          if (recentFileCursor > 0)
            recentFileCursor--;
        }
        continue;
      }
      // Enter: open selected item
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
        } else {
          if (recentFileCursor < (int)recentFiles.size()) {
            openfile(recentFiles[recentFileCursor]);
            currentView = ScreenView::Editor;
          }
        }
        continue;
      }
      // Backspace: navigate up one directory in explorer
      if (key == 127 && homeSection == HomeSection::Explorer) {
        string parent = ftRoot.fullPath;
        size_t sl = parent.rfind('/');
        if (sl != string::npos && sl > 0) {
          chdir(parent.substr(0, sl).c_str());
          loadFileList();
        }
        continue;
      }
      // Ctrl+B: toggle sidebar
      if (key == 2) {
        showFileTree = !showFileTree;
        continue;
      }
      // Any other key: fall through to global handlers below
    }

    // Mouse Event Processor
    if (key == MOUSE_EVENT) {
      handleMouseClick(lastMouseEvent.x, lastMouseEvent.y, lastMouseEvent.btn,
                       lastMouseEvent.type);
      continue;
    }

    // Command Palette Mode
    if (currentMode == EditorMode::Palette) {
      if (key == '\x1b') {
        currentMode = EditorMode::Normal;
        paletteBuffer.clear();
        continue;
      }
      if (key == ARROW_UP || key == 'k') {
        if (paletteIdx > 0)
          paletteIdx--;
        continue;
      }
      if (key == ARROW_DOWN || key == 'j') {
        if (paletteIdx < (int)filteredPalette.size() - 1)
          paletteIdx++;
        continue;
      }
      if (key == '\r') {
        if (!filteredPalette.empty() &&
            paletteIdx < (int)filteredPalette.size()) {
          executeCommand(filteredPalette[paletteIdx].action);
        }
        paletteBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
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
      continue;
    }

    // AI Mode
    if (currentMode == EditorMode::AiInput) {
      if (key == '\x1b') {
        aiInputBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == '1') {
        aiChatHistory.push_back("User: Explain current function");
        aiChatHistory.push_back("AI: Analyzed function at line " +
                                to_string(cy + 1) +
                                ". Performs buffer rendering.");
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == '2') {
        aiChatHistory.push_back("User: Find bug & optimize");
        aiChatHistory.push_back(
            "AI: Memory allocations optimal. No memory leaks detected.");
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == '\r') {
        showAiPanel = true;
        aiChatHistory.push_back("User: " + aiInputBuffer);
        aiChatHistory.push_back("AI: Refactored your snippet successfully.");
        aiInputBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!aiInputBuffer.empty())
          aiInputBuffer.pop_back();
        continue;
      }
      if (key >= 32 && key <= 126) {
        aiInputBuffer.push_back((char)key);
      }
      continue;
    }

    // Command Mode
    if (currentMode == EditorMode::Command) {
      if (key == '\x1b') {
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == '\r') {
        executeCommand(commandBuffer);
        commandBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == 127 || key == DEL_KEY) {
        if (!commandBuffer.empty())
          commandBuffer.pop_back();
        continue;
      }
      if (key >= 32 && key <= 126) {
        commandBuffer.push_back((char)key);
      }
      continue;
    }

    // Search Mode
    if (currentMode == EditorMode::Search) {
      if (key == '\x1b') {
        searchBuffer.clear();
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == '\r') {
        currentMode = EditorMode::Normal;
        continue;
      }
      if (key == 'n') {
        if (!searchBuffer.empty()) {
          for (size_t r = cy + 1; r < lines.size(); ++r) {
            size_t pos = lines[r].find(searchBuffer);
            if (pos != string::npos) {
              cy = r;
              cx = pos;
              break;
            }
          }
        }
        continue;
      }
      if (key == 'N') {
        if (!searchBuffer.empty() && cy > 0) {
          for (int r = cy - 1; r >= 0; --r) {
            size_t pos = lines[r].find(searchBuffer);
            if (pos != string::npos) {
              cy = r;
              cx = pos;
              break;
            }
          }
        }
        continue;
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
            break;
          }
        }
      }
      continue;
    }

    // Escape Key -> Return Home
    if (key == '\x1b') {
      currentView = ScreenView::Home;
      continue;
    }

    if (key == 16) { // Ctrl+P -> Command Palette
      currentMode = EditorMode::Palette;
      paletteBuffer.clear();
      filteredPalette = paletteActions;
      paletteIdx = 0;
      continue;
    }

    if (key == 1) { // Ctrl+A -> Toggle AI Panel
      showAiPanel = !showAiPanel;
      if (showAiPanel) {
        currentMode = EditorMode::AiInput;
        aiInputBuffer.clear();
      }
      continue;
    }

    if (key == 2) { // Ctrl+B -> Toggle Sidebar File Tree
      showFileTree = !showFileTree;
      if (showFileTree)
        loadFileList();
      continue;
    }

    if (key == 6) { // Ctrl+F -> Toggle Incremental Find
      currentMode = EditorMode::Search;
      searchBuffer.clear();
      continue;
    }

    if (key == 20) { // Ctrl+T -> Toggle Embedded Terminal
      showTerminalPanel = !showTerminalPanel;
      continue;
    }

    if (key == 17) { // Ctrl+Q -> Save As & Exit
      string filename = promptSaveAs();
      savefile(filename);
      break;
    }

    if (key == 19) { // Ctrl+S -> Quick Save
      savefile(currentFile);
      continue;
    }

    if (key == ARROW_LEFT) {
      moveLeft();
      continue;
    }
    if (key == ARROW_RIGHT) {
      moveRight();
      continue;
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
      continue;
    }
    if (key == ARROW_DOWN) {
      if (showCompletion) {
        completionIdx = (completionIdx + 1) % completionList.size();
      } else if (showFileTree && ftCursor < (int)flatTree.size() - 1) {
        ftCursor++;
      } else {
        moveDown();
      }
      continue;
    }

    // Normal & Visual Modes
    if (currentMode == EditorMode::Normal ||
        currentMode == EditorMode::Visual) {
      if (key == 'i') {
        currentMode = EditorMode::Insert;
        pendingJ = false;
        continue;
      }
      if (key == 'a') {
        if (cx < (int)lines[cy].size())
          cx++;
        currentMode = EditorMode::Insert;
        pendingJ = false;
        continue;
      }

      if (key == 'v') {
        if (currentMode == EditorMode::Visual)
          currentMode = EditorMode::Normal;
        else {
          currentMode = EditorMode::Visual;
          selStartX = cx;
          selStartY = cy;
        }
        continue;
      }

      if (key == ')' || key == '0') {
        if (currentMode == EditorMode::Visual) {
          selStartX = 0;
          cx = lines[cy].size();
        } else {
          cx = 0;
        }
        continue;
      }

      if (key == 'e') {
        cx = lines[cy].size();
        continue;
      }
      if (key == ':') {
        currentMode = EditorMode::Command;
        commandBuffer.clear();
        continue;
      }
      if (key == '/') {
        currentMode = EditorMode::Search;
        searchBuffer.clear();
        continue;
      }

      if (key == 'u') {
        undoManager.undo(lines, cx, cy);
        continue;
      }
      if (key == 18) {
        undoManager.redo(lines, cx, cy);
        continue;
      }

      if (key == 'h') {
        moveLeft();
        continue;
      }
      if (key == 'l') {
        moveRight();
        continue;
      }
      if (key == 'k') {
        if (showFileTree && ftCursor > 0)
          ftCursor--;
        else
          moveUp();
        continue;
      }
      if (key == 'j') {
        if (showFileTree && ftCursor < (int)flatTree.size() - 1)
          ftCursor++;
        else
          moveDown();
        continue;
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
        continue;
      }

      if (key == 'w') {
        moveWordForward();
        continue;
      }
      if (key == 'b') {
        moveWordBackward();
        continue;
      }
      if (key == '$') {
        cx = lines[cy].size();
        continue;
      }
      if (key == 'g') {
        if (lastKeyChar == 'g') {
          cy = 0;
          cx = 0;
          lastKeyChar = 0;
        } else {
          lastKeyChar = 'g';
        }
        continue;
      }
      if (key == 'G') {
        cy = lines.size() - 1;
        cx = 0;
        continue;
      }

      if (key == 'x') {
        if (currentMode == EditorMode::Visual)
          deleteSelection();
        else if (cx < (int)lines[cy].size()) {
          undoManager.saveSnapshot(lines, cx, cy);
          lines[cy].erase(cx, 1);
          isModified = true;
        }
        continue;
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
        continue;
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
        continue;
      }

      if (key == 'p') {
        if (!yankBuffer.empty()) {
          undoManager.saveSnapshot(lines, cx, cy);
          lines.insert(lines.begin() + cy + 1, yankBuffer);
          cy++;
          isModified = true;
        }
        continue;
      }

      lastKeyChar = key;
      continue;
    }

    // Insert Mode
    if (currentMode == EditorMode::Insert) {
      if (key == '\x1b') {
        currentMode = EditorMode::Normal;
        pendingJ = false;
        showCompletion = false;
        continue;
      }

      if (key == '\t' || key == '\r') {
        if (showCompletion && !completionList.empty()) {
          undoManager.saveSnapshot(lines, cx, cy);
          string selectedComp = completionList[completionIdx];
          lines[cy].insert(cx, selectedComp);
          cx += selectedComp.size();
          showCompletion = false;
          isModified = true;
          continue;
        }
      }

      if (pendingJ) {
        if (key == 'k') {
          pendingJ = false;
          currentMode = EditorMode::Normal;
          showCompletion = false;
          continue;
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
        continue;
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
        continue;
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
        continue;
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
                                 lines[cy][wordStart - 1] == '#')) {
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

  cleanupScreen();
  return 0;
}
