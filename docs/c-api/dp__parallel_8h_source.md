

# File dp\_parallel.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_parallel.h**](dp__parallel_8h.md)

[Go to the documentation of this file](dp__parallel_8h.md)


```C++
/* dp_parallel.h — a minimal bounded parallel-for over an index range, and a
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
 * forced-serial caller, or a malloc/pthread_create failure — degrades to
 * running the whole range on the calling thread, so the function always does
 * all the work. Header-only (static) so the sole caller pulls it in with no
 * separate translation unit; link Threads::Threads on that target.
 */
#ifndef DP_PARALLEL_H
#define DP_PARALLEL_H

#include "clib_common.h" /* dp_xmalloc / dp_xcalloc */
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct
{
  size_t n;                      /* index count: work covers [0, n)         */
  void (*body) (size_t, void *); /* per-index work                          */
  void *ctx;                     /* shared read-only context, passed through */
  atomic_size_t next;            /* lock-free work cursor (fetch-add)        */
} dp_pf_shared_t;

/* A worker drains indices off the shared cursor until the range is exhausted.
 * fetch-add is the whole synchronization story: each index is handed to
 * exactly one worker, so there is no contention on the outputs. */
static void *
dp_pf_worker (void *arg)
{
  dp_pf_shared_t *s = (dp_pf_shared_t *)arg;
  size_t          i;
  while ((i = atomic_fetch_add (&s->next, (size_t)1)) < s->n)
    s->body (i, s->ctx);
  return NULL;
}

/* Run body(i, ctx) for every i in [0, n) across up to max_threads workers.
 * max_threads <= 0 auto-selects the online core count; 1 forces serial. The
 * calling thread is itself a worker, so total concurrency is max_threads
 * (max_threads - 1 helpers spawned + the caller draining alongside them).
 * Whenever helpers cannot be spawned the caller alone drains the full range,
 * so the range is always completed. */
static inline void
dp_parallel_for (size_t n, void (*body) (size_t, void *), void *ctx,
                 int max_threads)
{
  if (n == 0)
    return;

  int nt = max_threads;
  if (nt <= 0)
    {
      long online = sysconf (_SC_NPROCESSORS_ONLN);
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
   * pthread_create failure just leaves fewer (or zero) helpers — the caller's
   * own dp_pf_worker() below still drains everything left on the cursor. */
  pthread_t *th      = (pthread_t *)malloc ((size_t)(nt - 1) * sizeof *th);
  int        spawned = 0;
  if (th)
    for (int t = 0; t < nt - 1; t++)
      {
        if (pthread_create (&th[t], NULL, dp_pf_worker, &s) != 0)
          break;
        spawned++;
      }

  dp_pf_worker (&s); /* the caller drains alongside the helpers */

  for (int t = 0; t < spawned; t++)
    pthread_join (th[t], NULL);
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
typedef struct
{
  pthread_mutex_t mu;
  pthread_cond_t  wake; /* helpers wait here between runs           */
  pthread_cond_t  done; /* the caller waits here for the run's end  */
  pthread_t      *th;
  int             helpers; /* helper threads actually running        */
  int             stop;
  unsigned long   gen;  /* bumped per run; helpers wake on a change */
  int             busy; /* helpers still draining the current run   */
  dp_pf_shared_t  work;
} dp_pool_t;

static void *
dp_pool_worker (void *arg)
{
  dp_pool_t    *p   = (dp_pool_t *)arg;
  unsigned long seen = 0;
  for (;;)
    {
      pthread_mutex_lock (&p->mu);
      while (!p->stop && p->gen == seen)
        pthread_cond_wait (&p->wake, &p->mu);
      if (p->stop)
        {
          pthread_mutex_unlock (&p->mu);
          return NULL;
        }
      seen = p->gen;
      pthread_mutex_unlock (&p->mu);
      dp_pf_worker (&p->work); /* drain the cursor alongside the caller */
      pthread_mutex_lock (&p->mu);
      if (--p->busy == 0)
        pthread_cond_signal (&p->done);
      pthread_mutex_unlock (&p->mu);
    }
}

/* Create a pool of max_threads workers (the caller counts as one):
 * max_threads <= 0 auto-selects the online core count; 1 creates no helper.
 * Never NULL: the pool's own allocations are fixed-size and abort on OOM. */
static inline dp_pool_t *
dp_pool_create (int max_threads)
{
  int nt = max_threads;
  if (nt <= 0)
    {
      long online = sysconf (_SC_NPROCESSORS_ONLN);
      nt          = (online > 1) ? (int)online : 1;
    }
  /* Fixed sizes from a validated count: abort-on-OOM, no unwind path. */
  dp_pool_t *p = (dp_pool_t *)dp_xcalloc (1, sizeof *p);
  pthread_mutex_init (&p->mu, NULL);
  pthread_cond_init (&p->wake, NULL);
  pthread_cond_init (&p->done, NULL);
  atomic_init (&p->work.next, (size_t)0);
  if (nt > 1)
    {
      p->th = (pthread_t *)dp_xmalloc ((size_t)(nt - 1) * sizeof *p->th);
      for (int t = 0; t < nt - 1; t++)
        {
          if (pthread_create (&p->th[t], NULL, dp_pool_worker, p) != 0)
            break;
          p->helpers++;
        }
    }
  return p;
}

/* Workers the pool runs work on, the caller included: helpers + 1. */
static inline int
dp_pool_threads (const dp_pool_t *p)
{
  return p ? p->helpers + 1 : 1;
}

/* Run body(i, ctx) for every i in [0, n) across the pool's threads; returns
 * once all n have completed. dp_parallel_for()'s contract exactly. */
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
  pthread_mutex_lock (&p->mu);
  p->work.n    = n;
  p->work.body = body;
  p->work.ctx  = ctx;
  atomic_store (&p->work.next, (size_t)0);
  p->busy = p->helpers;
  p->gen++;
  pthread_cond_broadcast (&p->wake);
  pthread_mutex_unlock (&p->mu);
  dp_pf_worker (&p->work); /* the caller drains alongside the helpers */
  pthread_mutex_lock (&p->mu);
  while (p->busy > 0)
    pthread_cond_wait (&p->done, &p->mu);
  pthread_mutex_unlock (&p->mu);
}

/* Stop and join the helpers, free the pool. NULL is a no-op. */
static inline void
dp_pool_destroy (dp_pool_t *p)
{
  if (!p)
    return;
  pthread_mutex_lock (&p->mu);
  p->stop = 1;
  pthread_cond_broadcast (&p->wake);
  pthread_mutex_unlock (&p->mu);
  for (int t = 0; t < p->helpers; t++)
    pthread_join (p->th[t], NULL);
  free (p->th);
  pthread_cond_destroy (&p->done);
  pthread_cond_destroy (&p->wake);
  pthread_mutex_destroy (&p->mu);
  free (p);
}

#endif /* DP_PARALLEL_H */
```


