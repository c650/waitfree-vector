#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#include "include/vector.hpp"

void test_pushback(const int NUM_THREADS) {
  std::cout << "TEST PUSHBACK " << NUM_THREADS << " threads\n";
  waitfree::vector<int> vec(NUM_THREADS);

  auto go = [&](int id) {
    // std::mt19937 bloop(
    //     std::chrono::system_clock::now().time_since_epoch().count());
    for (int i = 0; i < 30; ++i) {
      vec.wf_push_back(id, new int{(id + 1) * 100 + i});
      // std::this_thread::sleep_for(std::chrono::milliseconds(bloop() % 10));
    }
  };

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
}

void test_cwrite(const int NUM_THREADS) {
  const int LEN = 44;

  waitfree::vector<int> vec(NUM_THREADS);
  for (int i = 0; i < LEN; ++i) {
    vec.wf_push_back(0, new int{0});
  }

  std::vector<std::vector<int>> cnt(NUM_THREADS, std::vector<int>(LEN));

  auto go = [&](int id) {
    for (int i = 0; i < 1000; ++i) {
      const std::size_t pos = i % vec.size();
      auto prev = vec.at(id, pos).second;
      if (vec.cwrite(id, pos, prev, new int{*prev + 1}).first) {
        ++cnt[id][pos];
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 1; i < NUM_THREADS; ++i) {
    threads.push_back(std::thread{go, i});
  }

  for (auto& e : threads) {
    e.join();
  }

  std::vector<int> tot(LEN);
  for (const auto& row : cnt) {
    for (int i = 0; i < LEN; ++i) {
      tot[i] += row[i];
      std::cout << row[i] << " ";
    }
    std::cout << "\n";
  }
  std::cout << "-------------\n";

  for (int i = 0; i < LEN; ++i) {
    std::cout << vec.at(0, i).second[0] << " ";
  }
  std::cout << "\n";
  for (int i = 0; i < LEN; ++i) {
    std::cout << tot[i] << " ";
    assert(tot[i] == vec.at(0, i).second[0]);
  }
  std::cout << "\n";
}

int main(void) {
  test_pushback(16);
  // test_cwrite(16);

  return 0;
}
