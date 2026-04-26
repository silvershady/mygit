#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <vector>
using namespace std;
struct termios orig_termios;

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
int main() {
  enableRawMode();
  while (true) {

    char c = readkey();
    if (c == 17)
      break;
    if (c == '\x1b') {
      char seq[2];
      read(STDIN_FILENO, &seq[0], 1);
      read(STDIN_FILENO, &seq[1], 1);
      continue;
    }
    write(STDOUT_FILENO, &c, 1);
  }
}
