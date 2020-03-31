# waitfree-vector

- [Programming Assignment 1 Writeup](##Programming-Assignment-1-Writeup)
- [Programming Assignment 2 Writeup](##Programming-Assignment-2-Writeup)

Development was done using Ubuntu 16.04 and 18.04 on 64 bit Intel machines.

## Dependencies:

### MRLock

MRLock is included as a submodule. It requires libboost-dev, libboost-random-dev, and libtbb-dev (use `make get_deps`).

After installing the dependencies, to get the mrlock library code, run `git submodule update --init --recursive` in the root of the repo.

Follow the instructions in the [README](/mrlock/README) to build that code. Then you can build the targets of our project.

## Programming Assignment 1 Writeup

Our data structure is a vector. It has O(1) random access, amortised O(1) tail operations (push/pop), and O(n) arbitrary-index insertion and deletion. The elements are stored **contiguously** in memory.

The sequential version is in [src/sequential](src/sequential); the thread-safe blocking version is in [src/mrlock](src/mrlock).

Build the sequential with `make sequential` and the mrlock one with `make mrlock`. Binaries go into [bin/](/bin/).

### Design

Due to our need to maintain the contiguity property of the vector, at this time we have not found a better locking design than simply locking the entire vector. This is because some operations affect the _entire_ structure.

We are aware that much of the academic work in concurrent vectors abandons the contiguity requirement in order to achieve finer-grained locking or even lock-freedom [1, 4, 6]; we are also aware that Intel's TBB does the same. However, because our model paper's [2] wait-free vector boasts contiguity, we want to have all of our implementations be contiguous as well.

It is our commitment to the contiguity property that forces us to have to lock the entire structure for every operation. We use MRLock [5] for mutual exclusion; however, clearly we do not make use of the "multi-resource" aspect, but at least MRLock is starvation-free.

---

### API

Our implementation supports the following operations:

- at(pos)
  - returns the value at the _pos_'th position.
- operator[pos]
  - returns `at(pos)`
- insert(pos, val)
  - inserts the value `val` at the _pos_'th position, shifting to the right, everything previously at or beyond the _pos_'th position.
- erase(pos)
  - the opposite of insert. Removes the _pos_'th element and shifts to the left.
- push_back(val)
  - adds `val` to the end of the array.
- pop_back()
  - throws if the vector is empty.
  - otherwise, it removes the last value of the array.
- clear()
  - sets the logical size of the vector to zero.
- size()
  - return sthe logical size of the vector.
- capacity()
  - returns the actual size of the underlying array used to store the vector's elements.

All operations do bounds checking and throw accordingly.

---

### Implementation

Any operation that grows the vector (like push or insert) must first check that the vector has enough capacity. If the vector is full (size == capacity), then it is grown using the simple scheme `new_capacity = capacity * 2 + 1`. All elements are then copied over into a new internal array of size `new_capacity`.

Besides `pop_back()` (and the trivial `size()` and `capacity()`), all other methods may potentially affect the entire data structure (both `size` and `capacity` fields, and the underlying `data` array). So a global lock is needed.

---

## Programming Assignment 2 Writeup

This part of the project is our reimplementation of the wait-free vector described by Feldman et al. in [2].

Our wait-free vector supports many of the functions also available in the MRLock and Sequential implementations. The implementation is available in [src/concurrent/include/vector.hpp](/src/concurrent/include/vector.hpp). The following operations are supported:

- at(idx)
  - in O(1), retrieve what is stored at index `idx`
- wf_pushback(value)
  - in O(1), appends a value to the end of the vector
- wf_popback()
  - in O(1), removes the last element of the vector
- cwrite(idx, old, new)
  - in O(1), performs a CAS operation at index `idx` with `old` and `new`.
- insertAt(idx, value)
  - same as insertAt in the [previous section](###API)
- eraseAt(idx)
  - same as eraseAt in the [previous section](###API)

### Implementation Details

We used C++. We saw issues arise when using compiler optimisations, so we had to not use them. The make target `concurrent` builds our sample program.

#### Synchronisation Methods

We made heavy use of descriptors and atomic compare and swap operations. We used bit stealing to distinguish between descriptors and user data.

#### Key Algorithms

While some algorithms are made clear in the paper [2], others are not. And even after reading the supporting paper [3], we were still unclear about some things. We will cover these things.

Feldman et al. specifies the "fast path" of each vector operation, yet omits the slow path and the whole announcement structure, as well as the helping scheme, all taken from [3]. _Fast-path slow-path_ is a technique that combines a bounded lock-free approach and an unbounded wait-free approach; it achieves wait-freedom while still allowing each thread to first attempt a lock-free (generally faster than wait-free) approach some number of times. This greatly improves performance while still being wait-free.

The slow path implementations and helping scheme were left up to us. We implemented a simple helping scheme that requires that the number of threads accessing the shared object (the vector) is bounded. Each thread is assigned an integer id from `0` to `NUM_THREADS-1`. Before executing any operation, a thread will check if some other thread needs help completing an operation (a thread needs help when it has to resort to the slow path). To decide which thread to help, we simply increment a thread-local counter (mod `NUM_THREADS`) and then see if that thread needs help. If so, we help it.

The slow path is abstracted into descriptors called Ops. Each method (e.g., `cwrite`) has an associated Op (e.g., `WriteOp`). All Ops inherit from `base_op` and implement `base_op::complete()`. A challenge is that multiple threads may compete to complete the same operation; we took steps to avoid accidentally doing an operaton more than once. Namely, an atomic `result` field regulates whether the operation was already completed (each operation has some result) by another thread. The thread that announced the op is able to access `result` to finish its operation (e.g., complete the `cwrite()` call).

### Performance

We compared different combinations of operations between our wait-free implementation and the blocking (MRLock) implementation. We expect that our wait-free vector will have a higher throughput than the blocking vector.

## Citations

In our research, we have looked at a handful of concurrent vector papers. These may be cited down the road, so they are included in addition to works cited earlier in this document.

1. Dechev, Damian, Peter Pirkelbauer, and Bjarne Stroustrup. "Lock-free dynamically resizable arrays." International Conference On Principles Of Distributed Systems. Springer, Berlin, Heidelberg, 2006.
1. Feldman, Steven, Carlos Valera-Leon, and Damian Dechev. "An efficient wait-free vector." IEEE Transactions on Parallel and Distributed Systems 27.3 (2015): 654-667.
1. Kogan, Alex, and Erez Petrank. "A methodology for creating fast wait-free data structures." ACM SIGPLAN Notices 47.8 (2012): 141-150.
1. Walulya, Ivan, and Philippas Tsigas. "Scalable lock-free vector with combining." 2017 IEEE International Parallel and Distributed Processing Symposium (IPDPS). IEEE, 2017.
1. Zhang, Deli, Brendan Lynch, and Damian Dechev. "Fast and Scalable Queue-Based Resource Allocation Lock on Shared-Memory Multiprocessors." International Conference On Principles Of Distributed Systems. Springer, Cham, 2013.
1. Lamar, Kenneth, Christina Peterson, and Damian Dechev. "Lock-free transactional vector." Proceedings of the Eleventh International Workshop on Programming Models and Applications for Multicores and Manycores. 2020.
