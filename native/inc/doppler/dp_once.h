/**
 * @file dp_once.h
 * @brief Run a function exactly once, however many threads ask at once.
 *
 * The library builds some tables lazily, on first use: the AWGN and LO
 * sine LUTs, the CCSDS Reed-Solomon field tables and its marker's bits. A
 * plain `static int ready` flag guarding such a table is a DATA RACE, not a
 * harmless double build. Nothing orders the flag's store after the table's
 * stores for another thread, so that thread can see `ready == 1` and read a
 * table still being written (arm64 reorders those stores, and so may the
 * compiler). Two threads can also both see 0 and both write. First use is
 * concurrent in shipped code: `Plan.prepare` fans the per-source builds
 * across dp_parallel.h, and every block method releases the GIL
 * (doppler#2328, gh-817).
 *
 * dp_once() is the one guard for all of them. It is C11 atomics only, the
 * same `<stdatomic.h>` the library already uses in dp_parallel.h and
 * dp_interrupt.c, so it adds no link dependency: `pthread_once` would need
 * Threads::Threads on every component that builds a table, on glibc before
 * 2.34. The first caller to claim the state builds; every other caller
 * waits until the build is finished, then returns, so no caller ever sees a
 * partial table. The wait is a spin, which costs at most one build's
 * length, once per process.
 *
 * The library is C99 plus `<stdatomic.h>`, and that combination is already
 * accepted on every toolchain it builds with -- gcc, clang and clang-cl --
 * with dp_parallel.h as the precedent; this header uses the same subset
 * (`atomic_int`, explicit-order loads, stores and compare-exchange). It is
 * listed in dp_thread.h's table as the `pthread_once` row.
 */
#ifndef DP_ONCE_H
#define DP_ONCE_H

#include <stdatomic.h>

/** @brief The state of one one-time build; start it at #DP_ONCE_INIT. */
typedef struct
{
  atomic_int state; /**< 0 not started, 1 building, 2 built */
} dp_once_t;

/** @brief The initializer for a #dp_once_t whose build has not run. */
#define DP_ONCE_INIT { 0 }

/**
 * @brief Run @p fn once, and return only after it has finished.
 *
 * Every caller, on any thread, returns after the single call of @p fn has
 * completed, and sees everything @p fn wrote. The callers that lose the
 * race to run it spin until it is done.
 *
 * @param once  Its state, statically initialized with #DP_ONCE_INIT.
 * @param fn    The build; it must not call dp_once() on the same @p once.
 *
 * @code
 * static float     table[256];
 * static dp_once_t table_once = DP_ONCE_INIT;
 *
 * static void
 * table_build (void)
 * {
 *   for (int i = 0; i < 256; i++)
 *     table[i] = (float)i;
 * }
 *
 * // In every function that reads the table:
 * dp_once (&table_once, table_build);
 * @endcode
 */
static inline void
dp_once (dp_once_t *once, void (*fn) (void))
{
  if (atomic_load_explicit (&once->state, memory_order_acquire) == 2)
    return;
  int idle = 0;
  if (atomic_compare_exchange_strong_explicit (&once->state, &idle, 1,
                                               memory_order_acq_rel,
                                               memory_order_acquire))
    {
      fn ();
      atomic_store_explicit (&once->state, 2, memory_order_release);
      return;
    }
  /* Another thread is building: wait for its release store. */
  while (atomic_load_explicit (&once->state, memory_order_acquire) != 2)
    ;
}

#endif /* DP_ONCE_H */
