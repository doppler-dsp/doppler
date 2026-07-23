/* dp_parallel.h — a minimal bounded parallel-for over an index range.
 *
 * doppler is single-threaded C by default; this is the one place that fans a
 * genuinely independent, CPU-bound workload across cores (the per-source
 * signal build in wfm_plan's prepare()). The contract is deliberately narrow:
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

#endif /* DP_PARALLEL_H */
