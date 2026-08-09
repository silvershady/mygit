#pragma once
#include "EditorState.h"

struct SystemInfo {
  string osName, cpuModel, shell, terminalName, uptime;
  long totalMemMB = 0, usedMemMB = 0;
  long totalMemKB = 0, usedMemKB = 0;
  long totalDiskGB = 0, usedDiskGB = 0;
  unsigned long long totalDiskBytes = 0, usedDiskBytes = 0;
};

SystemInfo getSystemInfo();

// Core logic exports
void processKey(int key);
void handleMouseClick(int x, int y, int btn, char type);
void openfile(const string &filename);
void createNewFile();
void savefile(const string &filename);
void loadFileList();
void loadRecentFiles();
void enableRawMode();
void disableRawMode();
void moveWordEnd();
void executeLanguageFile(const string &filename);
void updateSyntaxHighlighting();


int readkey();
void refresh_screen();
void cleanupScreen();

// From index_core.cpp legacy
int legacy_main();
