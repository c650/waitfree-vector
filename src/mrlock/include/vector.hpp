#pragma once

#include <cstddef>
#include <sstream>
#include <stdexcept>

#include <strategy/lockablebase.h>
#include <strategy/mrlockable.h>

namespace blocking {
  template <typename T>
  struct vector {
    // data stuff
    T** data;
    std::size_t sz, cap; // capacity is the actual size of data

    ResourceAllocatorBase* resource_alloc;

    LockableBase* my_lock;

    /********* begin constructors *********/
    vector(std::size_t sz) : sz(sz), cap(sz) {
      data = new T*[cap]();

      resource_alloc = new MRResourceAllocator(1);
      my_lock = resource_alloc->CreateLockable({0});
    }

    vector(void) : vector(0) {
    }

    ~vector(void) {
      delete[] data;
      delete resource_alloc;
    }
    /********* end constructors *********/

    /********* begin internal functions *********/

  private:
    void resize(std::size_t new_cap) {
      if (new_cap < cap) {
        sz = new_cap;
        return;
      }
      T** new_data = new T*[new_cap];
      for (std::size_t i = 0; i < cap; ++i) {
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

  public:
    /********* begin vector functions *********/
    void push_back(T* const x) {
      my_lock->Lock();

      check_cap();
      data[sz++] = x;

      my_lock->Unlock();
    }

    void pop_back(void) {
      my_lock->Lock();
      if (sz == 0) {
        my_lock->Unlock();
        throw std::out_of_range{"vector is empty"}; // empty vector
      }

      --sz;
      my_lock->Unlock();
    }

    void clear(void) {
      my_lock->Lock();
      resize(0);
      my_lock->Unlock();
    }

    T* at(std::size_t pos) {
      my_lock->Lock();
      if (pos < 0 || pos >= sz) {
        std::stringstream ss;
        ss << "position " << pos << " is invalid for vector of size " << sz;

        my_lock->Unlock();

        throw std::out_of_range{ss.str()};
      }

      auto ret = data[pos];
      my_lock->Unlock();

      return ret;
    }

    T* operator[](int pos) {
      return at(pos);
    }

    void insert(std::size_t pos, T* const x) {
      my_lock->Lock();

      if (pos < 0 || pos > sz) {
        std::stringstream ss;
        ss << "cannot insert at position " << pos << " for vector of size "
           << sz;

        my_lock->Unlock();
        throw std::out_of_range{ss.str()};
      }

      check_cap();

      for (int i = sz; i > static_cast<int>(pos); --i) {
        data[i] = data[i - 1];
      }

      data[pos] = x;

      ++sz;

      my_lock->Unlock();
    }

    void erase(std::size_t pos) {
      my_lock->Lock();

      if (pos < 0 || pos >= sz) {
        std::stringstream ss;
        ss << "cannot erase at position " << pos << " in vector of size " << sz;

        my_lock->Unlock();

        throw std::out_of_range{ss.str()};
      }

      for (auto i = pos; i + 1 < sz; ++i) {
        data[pos] = data[pos + 1];
      }

      --sz;

      my_lock->Unlock();
    }

    std::size_t size(void) {
      return sz;
    }

    std::size_t capacity(void) {
      return cap;
    }

    /********* end vector functions *********/
  };
}; // namespace sequential
