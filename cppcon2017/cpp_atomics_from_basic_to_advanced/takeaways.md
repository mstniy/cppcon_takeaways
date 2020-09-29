# C++ atomics from basic to advanced

[View on Youtube](https://youtu.be/ZQFzMfHIxng)

1. #### Strong and Weak compare-and-swap
- If the hardware uses the double locking pattern, weak compare-and-swap may spuriously fail.
    - The core reads the value, gets the expected value, tries to get exclusive access to the cache line, times out.
    - This is also the reason cas has two memory orders.

2. #### Atomic Variables as Gateways to Memory Access :
- Need memory barriers (orders)
- `memory_order_relaxed`: Introduces no order guarantees. Used for variables that do not index any memory, like concurrent counters. Inhibits the least number of optimizations.
    - Note from [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order#Relaxed_ordering) : Increasing a reference counter can be relaxed. Decreasing must be `acq_rel`, because if the counter decreases to zero, we will want all writes done on the object by other threads until they destructed their references to be visible on the thread that will call the destructor before it invokes the destructor. Thus, the `acq_rel` fetch_sub on threads that release their non-last reference pairs with itself on the thread that invokes the destructor.
-  `memory_order_acquire`: Operations cannot be reordered from after to before the barrier. Can be used for loading indexes or pointers. Pair with stores using `memory_order_release`. Cannot be used for stores.
-  `memory_order_release`: The opposite. Can be used for storing indexes or pointers. Pair with loads using `memory_order_acquire`.
    - Note from [cppreference](https://en.cppreference.com/w/cpp/atomic/memory_order#Release-Acquire_ordering) : For the release-acquire ordering guarantees to be valid, the relevant threads must acquire and release **the same atomic variable**. `memory_order_seq_cst` removes this requirement.
-  `memory_order_acq_rel`: combines `acquire` and `release`. No operation can move across the barrier, but, again, only if both threads use the same atomic variable. Cannot be used for stores and loads.
-  `memory_order_seq_cst`: The most strict, and the default, barrier. On top of having both `acquire` and `release` semantics,  removes the "same atomic variable" requirement of `acq_rel` and establishes a single total modification order of all atomic operations **which are tagged with it**, visible to all threads, so long as there is no race condition.
- [GCC's wiki page on the subject](https://gcc.gnu.org/wiki/Atomic/GCCMM/AtomicSync) is also useful.
    - The part on mixing memory orders, however, seems to be under the impression that `memory_order_relaxed` does not "flush", which is false. This would violate atomicity. Memory orders only affect ordering guarantees. An atomic operation is atomic no matter the memory order it uses.

3. #### x86 specifics:
-  strong cas == weak cas
-  All loads are acquire loads
-  All stores are release stores
-  All read-modify-write operations (like exchange, fetch_add) are acquire-release
-  `memory_order_acq_rel` == `memory_order_seq_cst` where `acq_rel` is applicable