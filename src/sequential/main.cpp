#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include "include/vector.hpp"

int main(void) {
  sequential::vector<int> v;

  for (int i = 0; i < 1000; ++i) {
    v.push_back(new int(i));
  }

  v.insert(0, new int(10000));

  v.erase(3);

  v.pop_back();

  for (int i = 0; i < static_cast<int>(v.size()); ++i) {
    std::cout << *v[i] << " ";
    delete v[i];
  }
  std::cout << "\n";

  return 0;
}
