#include <cstddef>

namespace waitfree {

  using word_t = std::size_t;

  struct vector;

  enum DescriptorType { PushDescr, PushSubDescr };
  enum AnnouncementType {};

  struct base_descriptor {
    virtual DescriptorType type(void) = 0;
    virtual complete(void) = 0;
  };

  struct base_announcement {
    virtual AnnouncementType type(void) = 0;
    virtual complete(void) = 0;
  };

  struct Contiguous {
    vector* vec;
    Contiguous* old;
    const std::size_t capacity;

    word_t* array;

    Contiguous(vector* vec, Contiguous* old, std::size_t capacity)
        : vec(vec), old(old), capacity(capacity) {
      array = new word_t[capacity];
      std::fill(array, array + capacity, NOT_COPIED);
    }
  };

  struct vector {};

}; // namespace waitfree
