# atomic<> weapons

[View on Youtube](https://youtu.be/A8eCGOqgvH4)
[The second part](https://youtu.be/KeLBd2EJLOU)

#### Summary of the Talk

 - Don't write a race condition or use non-default atomics and your code will do what you think it will.
 
 #### Key Concepts
 *Timestamp: 9:20*
 
 - **Sequential Consistency**: Basically emulating a non-out-of-order, preemptive single core machine.
    - Default `std::atomic` operations (`memory_order_seq_cst`) provide sequential consistency, so long as there is no data race.
 - **Data race** : A memory location being simultaneously accessed by multiple threads, at least one of which is a writer.
     - `std::atomic`operations do not race
     - `happens-before` synchronized accesses cannot happen simultaneously thus do not race
     - Accesses that happen on the same thread do not race
     - Data races are **undefined behaviour**.
         - Data races break the optimizer's assumptions 
     - Memory location: an object of scalar type
     - Doesn't matter if the objects are on the same cache line or not.
     
 #### Transformations
 *Timestamp: 11:00*
     
- There are many layers of optimizations/transformations from the source code to the actual execution:
    - Compiler (various optimizations)
    - Processor (prefetching, speculation, OOO, ...)
    - Cache (store buffers, multi level caches, ...)
- We can reason about all transformations as reorderings of source code loads and stores, no matter at which level they happen.
- `std::atomic` memory orders affect all these levels.

#### Acq/Rel
*Timestamp: 43:20*

I am not sure what Herb is referring to as "plain acq/rel" and "sc. acq/rel". The standard has `memory_order_acquire`, `memory_order_release`  and `memory_order_seq_cst`, no seperate "sc. acquire" or "sc. release". I believe whenever he refers to "sc. acquire", he means a load with `memort_order_seq_cst` and a store with the same order by "sc. release".

#### Ordered Atomics
*Timestamp: 1:01:22*

- Java and .NET calls atomics variables `volatile`
- **NOT** the same as C/C++ `volatile`.
    - C/C++ `volatile` only affects compiler optimizations. The CPU/cache can still reorder (and potentially break) your code. `volatile` is not a substitute for `std::atomic`.
    
#### Fences
*Timestamp: 1:05:00*

- Avoid standalone fences. They are non-portable, error-prone and usually too heavy.
    - A pointer load/store can be acquire/release with `std::atomic` style memory orders, but requires a full memory barrier with standalone fences.
    
#### Limitations Imposed on the Optimizer
*Timestamp: 5:45 of the second part*

- The optimizer must not invent a write to a variable (or memory location) that would not be written to in a sequentially consistent execution, even if they don't affect the execution from a single-threaded point of view, unless it can prove that that variable is not shared.
    - This would make it impossible to correctly synchronize a program.
    - The compiler may have to inject padding, depending on the ISA.
    - Bit fields complicate things
    
#### Uses Cases for non-`seq_cst` atomics
*Timestamp: 1:09:10 of the second part*

- Encapsulate these non-default operations inside types.
- Concurrent counters: `relaxed` is enough. Of course, make sure to synchronize your threads before reading the final value of the counter.
- Setting flags: `relaxed` if the reader doesn't need to see the writes made to other memory locations by the thread that set the flag, `acquire` and `release` otherwise.
- Reference counting: Increments can be `relaxed`, decrements  `acq_rel` (acquire the object state before potentially destructing it).