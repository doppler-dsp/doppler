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
 * dp_once() is the one guard for all of them, and it is the platform's own
 * once primitive behind one signature: `pthread_once` on POSIX,
 * `InitOnceExecuteOnce` on Windows, the pair rs.c used privately since
 * gh-817. The first caller runs the build; every other caller BLOCKS in the
 * primitive until it has finished, then returns and sees everything it
 * wrote. A blocked waiter sleeps rather than spins, so a waiter that
 * outranks the builder on its core cannot starve it, and an oversubscribed
 * machine spends nothing on the wait. glibc also restarts, in a forked
 * child, a once whose build `fork()` interrupted, where a hand-rolled flag
 * would wait forever.
 *
 * The implementation lives in dp_once.c, so `<windows.h>` stays in that one
 * file: a component that builds a table includes this header and pulls in
 * `<pthread.h>` at most. On glibc before 2.34 `pthread_once` needs
 * `-lpthread`, which the `dp_once_core` target carries for every component
 * that links it.
 */
#ifndef DP_ONCE_H
#define DP_ONCE_H

#ifdef _WIN32
/** @brief The state of one one-time build; start it at #DP_ONCE_INIT.
 *
 *  An `INIT_ONCE`, which is one pointer, kept opaque so this header needs
 *  no `<windows.h>`; dp_once.c checks that the sizes agree. */
typedef struct
{
  void *opaque; /**< the INIT_ONCE */
} dp_once_t;
/** @brief The initializer for a #dp_once_t whose build has not run. */
#define DP_ONCE_INIT { 0 }
#else
#include <pthread.h>
/** @brief The state of one one-time build; start it at #DP_ONCE_INIT. */
typedef struct
{
  pthread_once_t once; /**< the platform's once control */
} dp_once_t;
/** @brief The initializer for a #dp_once_t whose build has not run. */
#define DP_ONCE_INIT { PTHREAD_ONCE_INIT }
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Run @p fn once, and return only after it has finished.
   *
   * Every caller, on any thread, returns after the single call of @p fn has
   * completed, and sees everything @p fn wrote. The callers that lose the
   * race to run it block until it is done.
   *
   * @param once  Its state, statically initialized with #DP_ONCE_INIT.
   * @param fn    The build; it must not call dp_once() on the same @p once.
   *
   * @code
   * #include <doppler/dp_once.h>
   * #include <stdio.h>
   *
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
   * // Every function that reads the table calls dp_once first.
   * static float
   * table_at (int i)
   * {
   *   dp_once (&table_once, table_build);
   *   return table[i];
   * }
   *
   * int
   * main (void)
   * {
   *   printf ("%g\n", table_at (255)); // 255
   *   return 0;
   * }
   * @endcode
   */
  void dp_once (dp_once_t *once, void (*fn) (void));

#ifdef __cplusplus
}
#endif

#endif /* DP_ONCE_H */
