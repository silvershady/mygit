#include <iostream>
#include <string>
using namespace std;
int main() {
    string a = "\u2588";
    string b = "\\u2588";
    cout << "Real unicode: [" << a << "] bytes=" << a.size() << endl;
    cout << "Escaped text: [" << b << "] bytes=" << b.size() << endl;
    return 0;
}
