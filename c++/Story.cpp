#include <iostream>
#include <string>
using namespace std;
int main() {
  string name;
  int choice;
  cout << "Enter Your Name" << endl;
  cin >> name;
  double credits = 100;
  cout << "The amount of credits is everything in this  story So be careful "
          "while using it"
       << endl;
  cout << "You initally Has 100 credits" << credits << endl;
  cout << "Hi Traveller\n"
       << name
       << "\nThis is a Story NOT any HIStory or HERstory It's Your Story"
       << endl;
  cout << "Do you enjoy living" << endl
       << "\n Type 1 for Yes And 0 for NO" << endl;
  cin >> choice;
  if (choice == 1) {
    cout << "\nwill you Hate your life If you had  a Million $ ,Good Life "
            ",POWER,WEALTH,RESPECT..."
         << "\033[31mBut One condition You have to hate it\033[0m\n " << endl;
  }
}
