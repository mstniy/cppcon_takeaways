# A Unifying Abstraction for Async in C++

[View on Youtube](https://youtu.be/tF-Nz4aRWAM)

1. #### Concurrency vs. Parallelism
*Timestamp: 3:50*

They're in many ways opposites.

Concurrency:
- Unknown inter-task dependencies
- The scheduler must guarantee forward progress without knowing inter-task dependencies
- A lossy abstraction for inter-task dependencies

Parallelism:
- Programmer guarantees that there are no inter-task dependencies
- Grants a high degree of freedom to the scheduler
- No forward-progress guarantee by the scheduler
    - Even if task A blocks on task B, the scheduler is allowed to keep scheduling task A forever.

Serial:
- Stronger guarantees than parallelism (No data races)
- Weaker guarantees than concurrency (No forward-progress guarantee)

When you use concurrent features (like `std::thread`) to express parallelism, you end up with unnecessary guarantees and unreasonable overheads.

Use C++17 parallel algorithms instead of threads or thread pools whenever possible.

2. #### Why are standard futures slow?
*Timestamp : 12.45*

- A `promise` - `future` pair refers to a heap-allocated, reference-counted and synchronized control block.
- (In the case of `boost::future`) the continuation is type-erased (= an indirect call + probably an allocation).
- The root of the problem is eager execution. We start the task before attaching the continuation, even though we know it beforehand.
- Code using `future`: 14.25
- Code using "lazy futures": 32.38
- Lazy `then`: 23.33
- Lazy blocking: 29.45