#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "include/vector.hpp"

const int NUM_THREADS = 3;

waitfree::vector<int> vec(NUM_THREADS);

void go(int id) {
  std::mt19937 bloop(
      std::chrono::system_clock::now().time_since_epoch().count());
  for (int i = 0; i < 100; ++i) {
    vec.wf_push_back(id, new int{(id + 1) * 100 + i});
    std::this_thread::sleep_for(std::chrono::milliseconds(bloop() % 10));
  }
}

int main(void) {
  std::vector<std::thread> threads;
  for (int i = 1; i < NUM_THREADS; ++i) {
    threads.push_back(std::thread(go, i));
  }

  for (auto& t : threads) {
    t.join();
  }

  for (std::size_t i = 0; i < vec.size(); ++i) {
    std::cout << vec.at(0, i).second[0] << '\n';
  }

  return 0;
}
