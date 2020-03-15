#include <iostream>
#include "include/vector.hpp"

int main(void) {
  sequential::vector<int> v(10);
  v.push_back(new int{1});

  for (int i = 0; i < v.size(); ++i)
    std::cout << v[i] << "\n";

  return 0;
}
