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
    try {
      threads.push_back(std::thread{go, i});
    } catch (...) {
      std::cerr << "fail to make " << i << "\n";
    }
  }

  for (auto& t : threads) {
    t.join();
  }

  for (std::size_t i = 0; i < vec.size(); ++i) {
    std::cout << vec.at(0, i).second[0] << '\n';
  }
}

void test_popback(const int NUM_THREADS) {
  const int LEN = 30;

  std::cout << "TEST POPBACK " << NUM_THREADS << " threads\n";
  waitfree::vector<int> vec(NUM_THREADS);

  std::vector<int> good(NUM_THREADS);

  auto go = [&](int id) {
    for (int i = 0; i < LEN; ++i) {
      good[id] += vec.wf_popback(id).first;
    }
  };

  for (int i = 0; i < NUM_THREADS * LEN; ++i) {
    vec.wf_push_back(0, new int{i});
  }

  std::vector<std::thread> threads;
  for (int i = 1; i < NUM_THREADS; ++i) {
    try {
      threads.push_back(std::thread{go, i});
    } catch (...) {
      std::cerr << "fail to make " << i << "\n";
    }
  }

  for (auto& t : threads) {
    t.join();
  }

  for (std::size_t i = 0; i < vec.size(); ++i) {
    std::cout << vec.at(0, i).second[0] << '\n';
  }

  for (int i = 0; i < NUM_THREADS; ++i) {
    std::cout << good[i] << " ";
  }
  std::cout << "\n";
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
    try {
      threads.push_back(std::thread{go, i});
    } catch (...) {
      std::cerr << "fail to make " << i << "\n";
    }
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

//only tests functionality
void test_insertAt(const int NUM_THREADS) {
  waitfree::vector<int> vec(NUM_THREADS);

  // vec.wf_push_back(0, new int{1});
  // vec.wf_push_back(0, new int{100});
  for(int i = 0; i < 10; ++i) {
    vec.wf_push_back(0, new int{i});
  }
  for(int i = 0; i < 5; ++i) {
    vec.eraseAt(0, 2);
    // vec.insertAt(0, 0, new int{i + 10});
  }
  for(int i = 0; i < vec.size(); ++i) {
    std::cerr << *vec.at(0, i).second << ' ';
  }
  std::cerr << std::endl;
  // vec.insertAt(0, 0, new int{-100});
  // std::cerr << vec.at(0, 0).first << std::endl;
  // std::cerr << "PREV SIZE: " << vec.size() << std::endl;
  // bool res = vec.eraseAt(0, 0);
  // bool res = vec.insertAt(0, 0, new int{10000});
  // std::cerr << "AFTER ERASE: " << (res ? "success" : "failed") << ' ' << vec.size() << std::endl;
  // std::cerr << "HERE: " << (static_cast<int*>(nullptr) == nullptr) << std::endl;
  // std::cout << vec.at(0, 0).first << ' ' << *vec.at(0, 0).second << std::endl;
  return;
}

int main(void) {
  // test_pushback(16);
  // test_popback(16);
  // test_cwrite(16);
  test_insertAt(16);

  return 0;
}
