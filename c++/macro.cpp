#include <iostream>
int main(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i)
    std::cout << argv[i] << std::endl;
  std::cout << "invoked with " << argv[0] << std::endl;
}
