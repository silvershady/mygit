#pragma once
#include "EditorState.h"

void openfile(const string &filename);
void savefile(const string &filename);
void processKey(int key);
void loadFileList();
void loadRecentFiles();
void updateSyntaxHighlighting();
SystemInfo getSystemInfo();
void handleMouseClick(int x, int y, int btn, char type);
