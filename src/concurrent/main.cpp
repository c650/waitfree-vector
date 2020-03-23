#include <iostream>

#include "include/vector.hpp"

int main(void) {
  waitfree::vector<int> v;

  for (int i = 0; i < 10; ++i) {
    std::cout << v.wf_push_back(new int{69 + i}) << "\n";
  }

  v.cwrite(2, v.at(2).second, new int{666});

  for (int i = 0; i < 10; ++i) {
    // std::cout << *v.wf_popback().second << '\n';
    std::cout << *v.at(i).second << "\n";
  }

  return 0;
}
