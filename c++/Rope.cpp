#include <iostream>
using namespace std;

const int LEAF_LEN = 4;

class Rope {
public:
  Rope *left, *right;
  char *str;
  int lCount;

  Rope() {
    left = right = NULL;
    str = NULL;
    lCount = 0;
  }
};

// Create rope
Rope *build(char a[], int l, int r) {
  Rope *node = new Rope();

  if (r - l + 1 <= LEAF_LEN) {
    node->str = new char[r - l + 2]; // +1 for '\0'
    int j = 0;
    for (int i = l; i <= r; i++)
      node->str[j++] = a[i];
    node->str[j] = '\0';
    return node;
  }

  int m = (l + r) / 2;
  node->left = build(a, l, m);
  node->right = build(a, m + 1, r);
  node->lCount = m - l + 1;

  return node;
}

// Print rope
void print(Rope *root) {
  if (!root)
    return;

  if (root->str != NULL) {
    cout << root->str;
    return;
  }

  print(root->left);
  print(root->right);
}

// Concatenate
Rope *concat(Rope *r1, Rope *r2, int len1) {
  Rope *root = new Rope();
  root->left = r1;
  root->right = r2;
  root->lCount = len1;
  return root;
}

int main() {
  char a[] = "Titus ";
  char b[] = "Emperor";

  Rope *r1 = build(a, 0, 5);
  Rope *r2 = build(b, 0, 5);

  Rope *result = concat(r1, r2, 6);

  print(result);

  return 0;
}
