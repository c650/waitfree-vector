#include <chrono>
#include <iostream>
#include <thread>
#include "include/vector.hpp"

int main(void) {
  sequential::vector<int> v;

  auto go = [&](int id) {
    int* me = new int{id};
    for (int i = 0; i < 100; ++i) {
      v.push_back(me);
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  };

  std::vector<std::thread> t;
  for (int i = 0; i < 3; ++i) {
    t.push_back(std::thread(go, i));
  }

  for (auto& e : t) {
    e.join();
  }

  for (std::size_t i = 0; i < v.size(); ++i) {
    auto e = v[i];
    if (e) {
      std::cout << *e << " ";
    } else {
      std::cout << "nil ";
    }
  }
  std::cout << "\n";

  return 0;
}
