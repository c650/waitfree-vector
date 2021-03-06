#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <utility>

namespace waitfree {

  const int LIMIT = 1000;
  const int NO_LIMIT = std::numeric_limits<int>::max();
  const std::size_t NO_TID = std::numeric_limits<std::size_t>::max();

  // vector type declaration
  template <typename T>
  struct vector;

  // enum types
  enum DescriptorType {
    PUSH_DESCR,
    POP_DESCR,
    POP_SUB_DESCR,
    WRITE_OP_DESCR,
    SHIFT_DESCR
  };
  enum DescriptorState { Undecided, Failed, Passed };
  enum OpType { POP_OP, PUSH_OP, WRITE_OP, SHIFT_OP };

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
  template <typename T>
  struct base_descriptor {
    virtual DescriptorType type(void) const = 0;
    virtual bool complete(std::size_t) = 0;
    virtual T* value(void) const = 0;
  };

  struct base_op {
    std::atomic_bool done;
    virtual OpType type(void) const = 0;
    virtual bool complete(std::size_t) = 0;
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
  struct PopDescr : public base_descriptor<T> {
    vector<T>* vec;
    std::size_t pos;
    std::atomic<PopSubDescr<T>*> child;

    base_op* owner;

    PopDescr(vector<T>* vec, std::size_t pos)
        : vec(vec), pos(pos), child(nullptr), owner(nullptr) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::POP_DESCR;
    }

    bool complete(std::size_t tid) override {
      std::atomic<T*>& spot = this->vec->getSpot(this->pos - 1);
      for (int failures = 0; this->child.load() == nullptr;) {
        if (failures++ >= LIMIT) {
          helper_cas(
              this->child, static_cast<PopSubDescr<T>*>(nullptr),
              reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed));

          break;
        }

        T* expected = spot.load();
        if (expected == reinterpret_cast<T*>(NotValue)) {
          helper_cas(
              this->child, static_cast<PopSubDescr<T>*>(nullptr),
              reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed));
        } else if (this->vec->is_descr(expected)) {
          this->vec->unpack_descr(expected)->complete(tid);
        } else {
          auto psh = new PopSubDescr<T>(this, expected);
          auto packed = this->vec->pack_descr(psh);
          if (spot.compare_exchange_strong(expected, packed)) {
            helper_cas(this->child, static_cast<decltype(psh)>(nullptr), psh);
            if (this->child.load() == psh) {
              spot.compare_exchange_strong(packed,
                                           reinterpret_cast<T*>(NotValue));
            } else {
              helper_cas(spot, this->vec->pack_descr(this),
                         expected); // ***
            }
          }
        }
      }

      auto e1 = this->vec->pack_descr(this);
      this->vec->getSpot(this->pos).compare_exchange_strong(
          e1, reinterpret_cast<T*>(NotValue));

      return this->child.load() !=
             reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed);
    }

    T* value(void) const override {
      return reinterpret_cast<T*>(NotValue);
    }
  };

  template <typename T>
  struct PopSubDescr : public base_descriptor<T> {
    PopDescr<T>* parent;
    T* val;

    PopSubDescr(PopDescr<T>* parent, T* val) : parent(parent), val(val) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::POP_SUB_DESCR;
    }

    bool complete(std::size_t tid) override {
      std::atomic<T*>& spot = this->parent->vec->getSpot(this->parent->pos - 1);
      helper_cas(this->parent->child, static_cast<PopSubDescr<T>*>(nullptr),
                 this);
      if (this->parent->child.load() == this) {
        helper_cas(spot, this->parent->vec->pack_descr(this),
                   reinterpret_cast<T*>(NotValue));
      } else {
        helper_cas(spot, this->parent->vec->pack_descr(this), this->val);
      }

      return this->parent->child.load() == this;
    }

    T* value(void) const override {
      return this->val;
    }
  };

  template <typename T>
  struct PopOp : public base_op {
    vector<T>* vec;

    alignas(16) std::atomic<std::pair<bool, T*>*> result;

    PopOp(vector<T>* vec) : vec(vec), result(nullptr) {
    }

    OpType type(void) const override {
      return OpType::POP_OP;
    }

    bool complete(std::size_t tid) override {
      while (this->result.load() == nullptr) {
        auto pos = this->vec->_size.load();
        if (pos == 0) {
          this->result.store(new std::pair<bool, T*>{});
          continue;
        }

        std::atomic<T*>& spot = this->vec->getSpot(pos);
        auto expected{spot.load()};

        if (this->vec->is_descr(expected)) {
          this->vec->unpack_descr(expected)->complete(tid);
          continue;
        }

        if (expected != reinterpret_cast<T*>(NotValue)) {
          ++pos;
          continue;
        }

        PopDescr<T>* ph = new PopDescr<T>(this->vec, pos);
        ph->owner = this;

        if (helper_cas(spot, expected, this->vec->pack_descr(ph))) {
          auto res = ph->complete(tid);
          if (res) {
            assert(helper_cas(
                this->result, static_cast<std::pair<bool, T*>*>(nullptr),
                new std::pair<bool, T*>(true, ph->child.load()->val)));
            this->vec->_size -= 1;
          } else {
            --pos;
          }
        }

        // std::this_thread::sleep_for(
        //     std::chrono::milliseconds(this->vec->_num_threads));
      }

      // std::cerr << this->result.load() << std::endl;
      assert(this->result.load());

      return true;
    }
  };

  template <typename T>
  struct PushDescr : public base_descriptor<T> {
    vector<T>* vec;
    T* val;
    std::size_t pos;
    std::atomic<DescriptorState> state;

    base_op* owner;

    PushDescr(vector<T>* vec, T* val, std::size_t pos)
        : vec(vec),
          val(val),
          pos(pos),
          state(DescriptorState::Undecided),
          owner(nullptr) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::PUSH_DESCR;
    }

    bool complete(std::size_t tid) override {
      std::atomic<T*>& spot = this->vec->getSpot(this->pos);

      if (this->pos == 0) {
        helper_cas(this->state, DescriptorState::Undecided,
                   DescriptorState::Passed);
        if (this->owner) {
          helper_cas(this->owner->done, false, true);
        }

        helper_cas(spot, this->vec->pack_descr(this), this->val);

        return true;
      }

      decltype(spot) spot2 = this->vec->getSpot(this->pos - 1);

      auto current = spot2.load();

      int failures = 0;

      while (this->state.load() == DescriptorState::Undecided &&
             vec->is_descr(current)) {
        if (failures++ >= LIMIT) {
          helper_cas(this->state, DescriptorState::Undecided,
                     DescriptorState::Failed);
        }

        vec->unpack_descr(current)->complete(tid);
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
        if (this->owner) {
          helper_cas(this->owner->done, false, true);
        }
        helper_cas(spot, this->vec->pack_descr(this), this->val);
      } else {
        helper_cas(spot, this->vec->pack_descr(this),
                   reinterpret_cast<T*>(NotValue));
      }

      return this->state.load() == DescriptorState::Passed;
    }

    T* value(void) const override {
      return this->val;
    }
  };

  template <typename T>
  struct PushOp : public base_op {
    vector<T>* vec;
    T* const value;

    std::atomic<std::size_t> result;

    std::atomic_bool can_return;

    PushOp(vector<T>* vec, T* const value)
        : vec(vec), value(value), result(-1), can_return(false) {
    }

    OpType type(void) const override {
      return OpType::PUSH_OP;
    }

    bool complete(std::size_t tid) override {
      // just like pushback

      // do the pushback loop,
      // but only while we aren't done

      // but if we have to place a descriptor,
      // we try placing descriptor, but make sure that
      // some other thread hasnt yet

      // have a thread CAS in a descriptor
      // some PushOp::state will be Pending
      // and then the threads try to CAS a descriptor
      // the winner gets to update PushOp::state to DescriptorPlaced
      // then all threads call descriptor's complete(tid)
      // if successful, we are done! (this->done |= complete(tid))
      // otherwise, repeat loop

      auto pos = this->vec->_size.load();
      while (!this->done.load()) {
        std::atomic<T*>& spot = this->vec->getSpot(pos);
        auto expected = spot.load();

        if (this->vec->is_descr(expected)) {
          this->vec->unpack_descr(expected)->complete(tid);
          continue;
        }

        if (expected != reinterpret_cast<T*>(NotValue)) {
          ++pos;
          continue;
        }

        PushDescr<T>* pd = new PushDescr<T>(this->vec, this->value, pos);
        pd->owner = this;

        if (helper_cas(spot, expected, this->vec->pack_descr(pd))) {
          auto res = pd->complete(tid);
          if (res) {
            this->result.store(pos);
            this->vec->_size += 1;
            this->done.store(true);
            this->can_return.store(true);
          } else {
            if (pos == 0) {
              ++pos;
            } else {
              --pos;
            }
          }
        }
      }

      while (!this->can_return.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
      }

      return true;
    }
  };

  template <typename T>
  struct WriteOp : public base_op {
    struct WriteOpDesc : public base_descriptor<T> {
      WriteOp* const _owner;
      vector<T>* const _vec;
      T* const _val;

      WriteOpDesc(WriteOp* const owner, vector<T>* const vec, T* const val)
          : _owner(owner), _vec(vec), _val(val) {
      }

      DescriptorType type(void) const override {
        return DescriptorType::WRITE_OP_DESCR;
      }

      bool complete(std::size_t tid) override {
        auto& ref = this->_vec->getSpot(this->_owner->pos);

        helper_cas(this->_owner->result,
                   static_cast<std::pair<bool, T*>*>(nullptr),
                   new std::pair<bool, T*>(true, this->_owner->old));

        helper_cas(ref, this->_vec->pack_descr(this), this->_owner->noo);
        return true;
      }

      T* value(void) const override {
        return this->_val;
      }
    };

    vector<T>* _vec;
    std::size_t pos;
    T* old;
    T* noo;

    alignas(16) std::atomic<std::pair<bool, T*>*> result;

    WriteOp(vector<T>* vec, std::size_t pos, T* old, T* noo)
        : _vec(vec), pos(pos), old(old), noo(noo), result(nullptr) {
    }

    OpType type(void) const override {
      return OpType::WRITE_OP;
    }

    bool complete(std::size_t tid) override {
      while (this->result.load() == nullptr) {
        auto& ref = this->_vec->getSpot(this->pos);

        auto val = ref.load();

        if (_vec->is_descr(val)) {
          _vec->unpack_descr(val)->complete(tid);
          continue;
        }

        if (val != this->old) {
          helper_cas(this->result, static_cast<std::pair<bool, T*>*>(nullptr),
                     new std::pair<bool, T*>(false, val));
          return true;
        }

        WriteOpDesc* d = new WriteOpDesc(this, _vec, this->noo);

        if (helper_cas(ref, val, this->_vec->pack_descr(d))) {
          d->complete(tid);
          return true;
        }
      }

      return true;
    }
  };

  template <typename T>
  struct ShiftOp;

  template <typename T>
  struct ShiftDescr : public base_descriptor<T> {
    ShiftOp<T>* op;
    std::size_t pos;
    T* val;
    ShiftDescr<T>* prev;
    std::atomic<ShiftDescr<T>*> next;

    ShiftDescr(ShiftOp<T>* op, ShiftDescr<T>* prev, T* val, std::size_t pos)
        : op(op), pos(pos), val(val), prev(prev), next(nullptr) {
    }

    DescriptorType type(void) const override {
      return DescriptorType::SHIFT_DESCR;
    }

    bool complete(std::size_t tid) override {
      bool isAssoc = false;

      if (this->prev == nullptr) {
        helper_cas(this->op->next, static_cast<ShiftDescr<T>*>(nullptr), this);
        isAssoc = this->op->next.load() == this;
      } else {
        helper_cas(this->prev->next, static_cast<ShiftDescr<T>*>(nullptr),
                   this);
        isAssoc = this->prev->next.load() == this;
      }

      std::atomic<T*>& spot = this->op->vec->getSpot(this->pos);
      auto packed_desc = this->op->vec->pack_descr(this);
      if (isAssoc) {
        this->op->complete(tid);
        helper_cas(spot, packed_desc, this->op->valueGetter(this));
      } else {
        helper_cas(spot, packed_desc, this->val);
      }
      return true;
    }

    T* value(void) const override {
      return val;
    }
  };

  template <typename T>
  struct ShiftOp : public base_op {
    vector<T>* vec;
    std::size_t pos;
    std::atomic<bool> incomplete;
    std::atomic<ShiftDescr<T>*> next;
    std::function<T*(ShiftDescr<T>*)> valueGetter;

    ShiftOp(vector<T>* vec, std::size_t pos,
            std::function<T*(ShiftDescr<T>*)> valueGetter)
        : vec(vec),
          pos(pos),
          incomplete(true),
          next(nullptr),
          valueGetter(valueGetter) {
    }

    OpType type(void) const override {
      return OpType::SHIFT_OP;
    }

    void clean(void) {
      auto sh = this->next.load();
      for (auto tpos = this->pos; sh != nullptr; tpos++) {
        auto packed_sh = this->vec->pack_descr(sh);
        helper_cas(this->vec->getSpot(tpos), packed_sh, valueGetter(sh));
        sh = sh->next.load();
      }
    }

    bool complete(std::size_t tid) override {
      auto i = this->pos;
      if (i >= this->vec->size()) {
        helper_cas(this->next, static_cast<ShiftDescr<T>*>(nullptr),
                   reinterpret_cast<ShiftDescr<T>*>(DescriptorState::Failed));
      }

      for (int failures = 0; this->next.load() == nullptr;) {
        if (failures++ >= LIMIT) {
          this->vec->announceOp(tid, this);
          return false;
        }
        std::atomic<T*>& spot = this->vec->getSpot(i);
        T* cvalue = spot.load();
        if (this->vec->is_descr(cvalue)) {
          this->vec->unpack_descr(cvalue)->complete(tid);
        } else if (cvalue == reinterpret_cast<T*>(NotValue)) {
          helper_cas(this->next, static_cast<ShiftDescr<T>*>(nullptr),
                     reinterpret_cast<ShiftDescr<T>*>(DescriptorState::Failed));
        } else {
          auto sh = new ShiftDescr<T>(this, nullptr, cvalue, i);
          auto packed_sh = this->vec->pack_descr(sh);
          if (spot.compare_exchange_strong(cvalue, packed_sh)) {
            helper_cas(this->next, static_cast<decltype(sh)>(nullptr), sh);
            if (sh != this->next.load()) {
              helper_cas(spot, packed_sh, cvalue);
            }
          }
        }
      }

      auto last = this->next.load();
      if (last == reinterpret_cast<ShiftDescr<T>*>(DescriptorState::Failed)) {
        return false;
      }

      while (this->incomplete.load()) {
        i++;
        if (last->value() == nullptr) {
          this->incomplete.store(false);
          break;
        }
        for (int failures = 0; last->next.load() == nullptr;) {
          if (failures++ >= LIMIT) {
            this->vec->announceOp(tid, this);
            return false;
          }
          std::atomic<T*>& spot = this->vec->getSpot(i);
          T* cvalue = spot.load();
          if (this->vec->is_descr(cvalue)) {
            auto desc = this->vec->unpack_descr(cvalue);
            if (desc->type() == DescriptorType::PUSH_DESCR) {
              auto cdesc = reinterpret_cast<PushDescr<T>*>(desc);
              helper_cas(cdesc->state, DescriptorState::Undecided,
                         DescriptorState::Passed);
            } else if (desc->type() == DescriptorType::POP_DESCR) {
              auto cdesc = reinterpret_cast<PopDescr<T>*>(desc);
              helper_cas(
                  cdesc->child, static_cast<PopSubDescr<T>*>(nullptr),
                  reinterpret_cast<PopSubDescr<T>*>(DescriptorState::Failed));
            }
            desc->complete(tid);
          } else {
            auto sh = new ShiftDescr<T>(this, last, cvalue, i);
            auto packed_sh = this->vec->pack_descr(sh);
            if (spot.compare_exchange_strong(cvalue, packed_sh)) {
              helper_cas(last->next, static_cast<decltype(sh)>(nullptr), sh);
              if (sh != last->next.load()) {
                helper_cas(spot, packed_sh, cvalue);
              }
            }
          }
        }
        last = last->next.load();
      }
      return true;
    }
  };

  template <typename T>
  struct Contiguous {
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
      if (this->vec->_storage.compare_exchange_strong(expected, vnew)) {
        for (std::size_t i = 0; i < this->capacity; ++i) {
          vnew->copyValue(i);
        }
      }

      return this->vec->_storage.load();
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
    const std::size_t _num_threads;
    std::vector<std::atomic<base_op*>> _thread_ops;
    std::vector<std::size_t> _thread_to_help;

    std::atomic<Contiguous<T>*> _storage;
    std::atomic<std::size_t> _size;

    // For maintaining efficient number of descriptors.
    // Each thread gets 2 of each descriptor type per
    // instance of vector<T>.
    // https://stackoverflow.com/a/41801400
    // static thread_local std::map<vector<T>*, DescriptorSet> descs;

    vector(std::size_t num_threads) : vector(num_threads, 0) {
    }

    vector(std::size_t num_threads, std::size_t capacity)
        : _num_threads(num_threads),
          _thread_ops(_num_threads),
          _thread_to_help(_num_threads),
          _storage(new Contiguous<T>(this, nullptr, capacity)),
          _size(0) {
      static_assert(sizeof(T) >= 4,
                    "underlying type must be at least 4 bytes so that last 2 "
                    "bits of address are available");
    }

    // returns whether successful and if successful returns ptr to element
    std::pair<bool, T*> wf_popback(const std::size_t tid) {
      this->help_if_needed(tid);

      auto pos = this->_size.load();
      for (int failures = 0; failures <= LIMIT; ++failures) {
        if (pos == 0) {
          return std::make_pair(false, nullptr);
        }

        std::atomic<T*>& spot = this->getSpot(pos);
        T* expected = spot.load();
        if (expected == reinterpret_cast<T*>(NotValue)) {
          auto ph = new PopDescr<T>(this, pos);
          if (spot.compare_exchange_strong(expected, pack_descr(ph))) {
            auto res = ph->complete(tid);
            if (res) {
              auto value = ph->child.load()->val;
              this->_size -= 1;
              return std::make_pair(true, value);
            } else {
              --pos;
            }
          }
        } else if (is_descr(expected)) {
          unpack_descr(expected)->complete(tid);
        } else {
          ++pos;
        }
      }

      assert(tid != NO_TID);

      PopOp<T>* __po = new PopOp<T>(this);

      this->announceOp(tid, __po);

      return *(__po->result.load());
    }

    std::size_t wf_push_back(const std::size_t tid, T* const value) {
      this->help_if_needed(tid);

      if (value == nullptr) {
        throw std::runtime_error("cannot push_back nullptr!!");
      }

      auto pos = this->_size.load();
      for (int failures = 0; failures <= LIMIT; ++failures) {
        std::atomic<T*>& spot = this->getSpot(pos);
        auto expected = spot.load();
        if (expected == reinterpret_cast<T*>(NotValue)) {
          if (pos == 0) {
            if (helper_cas(spot, expected, value)) {
              this->_size += 1;
              return 0;
            } else {
              pos++;
              continue; // not reassigning spot cause references dont like it
            }
          }

          auto ph = new PushDescr<T>(this, value, pos);
          if (helper_cas(spot, expected, this->pack_descr(ph))) {
            auto res = ph->complete(tid);
            if (res) {
              this->_size += 1;
              return pos;
            } else {
              --pos;
            }
          }

        } else if (is_descr(expected)) {
          this->unpack_descr(expected)->complete(tid);
        } else {
          ++pos;
        }
      }

      assert(tid != NO_TID);

      PushOp<T>* __po = new PushOp<T>(this, value);

      announceOp(tid, __po);

      return __po->result.load();
    }

    std::pair<bool, T*> at(const std::size_t tid, std::size_t pos) {
      this->help_if_needed(tid);

      if (pos < this->_size.load()) { // should be this, not whats in paper
        auto value = this->getSpot(pos).load();
        if (this->is_descr(value)) {
          value = this->unpack_descr(value)->value();
        }
        if (value != reinterpret_cast<T*>(NotValue)) {
          return std::make_pair(true, value);
        }
      }
      return std::make_pair(false, nullptr);
    }

    bool insertAt(std::size_t tid, std::size_t pos, T* const val) {
      this->help_if_needed(tid);

      std::function<T*(ShiftDescr<T>*)> valueGetter =
          [val](ShiftDescr<T>* sh) -> T* {
        if (sh->prev == nullptr) {
          return val;
        } else {
          return sh->prev->val;
        }
      };
      auto op = new ShiftOp<T>(this, pos, valueGetter);
      op->complete(tid);
      if (!(op->incomplete.load())) {
        op->clean();
        this->_size.fetch_add(1);
        return true;
      } else {
        return false;
      }
    }

    bool eraseAt(std::size_t tid, std::size_t pos) {
      this->help_if_needed(tid);

      std::function<T*(ShiftDescr<T>*)> valueGetter =
          [](ShiftDescr<T>* sh) -> T* {
        if (sh->next.load() == nullptr) {
          return nullptr;
        } else {
          return sh->next.load()->value();
        }
      };
      auto op = new ShiftOp<T>(this, pos, valueGetter);
      bool succ = op->complete(tid);
      if (succ)
        ;
      if (!(op->incomplete.load())) {
        op->clean();
        this->_size.fetch_add(-1);
        return true;
      } else {
        return false;
      }
    }

    std::pair<bool, T*> cwrite(const std::size_t tid, std::size_t pos, T* old,
                               T* noo) {
      this->help_if_needed(tid);

      if (noo == nullptr) {
        return std::make_pair(false, nullptr);
      }

      if (pos >= this->_size.load()) {
        return std::make_pair(false, nullptr);
      }

      std::atomic<T*>& spot = this->getSpot(pos);
      for (int failures = 0; failures <= LIMIT; ++failures) {
        auto value = spot.load();
        if (this->is_descr(value)) {
          this->unpack_descr(value)->complete(tid);
        } else if (value == old) {
          if (helper_cas(spot, value, noo)) {
            return std::make_pair(true, old);
          } else {
            return std::make_pair(false, value);
          }
        }
      }

      assert(tid != NO_TID);

      WriteOp<T>* __wo = new WriteOp<T>(this, pos, old, noo);

      announceOp(tid, __wo);

      return *(__wo->result);
    }

    std::size_t size(void) const {
      return this->_size.load();
    }

    // helpers

    T* pack_descr(base_descriptor<T>* desc) {
      std::size_t x = reinterpret_cast<std::size_t>(desc);

      if ((x & 0b11) != 0 || x == 0) {
        throw std::runtime_error("bad argument passed to pack_descr");
      }

      return reinterpret_cast<T*>(x | BitMarkings::IsDescriptor);
    }

    base_descriptor<T>* unpack_descr(T* desc) {
      std::size_t a = 0b11;
      std::size_t b = reinterpret_cast<std::size_t>(desc);
      return reinterpret_cast<base_descriptor<T>*>(b & ~a);
    }

    // returns true IFF desc is bit-marked as a descriptor AND
    // desc cannot just be the bitmarking (i.e., must not be null |
    // IsDescriptor)
    bool is_descr(T* desc) {
      std::size_t x = reinterpret_cast<std::size_t>(desc);
      return (x & 0b11) == BitMarkings::IsDescriptor &&
             (x != BitMarkings::IsDescriptor);
    }

    void help(const std::size_t my_tid, const std::size_t tid) {
      // for (;;) {
      auto t_op = std::atomic_load(&(this->_thread_ops[tid]));

      if (t_op == nullptr) {
        return;
      }

      t_op->complete(my_tid);

      std::atomic_compare_exchange_strong(
          &(this->_thread_ops[this->_thread_to_help[tid]]), &t_op,
          decltype(t_op){nullptr});
      // }
    }

    void help_if_needed(const std::size_t tid) {
      if (tid >= this->_num_threads) {
        throw std::runtime_error{"tid out of bounds"};
      }

      this->_thread_to_help[tid] =
          (this->_thread_to_help[tid] + 1) % this->_num_threads;

      help(tid, this->_thread_to_help[tid]);
    }

    void announceOp(const std::size_t tid, base_op* op) {
      if (tid >= this->_num_threads) {
        throw std::runtime_error{"tid out of bounds"};
      }

      auto cur = std::atomic_load(&(this->_thread_ops[tid]));

      // if (cur != nullptr) {
      //   throw std::runtime_error{"tid has op already"};
      // }

      std::atomic_compare_exchange_strong(&(this->_thread_ops[tid]), &cur, op);

      help(tid, tid);
    }

    std::atomic<T*>& getSpot(std::size_t pos) {
      return this->_storage.load()->getSpot(pos);
    }
  };
}; // namespace waitfree
