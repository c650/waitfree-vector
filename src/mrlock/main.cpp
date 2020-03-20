#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include "include/vector.hpp"

int main(void) {
  blocking::vector<int> v;

  auto go = [&](int id) {
    std::mt19937 gen(id);

    std::vector<int> got;
    int* me = new int{id};

    for (int i = 0; i < 10000; ++i) {
      const int pick = gen() % 5;
      try {
        if (pick == 0) {
          v.push_back(me);
        } else if (pick == 1) {
          v.pop_back();
        } else if (pick == 2) {
          v.insert(gen() % (1 + v.size()), me);
        } else if (pick == 3) {
          v.erase(gen() % (1 + v.size()));
        } else if (pick == 4) {
          auto val = v[gen() % (1 + v.size())];
          if (val) {
            got.push_back(*val);
          }
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  };

  std::vector<std::thread> t;
  for (int i = 0; i < 4; ++i) {
    t.push_back(std::thread(go, i));
  }

  for (auto& e : t) {
    if (e.joinable()) {
      e.join();
    }
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
