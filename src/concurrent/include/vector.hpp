#include <algorithm>
#include <atomic>
#include <cstddef>
#include <map>
#include <utility>

namespace waitfree {

  const int LIMIT = 6;

  // vector type declaration
  template <typename T>
  struct vector;

  // enum types
  enum DescriptorType { PUSH_DESCR, POP_DESCR, POP_SUB_DESCR };
  enum DescriptorState { Undecided, Failed, Passed };
  enum OpType { POP_OP, PUSH_OP };

  // IsDescriptor when non-nul | 0b01
  // NotValue when null | 0b00
  // NotCopied when null | 0b01
  // Resizing when non-null | 0b10

  const std::size_t NotValue = 0b00;
  const std::size_t NotCopied = 0b01;

  enum BitMarkings { IsDescriptor = 0b01, Resize = 0b10 };

  // helperCas
  template <typename U>
  bool helper_cas(std::atomic<U>& a, U expected, U replacewith) {
    return a.compare_exchange_strong(expected, replacewith);
  }

  // virtual types
  struct base_descriptor {
    virtual DescriptorType type(void) const = 0;
    virtual bool complete(void) = 0;
  };

  struct base_op {
    virtual OpType type(void) const = 0;
    virtual bool complete(void) = 0;
  };

  // type declarations
  template <typename T>
  struct PopDescr;

  template <typename T>
  struct PopSubDescr;

  template <typename T>
  struct PopOp;

  // descriptor implementations
  template <typename T>
  struct PopDescr : public base_descriptor {
    vector<T>* vec;
    std::size_t pos;
    std::atomic<PopSubDescr<T>*> child;

    PopDescr(vector<T>* vec, std::size_t pos)
        : vec(vec), pos(pos), child(nullptr) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::POP_DESCR;
    }

    bool complete(void) override {
      std::atomic<T*>& spot = this->vec->getSpot(this->pos - 1);
      for (int failures = 0; this->child.load() == nullptr;) {
        if (failures++ >= LIMIT) {
          PopSubDescr<T>* e1 = nullptr;
          this->child.compare_exchange_strong(
              e1, reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed));
        } else {
          T* expected = spot.load();
          if (expected == reinterpret_cast<T*>(NotValue)) {
            helper_cas(
                this->child, static_cast<PopSubDescr<T>*>(nullptr),
                reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed));
          } else if (vec->is_descr(expected)) {
            vec->unpack_descr(expected)->complete();
          } else {
            auto psh = new PopSubDescr<T>(this, expected);
            auto packed = vec->pack_descr(psh);
            if (spot.compare_exchange_strong(expected, packed)) {
              helper_cas(this->child, static_cast<decltype(psh)>(nullptr), psh);
              if (this->child.load() == psh) {
                spot.compare_exchange_strong(packed,
                                             reinterpret_cast<T*>(NotValue));
              } else {
                helper_cas(spot, reinterpret_cast<T*>(this), expected);
              }
            }
          }
        }
      }

      auto e1 = reinterpret_cast<T*>(this);
      this->vec->getSpot(this->pos).compare_exchange_strong(
          e1, reinterpret_cast<T*>(NotValue));

      return this->child.load() !=
             reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed);
    }
  };

  template <typename T>
  struct PopSubDescr : public base_descriptor {
    PopDescr<T>* parent;
    T* value;

    PopSubDescr(PopDescr<T>* parent, T* value) : parent(parent), value(value) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::POP_SUB_DESCR;
    }

    bool complete(void) override {
      std::atomic<T*>& spot = this->parent->vec->getSpot(this->parent->pos - 1);
      helper_cas(this->parent->child, static_cast<PopSubDescr<T>*>(nullptr),
                 this);
      if (this->parent->child.load() == this) {
        helper_cas(spot, this->parent->vec->pack_descr(this),
                   reinterpret_cast<T*>(NotValue));
      } else {
        helper_cas(spot, this->parent->vec->pack_descr(this), this->value);
      }

      return this->parent->child.load() == this;
    }
  };

  template <typename T>
  struct PopOp : public base_op {
    vector<T>* vec;

    std::pair<bool, T*> result;

    PopOp(vector<T>* vec) : vec(vec) {
    }

    OpType type(void) const override {
      return OpType::POP_OP;
    }

    bool complete(void) override {
      // fadfdfad(); // TODO(c650) -- this
      return false;
    }
  };

  template <typename T>
  struct PushDescr : public base_descriptor {
    vector<T>* vec;
    T* value;
    std::size_t pos;
    std::atomic<DescriptorState> state;

    PushDescr(vector<T>* vec, T* value, std::size_t pos)
        : vec(vec), value(value), pos(pos), state(DescriptorState::Undecided) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::PUSH_DESCR;
    }

    bool complete(void) override {
      std::atomic<T*>& spot = this->vec->getSpot(this->pos);
      decltype(spot) spot2 = this->vec->getSpot(this->pos - 1);

      auto current = spot2.load();

      int failures = 0;

      while (this->state.load() == DescriptorState::Undecided &&
             vec->is_descr(current)) {
        if (failures++ >= LIMIT) {
          helper_cas(this->state, DescriptorState::Undecided,
                     DescriptorState::Failed);
        }

        vec->unpack_descr(current)->complete();
        current = spot2.load();
      }

      if (this->state.load() == DescriptorState::Undecided) {
        if (current == reinterpret_cast<T*>(NotValue)) {
          helper_cas(this->state, DescriptorState::Undecided,
                     DescriptorState::Failed);
        } else {
          helper_cas(this->state, DescriptorState::Undecided,
                     DescriptorState::Passed);
        }
      }

      if (this->state.load() == DescriptorState::Passed) {
        helper_cas(spot, reinterpret_cast<T*>(this), this->value);
      } else {
        helper_cas(spot, reinterpret_cast<T*>(this),
                   reinterpret_cast<T*>(NotValue));
      }

      return this->state.load() == DescriptorState::Passed;
    }
  };

  template <typename T>
  struct PushOp : public base_op {
    vector<T>* vec;

    std::size_t result;

    PushOp(vector<T>* vec) : vec(vec) {
    }

    OpType type(void) const override {
      return OpType::PUSH_OP;
    }

    bool complete(void) override {
      // fadffdafdfdfdfad(); // TODO(c650) -- this
      return false;
    }
  };

  struct DescriptorSet {};

  template <typename T>
  struct InternalStorage {};

  template <typename T>
  struct Contiguous : private InternalStorage<T> {
    vector<T>* vec;
    Contiguous* old;
    const std::size_t capacity;

    std::atomic<T*>* array;

    Contiguous(vector<T>* vec, Contiguous* old, std::size_t capacity)
        : vec(vec),
          old(old),
          capacity(capacity),
          array(new std::atomic<T*>[capacity]) {
      // reinterpret_cast is the C++ analog of summoning Satan
      const std::size_t prefix = old == nullptr ? 0 : old->capacity;
      std::fill(this->array, this->array + prefix,
                reinterpret_cast<T*>(NotCopied));
      std::fill(this->array + prefix, this->array + this->capacity,
                reinterpret_cast<T*>(NotValue));
    }

    ~Contiguous(void) {
      delete old;
      delete[] array;
    }

    Contiguous<T>* resize(void) {
      Contiguous<T>* vnew =
          new Contiguous(this->vec, this, this->capacity * 2 + 1);

      auto expected = this;
      if (this->vec->storage.compare_exchange_strong(expected, vnew)) {
        for (std::size_t i = 0; i < this->capacity; ++i) {
          vnew->copyValue(i);
        }
      }

      return this->vec->storage.load();
    }

    void copyValue(const std::size_t pos) {
      if (this->old &&
          this->old->array[pos] == reinterpret_cast<T*>(NotCopied)) {
        this->old->copyValue(pos);
      }

      // atomic mark resize bit

      atomicMarkResizeBit(this->old->array[pos]);

      const auto v =
          reinterpret_cast<std::size_t>(this->old->array[pos].load()) &
          ~(BitMarkings::Resize);

      helper_cas(this->array[pos], reinterpret_cast<T*>(NotCopied),
                 reinterpret_cast<T*>(v));
    }

    std::atomic<T*>& getSpot(const std::size_t pos) {
      if (pos >= this->capacity) {
        return this->resize()->getSpot(pos);
      }

      if (this->array[pos] == reinterpret_cast<T*>(NotCopied)) {
        this->copyValue(pos);
      }

      return this->array[pos];
    }

    static void atomicMarkResizeBit(std::atomic<T*>& spot) {
      // spot |= reinterpret_cast<T*>(BitMarkings::Resize);
      for (;;) {
        std::size_t val =
            reinterpret_cast<std::size_t>(spot.load()) | BitMarkings::Resize;
        if (helper_cas(spot, spot.load(), reinterpret_cast<T*>(val))) {
          return;
        }
      }
    }
  };

  template <typename T>
  struct vector {
    std::atomic<Contiguous<T>*> storage;
    std::atomic<std::size_t> size;

    // For maintaining efficient number of descriptors.
    // Each thread gets 2 of each descriptor type per
    // instance of vector<T>.
    // https://stackoverflow.com/a/41801400
    // static thread_local std::map<vector<T>*, DescriptorSet> descs;

    vector(void) : vector(0) {
    }

    vector(std::size_t capacity)
        : storage(new Contiguous<T>(this, nullptr, capacity)), size(0) {
    }

    // returns whether successful and if successful returns ptr to element
    std::pair<bool, T*> wf_popback(void) {
      help_if_needed();

      auto pos = this->size.load();
      for (int failures = 0; failures <= LIMIT; ++failures) {
        if (pos == 0) {
          return std::make_pair(false, nullptr);
        }

        std::atomic<T*>& spot = this->storage.load()->getSpot(pos);
        T* expected = spot.load();
        if (expected == reinterpret_cast<T*>(NotValue)) {
          auto ph = new PopDescr<T>(this, pos);
          if (spot.compare_exchange_strong(expected, pack_descr(ph))) {
            auto res = ph->complete();
            if (res) {
              auto value = ph->child.load()->value;
              this->size -= 1;
              return std::make_pair(true, value);
            } else {
              --pos;
            }
          }
        } else if (is_descr(expected)) {
          unpack_descr(expected)->complete();
        } else {
          ++pos;
        }
      }

      auto pop_op = new PopOp<T>(this);

      announceOp(pop_op);

      return pop_op->result;
    }

    std::size_t wf_push_back(T* const value) {
      help_if_needed();

      if (value == nullptr) {
        throw std::runtime_error("cannot push_back nullptr!!");
      }

      auto pos = this->size.load();
      for (int failures = 0; failures <= LIMIT; ++failures) {
        std::atomic<T*>& spot = this->getSpot(pos);
        auto expected = spot.load();
        if (expected == reinterpret_cast<T*>(NotValue)) {
          if (pos == 0) {
            if (helper_cas(spot, expected, value)) {
              this->size += 1;
              return 0;
            } else {
              pos++;
              continue; // not reassigning spot cause references dont like it
            }
          }

          auto ph = new PushDescr<T>(this, value, pos);
          if (helper_cas(spot, expected, reinterpret_cast<T*>(ph))) {
            auto res = ph->complete();
            if (res) {
              this->size += 1;
              return pos - 1; // hmmm ???
            } else {
              --pos;
            }
          }

        } else if (is_descr(expected)) {
          this->unpack_descr(expected)->complete();
        } else {
          ++pos;
        }
      }

      auto push_op = new PushOp<T>(this);

      announceOp(push_op);

      return push_op->result;
    }

    // helpers

    T* pack_descr(base_descriptor* desc) {
      std::size_t x = reinterpret_cast<std::size_t>(desc);

      if ((x & 0b11) != 0 || x == 0) {
        throw std::runtime_error("bad argument passed to pack_descr");
      }

      return reinterpret_cast<T*>(x | BitMarkings::IsDescriptor);
    }

    base_descriptor* unpack_descr(T* desc) {
      std::size_t a = 0b11;
      std::size_t b = reinterpret_cast<std::size_t>(desc);
      return reinterpret_cast<base_descriptor*>(b & ~a);
    }

    // returns true IFF desc is bit-marked as a descriptor AND
    // desc cannot just be the bitmarking (i.e., must not be null |
    // IsDescriptor)
    bool is_descr(T* desc) {
      std::size_t x = reinterpret_cast<std::size_t>(desc);
      return (x & 0b11) == BitMarkings::IsDescriptor &&
             (x != BitMarkings::IsDescriptor);
    }

    void help_if_needed(void) {
      // fadfdfdfad(); // TODO (c650) -- this.
    }

    void announceOp(base_op* op) {
    }

    std::atomic<T*>& getSpot(std::size_t pos) {
      return this->storage.load()->getSpot(pos);
    }
  };
}; // namespace waitfree
