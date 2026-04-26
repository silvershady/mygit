#include <iostream>
using namespace std;

int main() {
  cout << "Enter a number for factorial: ";
  int n, fact = 1;
  cin >> n;

  if (n < 0) {
    cout << "Factorial is not defined for negative numbers." << endl;
  } else {
    for (int i = 1; i <= n; i++) {
      fact *= i;
    }
    cout << "Factorial of " << n << " is " << fact << endl;
  }

  return 0;
}
