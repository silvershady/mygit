#include <fstream>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

struct termios orig_termios;

vector<string> lines(1);

int cx = 0;
int cy = 0;

void disableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);

  atexit(disableRawMode);

  struct termios raw = orig_termios;

  raw.c_iflag &= ~(IXON | ICRNL);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cflag |= (CS8);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

char readkey() {
  char c;
  read(STDIN_FILENO, &c, 1);
  return c;
}

void refresh_screen() {
  system("clear");

  for (auto &line : lines) {
    cout << line << endl;
  }
}

void savefile(string filename) {
  ofstream file(filename);

  for (auto &line : lines) {
    file << line << "\n";
  }
}

void openfile(string filename) {
  ifstream file(filename);

  string line;

  lines.clear();

  while (getline(file, line)) {
    lines.push_back(line);
  }

  if (lines.empty()) {
    lines.push_back("");
  }
}

int main() {
  openfile("text.txt");

  enableRawMode();

  while (true) {

    refresh_screen();

    char c = readkey();
    if (c == 127) {
      if (cx > 0) {
        lines[cy].erase(cx - 1, 1);
        cx--;
      }
    }

    if (c == 17)
      break;

    if (c == 19)
      savefile("text.txt");
    if (c == '\r') {
      string current = lines[cy];
      string left = current.substr(0, cx);
      string right = current.substr(cx);
      lines[cy] = left;
      lines.insert(lines.begin() + cy + 1, right);
      cy++;
      cx = 0;
      continue;
    }

    if (c >= 32 && c <= 126) { // buffer this inserts into lines[0]
      lines[cy].insert(cx, 1, c);
      cx++;
    }
  }

  return 0;
}
