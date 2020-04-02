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

void test_all(int MAX_NUM_THREADS) {
  const int MAX_OPS = 6400;
  const int INSERT = 0, ERASE = 1;

  const int LIMIT = 25;

  for(int num_threads = 1; num_threads <= MAX_NUM_THREADS; ++num_threads) {
    std::cout << num_threads;

    for(int type : {INSERT, ERASE}) {
      waitfree::vector<int> vec(num_threads + 1);

      int each_thread = MAX_OPS / num_threads;
      int extra = MAX_OPS % num_threads;
      std::vector<int> ops_per_thread(num_threads + 1);
      for(int i = 1; i <= num_threads; ++i) {
          ops_per_thread[i] = each_thread;
        if(i <= extra) {
          ops_per_thread[i]++;
        }
      }

      auto go_insert = [&](int id) {
        std::mt19937 r(id);
        int tot_ops = ops_per_thread[id];
        for(int i = 0; i < tot_ops; ++i) {
          const int cur_op = r() % 3;
          const bool do_pushback = (r()%100+100)%100 < LIMIT;
          try {
            int x = r();
            int size = vec.size();
            if(do_pushback) {
              vec.wf_push_back(id, new int{x});
            } else if(!do_pushback) {
              if(cur_op == 0 && size > 0) {
                vec.insertAt(id, r() % size, new int{x});
              } else if(cur_op == 1 && size > 0) {
                vec.at(id, r() % size);
              } else if(cur_op == 2 && size > 0) {
                int pos = r() % size;
                int* old = vec.at(id, pos).second;
                vec.cwrite(id, pos, old, new int{x});
              }
            }
          } catch(...) {
          }
        }
      };

      auto go_erase = [&](int id) {
        std::mt19937 r(id);
        int tot_ops = ops_per_thread[id];
        for(int i = 0; i < tot_ops; ++i) {
          const int cur_op = r() % 3;
          const bool do_pushback = (r()%100+100)%100 < LIMIT;
          try {
            int x = r();
            int size = vec.size();
            if(do_pushback) {
              vec.wf_push_back(id, new int{x});
            } else if(!do_pushback) {
              if(cur_op == 0 && size > 0) {
                vec.eraseAt(id, r() % size);
              } else if(cur_op == 1 && size > 0) {
                vec.at(id, r() % size);
              } else if(cur_op == 2 && size > 0) {
                int pos = r() % size;
                int* old = vec.at(id, pos).second;
                vec.cwrite(id, pos, old, new int{x});
              }
            }
          } catch(...) {
          }
        }
      };

      auto start_time = std::chrono::steady_clock::now();

      //add dummy values initially
      for(int i = 0; i < 10; ++i) {
        vec.wf_push_back(0, new int{i});
      }

      std::vector<std::thread> threads;
      for(int i = 1; i <= num_threads; ++i) {
        if(type == INSERT) {
          threads.emplace_back(go_insert, i);
        } else {
          threads.emplace_back(go_erase, i);
        }
      }
      for(auto& cur : threads) {
        cur.join();
      }

      auto end_time = std::chrono::steady_clock::now();
      auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

      // std::cout << "\t" << "TOTAL TIME FOR " << (type == INSERT ? "INSERT" : "ERASE") << ": " << elapsed_time << " ms" << std::endl;

      std::cout << "," << elapsed_time;
      std::cout.flush();
    }
    // std::cout << '\n';
    std::cout << std::endl;
  }

  /*
  // std::cerr << (reinterpret_cast<int*>(waitfree::NotValue) == nullptr) << std::endl;;
  // return;
  NUM_THREADS = 16;
  const int NUM_ITERS = 100;

  waitfree::vector<int> vec(NUM_THREADS);

  for (int i = 0; i < NUM_ITERS * 2 * (NUM_THREADS-1); ++i) {
    int* cur = new int{i};
    // std::cerr << "INIT ELEMENT: " << cur << ' ' << *cur << std::endl;
    vec.wf_push_back(0, cur);
  }
  
  // for(int i = 0; i < 10; ++i) {
  //   vec.wf_push_back(0, new int{i});
  // }
  // for (int i = 0; i < 5; ++i) {
  //   vec.eraseAt(0, 2);
  //   // vec.insertAt(0, 2, new int{i + 10});
  // }
  // for (std::size_t i = 0; i < vec.size(); ++i) {
  //   std::cerr << *vec.at(0, i).second << ' ';
  // }
  // std::cerr << std::endl;
  // return;
  

  std::vector<int> succ(NUM_THREADS);

  auto go = [&](int id) {
    std::mt19937 r(
        std::chrono::system_clock::now().time_since_epoch().count());
    for (int i = 0; i < NUM_ITERS; ++i) {
      const std::size_t pos = (r() % vec.size() + vec.size()) % vec.size();
      // const std::size_t pos = vec.size() - 1;
      // const std::size_t pos = 0;
      // int x = i; //r();
      // int* y = new int{x};
      // if(y == nullptr) throw;
      // bool cur = vec.insertAt(id, pos, y);
      bool cur = vec.eraseAt(id, pos);
      if(!cur) {
        std::printf("FAILED: %d %d\n", (int) pos, (int) vec.size());
        // std::cerr << "FAILED: " << pos << ' ' << vec.size() << std::endl;
        exit(0);
      }
      // for(int tries = 0; !cur; ++tries) {
      //   if(tries > 100) {
      //     auto xx = vec.at(id, pos).second;
      //     std::cerr << "trying to insert " << x << " at pos " << pos << " tid: " << id << " total size: " << vec.size() << " element: " << xx << std::endl;
      //     break;
      //   }
      //   cur = vec.insertAt(id, pos, new int{x});
      // }
      succ[id] += cur;
    }
  };
  
  std::size_t initial_size = vec.size();

  std::cout << "INITIAL SIZE: " << initial_size << std::endl;

  std::vector<std::thread> threads;
  for(int i = 1; i < NUM_THREADS; ++i) {
    threads.emplace_back(go, i);
  }
  for(auto& cur : threads) {
    cur.join();
  }

  std::size_t expected_size = initial_size + NUM_ITERS * (NUM_THREADS - 1);
  // std::size_t expected_size = initial_size; for(int x : succ) expected_size += x;
  std::size_t actual_size = vec.size();

  std::cout << "EXPECTED SIZE: " << expected_size << std::endl;
  std::cout << "ACTUAL SIZE: " << actual_size << std::endl;

  for(int i = 0; i < 100; ++i) {
    std::cout << *vec.at(0, i).second << ' ';
  }
  std::cout << std::endl;
  */
}

int main(void) {
  // test_pushback(16);
  // test_popback(16);
  // test_cwrite(16);
  // test_erase_insert(32);
  test_all(32);

  return 0;
}
