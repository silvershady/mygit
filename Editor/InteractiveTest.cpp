#include <iostream>
#include <string>

int main() {
    std::cout << "Enter a number: ";
    int num;
    if (std::cin >> num) {
        std::cout << "You entered: " << num << "\n";
        std::cout << "Square is: " << (num * num) << "\n";
    }
    return 0;
}
