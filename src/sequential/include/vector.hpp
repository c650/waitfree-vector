#pragma once

#include <cstddef>
#include <sstream>
#include <stdexcept>


namespace sequential {

  template <typename T>
  struct vector {
    // data stuff
    T** data;
    std::size_t sz, cap; // capacity is the actual size of data

    /********* begin constructors *********/
    vector(std::size_t sz) : sz(sz), cap(sz) {
      if (sz < 0)
        throw; // check for negative size
      data = new T*[cap]();
    }

    vector(void) : vector(0) {
    }

    ~vector(void) {
      delete[] data;
    }
    /********* end constructors *********/

    /********* begin internal functions *********/
    void resize(std::size_t new_cap) {
      if (new_cap < cap) {
        sz = new_cap;
        return;
      }
      T** new_data = new T*[new_cap];
      for (int i = 0; i < cap; ++i) {
        new_data[i] = data[i];
      }
      delete[] data;
      data = new_data;
      cap = new_cap;
    }

    // this is called when capacity is implicitly increased
    void increase_cap(void) {
      std::size_t new_cap = cap * 2 + 1;
      resize(new_cap);
    }

    void check_cap(void) {
      if (sz >= cap) {
        increase_cap();
      }
    }
    /********* end internal functions *********/

    /********* begin vector functions *********/
    void push_back(T* const x) {
      check_cap();
      data[sz++] = x;
    }

    void pop_back(void) {
      if (sz == 0) {
        throw; // empty vector
      }
      --sz;
    }

    void clear(void) {
      resize(0);
    }

    T* at(std::size_t pos) {
      if (pos < 0 || pos >= sz) {
        std::stringstream ss;
        ss << "position " << pos << " is invalid for vector of size " << sz;
        throw std::out_of_range{ss.str()};
      }
      return data[pos];
    }

    T* operator[](int pos) {
      return at(pos);
    }

    void insert(std::size_t pos, T* const x) {
      if (pos < 0 || pos > sz) {
        std::stringstream ss;
        ss << "cannot insert at position " << pos << " for vector of size "
           << sz;
        throw std::out_of_range{ss.str()};
      }

      check_cap();

      for (auto i = sz - 1; i >= pos; --i) {
        data[i + 1] = data[i];
      }

      data[pos] = x;

      ++sz;
    }

    void erase(std::size_t pos) {
      if (pos < 0 || pos >= sz) {
        std::stringstream ss;
        ss << "cannot erase at position " << pos << " in vector of size " << sz;
        throw std::out_of_range{ss.str()};
      }

      for (auto i = pos; i + 1 < sz; ++i) {
        data[pos] = data[pos + 1];
      }

      --sz;
    }

    std::size_t size(void) {
      return sz;
    }

    std::size_t capacity(void) {
      return cap;
    }

    /********* end vector functions *********/
  };
};
