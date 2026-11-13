# Queue and shutdown decisions

## Bounded FIFO

Capacity bounds queued callbacks, plus one executing callback. Admission returns accepted/full/closed, making overload explicit. Callbacks execute outside the mutex. One consumer preserves execution order for accepted tasks; concurrent producers do not have a predetermined admission order.

This bounds task count, not bytes: large captures can consume substantial memory. Slow callbacks cause head-of-line blocking. A pool improves independent throughput at the expense of completion ordering. A durable broker is needed for process-loss recovery; this queue is in memory.

## Drain, join, then release

Owner-thread `close()` rejects new work, wakes the consumer, drains accepted work and joins. Dependencies captured by callbacks must remain alive until that join. Thread-safe queue storage does not make raw pointers safe.

Concurrent close calls and destruction from a callback are unsupported. A blocked callback can block shutdown forever. Real device I/O needs bounded operations or cooperative cancellation and an explicit drain/discard policy. Forced thread termination is not a safe general solution.

## Exception isolation

One failing callback increments a counter while later work continues. Production callers needing outcomes should receive structured results/futures and diagnostic context. A failure count must not be mistaken for successful completion.
