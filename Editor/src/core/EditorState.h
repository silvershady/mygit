#pragma once
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

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
  Normal, Insert, Visual, Command, Search, AiInput, Palette
};
enum class HomeSection { Explorer, RecentFiles, QuickActions, SystemInfo };

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
    if (undoStack.empty()) return false;
    redoStack.push_back({l, c_x, c_y});
    BufferState prev = undoStack.back();
    undoStack.pop_back();
    l = prev.lines; c_x = prev.cx; c_y = prev.cy;
    return true;
  }

  bool redo(vector<string> &l, int &c_x, int &c_y) {
    if (redoStack.empty()) return false;
    undoStack.push_back({l, c_x, c_y});
    BufferState next = redoStack.back();
    redoStack.pop_back();
    l = next.lines; c_x = next.cx; c_y = next.cy;
    return true;
  }
};

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

struct Theme {
  string name;
  string symbol;
  string primary;
  string secondary;
  string text;
  string inactive;
  string border;
  string warning;
  string error;
  uint8_t pR, pG, pB;
  uint8_t sR, sG, sB;
};

extern const vector<Theme> themes;
extern int activeThemeIdx;

extern ScreenView currentView;
extern EditorMode currentMode;
extern HomeSection homeSection;
extern bool isMaximized;
extern bool collapseExplorer;
extern bool collapseRecent;
extern bool shouldExitApp;
extern int animFrame;
extern int newFileAnimTimer;
extern float sidebarAnimWidth;

extern vector<string> lines;
extern int cx;
extern int cy;
extern int rowOffset;
extern int colOffset;
extern int screenRows;
extern int screenCols;

extern string commandBuffer;
extern string searchBuffer;
extern string paletteBuffer;
extern string currentFile;
extern bool isModified;

extern vector<string> openTabs;
extern int activeTabIdx;

extern FileNode ftRoot;
extern vector<FlatEntry> flatTree;
extern int ftCursor;
extern int ftScrollOff;

extern vector<string> recentFiles;
extern int recentFileCursor;
extern int quickActionCursor;

extern string toastMsg;
extern time_t toastTime;

extern MouseEvent lastMouseEvent;
extern int selStartX;
extern int selStartY;
extern bool isMouseDragging;
extern string yankBuffer;
extern int matchRow;
extern int matchCol;
extern bool showFileTree;

extern vector<string> completionList;
extern bool showCompletion;
extern int completionIdx;

extern vector<string> paletteItems;
extern int paletteIdx;

extern bool showTerminalPanel;
extern int terminalHeight;
extern vector<string> terminalOutput;

extern bool showAiPanel;
extern int aiPanelWidth;
extern string aiOutputBuffer;
extern string aiInputBuffer;

extern bool pendingJ;
extern UndoManager undoManager;

extern bool showContextMenu;
extern int contextMenuX;
extern int contextMenuY;
