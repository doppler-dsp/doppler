/**
 * @file dp_parallel.h
 * @brief A minimal bounded parallel-for over an index range, and a
 * persistent pool with the same contract.
 *
 * doppler is single-threaded C by default; this is the one place that fans a
 * genuinely independent, CPU-bound workload across cores (the per-source
 * signal build in wfm_plan's prepare(); the searcher's roll per thread). The contract is deliberately narrow:
 * run body(i, ctx) for every i in [0, n), possibly concurrently, and return
 * only once all n have completed. body MUST be free of cross-i data races — it
 * may read shared read-only state through ctx and must write only to per-i
 * outputs (a distinct slot per i). Given that, the observable result is
 * bit-identical to the plain serial loop no matter how the work is scheduled.
 *
 * Portability: POSIX threads on the two supported platforms (linux, macos).
 * Every path to *not* running parallel — a single online core, n <= 1, a
 * forced-serial caller, or a malloc/thread-create failure — degrades to
 * running the whole range on the calling thread, so the function always does
 * all the work. Header-only (static) so the sole caller pulls it in with no
 * separate translation unit; link Threads::Threads on that target.
 */
#ifndef DP_PARALLEL_H
#define DP_PARALLEL_H

#include "doppler/clib_common.h" /* dp_xmalloc / dp_xcalloc */
#include "doppler/dp_thread.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

/**
 * @brief One parallel-for run: the range, the work, and the shared cursor.
 *
 * Shared by every worker of a dp_parallel_for() or dp_pool_run() call;
 * workers claim indices by fetch-add on @c next.  Internal: callers never
 * build one.
 */
typedef struct
{
  size_t n;                      /**< index count: work covers [0, n)    */
  void (*body) (size_t, void *); /**< per-index work, body(i, ctx)       */
  void *ctx;                     /**< shared read-only context, passed
                                      through to every body() call      */
  atomic_size_t next;            /**< lock-free work cursor (fetch-add):
                                      the next unclaimed index          */
} dp_pf_shared_t;

/** A worker drains indices off the shared cursor until the range is exhausted.
 * fetch-add is the whole synchronization story: each index is handed to
 * exactly one worker, so there is no contention on the outputs.
 *
 * Declared through DP_THREAD_FN, so it is also a thread entry point; the
 * caller of dp_parallel_for() runs it directly as the last worker. */
DP_THREAD_FN (dp_pf_worker, arg)
{
  dp_pf_shared_t *s = (dp_pf_shared_t *)arg;
  size_t          i;
  while ((i = atomic_fetch_add (&s->next, (size_t)1)) < s->n)
    s->body (i, s->ctx);
  DP_THREAD_RETURN;
}

/**
 * @brief Run body(i, ctx) for every i in [0, n) across up to max_threads
 *        workers.
 *
 * max_threads <= 0 auto-selects the online core count; 1 forces serial. The
 * calling thread is itself a worker, so total concurrency is max_threads
 * (max_threads - 1 helpers spawned + the caller draining alongside them),
 * capped at @p n.  Whenever helpers cannot be spawned the caller alone
 * drains the full range, so the range is always completed.  Helpers are
 * created and joined within this call.
 *
 * @param n            Number of indices; 0 returns at once.
 * @param body         Per-index work.  Each index is passed to exactly one
 *                     call, in no guaranteed order, possibly concurrently.
 * @param ctx          Opaque context passed to every body() call.
 * @param max_threads  Total worker count including the caller; <= 0 means
 *                     the online core count, 1 means serial.
 */
static inline void
dp_parallel_for (size_t n, void (*body) (size_t, void *), void *ctx,
                 int max_threads)
{
  if (n == 0)
    return;

  int nt = max_threads;
  if (nt <= 0)
    {
      long online = dp_cpu_count ();
      nt          = (online > 1) ? (int)online : 1;
    }
  if ((size_t)nt > n)
    nt = (int)n;

  if (nt <= 1) /* serial: single core, forced, or n == 1 */
    {
      for (size_t i = 0; i < n; i++)
        body (i, ctx);
      return;
    }

  dp_pf_shared_t s;
  s.n    = n;
  s.body = body;
  s.ctx  = ctx;
  atomic_init (&s.next, (size_t)0);

  /* Spawn nt-1 helpers; the caller is the nt-th worker. A malloc or
   * A thread-create failure just leaves fewer (or zero) helpers — the caller's
   * own dp_pf_worker() below still drains everything left on the cursor. */
  dp_thread_t *th      = (dp_thread_t *)malloc ((size_t)(nt - 1) * sizeof *th);
  int        spawned = 0;
  if (th)
    for (int t = 0; t < nt - 1; t++)
      {
        if (dp_thread_create (&th[t], dp_pf_worker, &s) != 0)
          break;
        spawned++;
      }

  dp_pf_worker (&s); /* the caller drains alongside the helpers */

  for (int t = 0; t < spawned; t++)
    dp_thread_join (th[t]);
  free (th);
}

/* ── A persistent pool: the same contract, the threads created once ──────
 *
 * dp_parallel_for() spawns its helpers per call, which is right for a caller
 * that fans once per second and wrong for one that fans once per epoch: the
 * continuous searcher's roll per thread (docs/design/async-dsss-receiver.md
 * §2.3) hands work over every 0.2 ms, and a thread creation per worker per
 * push would be most of the budget. dp_pool_run() has exactly
 * dp_parallel_for()'s contract -- body(i, ctx) for every i in [0, n), each
 * index to exactly one worker, bit-identical to the serial loop, the caller
 * draining alongside its helpers, always completing the range -- over
 * helpers created once at dp_pool_create() and parked on a condition
 * variable between runs. A run costs one broadcast and one join-by-counter,
 * not nthreads thread creations.
 *
 * A pool of one thread has no helpers and runs everything on the caller; a
 * NULL pool behaves the same, so a component can carry an optional pool and
 * call dp_pool_run() unconditionally. The pool is not reentrant: one run at
 * a time, from one thread. Every failure to create a helper leaves a smaller
 * pool (dp_pool_threads() says how big), never a broken one. */
/**
 * @brief A persistent worker pool for repeated parallel-for runs.
 *
 * Create with dp_pool_create(), run with dp_pool_run(), free with
 * dp_pool_destroy().  All fields are internal.
 */
typedef struct
{
  dp_mutex_t mu;   /**< guards every field below except work.next */
  dp_cond_t  wake; /**< helpers wait here between runs           */
  dp_cond_t  done; /**< the caller waits here for the run's end  */
  dp_thread_t      *th; /**< helper thread handles (max_threads - 1
                             slots; the first @c helpers are live) */
  int             helpers; /**< helper threads actually running        */
  int             stop;    /**< set by dp_pool_destroy(): helpers exit */
  unsigned long   gen;  /**< bumped per run; helpers wake on a change */
  int             busy; /**< helpers still draining the current run   */
  dp_pf_shared_t  work; /**< the current run's range, body and cursor */
} dp_pool_t;

/** A pool helper's thread body: park on @c wake until dp_pool_run() bumps
 * @c gen or dp_pool_destroy() sets @c stop, drain the run's cursor with
 * dp_pf_worker(), decrement @c busy (signaling @c done at zero), repeat.
 * Returns only on @c stop. */
DP_THREAD_FN (dp_pool_worker, arg)
{
  dp_pool_t    *p   = (dp_pool_t *)arg;
  unsigned long seen = 0;
  for (;;)
    {
      dp_mutex_lock (&p->mu);
      while (!p->stop && p->gen == seen)
        dp_cond_wait (&p->wake, &p->mu);
      if (p->stop)
        {
          dp_mutex_unlock (&p->mu);
          DP_THREAD_RETURN;
        }
      seen = p->gen;
      dp_mutex_unlock (&p->mu);
      dp_pf_worker (&p->work); /* drain the cursor alongside the caller */
      dp_mutex_lock (&p->mu);
      if (--p->busy == 0)
        dp_cond_signal (&p->done);
      dp_mutex_unlock (&p->mu);
    }
}

/**
 * @brief Create a pool of max_threads workers (the caller counts as one).
 *
 * Spawns max_threads - 1 helper threads that park on a condition variable
 * until dp_pool_run().  A helper that fails to start leaves a smaller pool;
 * dp_pool_threads() reports the size actually achieved.
 *
 * @param max_threads  Total worker count including the caller; <= 0
 *                     auto-selects the online core count; 1 creates no
 *                     helper.
 * @return The pool, owned by the caller (free with dp_pool_destroy()).
 *         Never NULL: the pool's own allocations are fixed-size and abort
 *         on OOM.
 */
static inline dp_pool_t *
dp_pool_create (int max_threads)
{
  int nt = max_threads;
  if (nt <= 0)
    {
      long online = dp_cpu_count ();
      nt          = (online > 1) ? (int)online : 1;
    }
  /* Fixed sizes from a validated count: abort-on-OOM, no unwind path. */
  dp_pool_t *p = (dp_pool_t *)dp_xcalloc (1, sizeof *p);
  dp_mutex_init (&p->mu);
  dp_cond_init (&p->wake);
  dp_cond_init (&p->done);
  atomic_init (&p->work.next, (size_t)0);
  if (nt > 1)
    {
      p->th = (dp_thread_t *)dp_xmalloc ((size_t)(nt - 1) * sizeof *p->th);
      for (int t = 0; t < nt - 1; t++)
        {
          if (dp_thread_create (&p->th[t], dp_pool_worker, p) != 0)
            break;
          p->helpers++;
        }
    }
  return p;
}

/**
 * @brief Workers the pool runs work on, the caller included.
 *
 * @param p  The pool, or NULL.
 * @return helpers + 1; 1 for a NULL pool.
 */
static inline int
dp_pool_threads (const dp_pool_t *p)
{
  return p ? p->helpers + 1 : 1;
}

/**
 * @brief Run body(i, ctx) for every i in [0, n) across the pool's threads.
 *
 * Returns once all n have completed; dp_parallel_for()'s contract exactly.
 * A NULL pool, a pool with no helpers, or n == 1 runs serially on the
 * caller.  Not reentrant: one run at a time, from one thread.
 *
 * @param p     The pool, or NULL for serial execution.
 * @param n     Number of indices; 0 returns at once.
 * @param body  Per-index work; each index goes to exactly one call.
 * @param ctx   Opaque context passed to every body() call.
 */
static inline void
dp_pool_run (dp_pool_t *p, size_t n, void (*body) (size_t, void *),
             void *ctx)
{
  if (n == 0)
    return;
  if (!p || p->helpers == 0 || n == 1)
    {
      for (size_t i = 0; i < n; i++)
        body (i, ctx);
      return;
    }
  dp_mutex_lock (&p->mu);
  p->work.n    = n;
  p->work.body = body;
  p->work.ctx  = ctx;
  atomic_store (&p->work.next, (size_t)0);
  p->busy = p->helpers;
  p->gen++;
  dp_cond_broadcast (&p->wake);
  dp_mutex_unlock (&p->mu);
  dp_pf_worker (&p->work); /* the caller drains alongside the helpers */
  dp_mutex_lock (&p->mu);
  while (p->busy > 0)
    dp_cond_wait (&p->done, &p->mu);
  dp_mutex_unlock (&p->mu);
}

/**
 * @brief Stop and join the helpers, free the pool.
 *
 * @param p  The pool, or NULL (a no-op).  Must not be mid-run.
 */
static inline void
dp_pool_destroy (dp_pool_t *p)
{
  if (!p)
    return;
  dp_mutex_lock (&p->mu);
  p->stop = 1;
  dp_cond_broadcast (&p->wake);
  dp_mutex_unlock (&p->mu);
  for (int t = 0; t < p->helpers; t++)
    dp_thread_join (p->th[t]);
  free (p->th);
  dp_cond_destroy (&p->done);
  dp_cond_destroy (&p->wake);
  dp_mutex_destroy (&p->mu);
  free (p);
}

#endif /* DP_PARALLEL_H */
