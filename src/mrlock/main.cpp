#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include "include/vector.hpp"

int main(void) {

  const int MAX_OPS = 6400;
  const int INSERT = 0, ERASE = 1;

  const int LIMIT = 30;

  const int MAX_NUM_THREADS = 32;
  for(int num_threads = 1; num_threads <= MAX_NUM_THREADS; ++num_threads) {
    std::cout << num_threads;

    for(int type : {INSERT, ERASE}) {
      blocking::vector<int> vec;

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
              vec.push_back(new int{x});
            } else if(!do_pushback) {
              if(cur_op == 0 && size > 0) {
                vec.insert(r() % size, new int{x});
              } else if(cur_op == 1) {
                vec.push_back(new int{x});
              } else if(cur_op == 2 && size > 0) {
                vec.at(r() % size);
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
              vec.push_back(new int{x});
            } else if(!do_pushback) {
              if(cur_op == 0 && size > 0) {
                vec.erase(r() % size);
              } else if(cur_op == 1) {
                vec.push_back(new int{x});
              } else if(cur_op == 2 && size > 0) {
                vec.at(r() % size);
              }
            }
          } catch(...) {
          }
        }
      };

      auto start_time = std::chrono::steady_clock::now();

      //add dummy values initially
      for(int i = 0; i < 10; ++i) {
        vec.push_back(new int{i});
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


  // blocking::vector<int> v;

  // auto go = [&](int id, int* me) {
  //   std::mt19937 gen(id);

  //   std::vector<int> got;

  //   for (int i = 0; i < 10000; ++i) {
  //     const int pick = gen() % 5;
  //     try {
  //       if (pick == 0) {
  //         v.push_back(me);
  //       } else if (pick == 1) {
  //         v.pop_back();
  //       } else if (pick == 2) {
  //         v.insert(gen() % (1 + v.size()), me);
  //       } else if (pick == 3) {
  //         v.erase(gen() % (1 + v.size()));
  //       } else if (pick == 4) {
  //         auto val = v[gen() % (1 + v.size())];
  //         if (val) {
  //           got.push_back(*val);
  //         }
  //       }
  //     } catch (...) {
  //     }
  //     std::this_thread::sleep_for(std::chrono::microseconds(1));
  //   }
  // };

  // std::vector<std::thread> t;
  // std::vector<int*> me;
  // for (int i = 0; i < 4; ++i) {
  //   me.push_back(new int(i));
  //   t.push_back(std::thread(go, i, me.back()));
  // }

  // for (auto& e : t) {
  //   if (e.joinable()) {
  //     e.join();
  //   }
  // }

  // for (std::size_t i = 0; i < v.size(); ++i) {
  //   auto e = v[i];
  //   if (e) {
  //     std::cout << *e << " ";
  //   } else {
  //     std::cout << "nil ";
  //   }
  // }
  // std::cout << "\n";

  // for (int* m : me) {
  //   delete m;
  // }

  return 0;
}
