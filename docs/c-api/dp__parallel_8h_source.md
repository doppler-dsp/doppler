

# File dp\_parallel.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_parallel.h**](dp__parallel_8h.md)

[Go to the documentation of this file](dp__parallel_8h.md)


```C++

#ifndef DP_PARALLEL_H
#define DP_PARALLEL_H

#include "doppler/clib_common.h" /* dp_xmalloc / dp_xcalloc */
#include "doppler/dp_thread.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct
{
  size_t n;                      
  void (*body) (size_t, void *); 
  void *ctx;                     
  atomic_size_t next;            
} dp_pf_shared_t;

DP_THREAD_FN (dp_pf_worker, arg)
{
  dp_pf_shared_t *s = (dp_pf_shared_t *)arg;
  size_t          i;
  while ((i = atomic_fetch_add (&s->next, (size_t)1)) < s->n)
    s->body (i, s->ctx);
  DP_THREAD_RETURN;
}

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
typedef struct
{
  dp_mutex_t mu;   
  dp_cond_t  wake; 
  dp_cond_t  done; 
  dp_thread_t      *th; 
  int             helpers; 
  int             stop;    
  unsigned long   gen;  
  int             busy; 
  dp_pf_shared_t  work; 
} dp_pool_t;

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

static inline int
dp_pool_threads (const dp_pool_t *p)
{
  return p ? p->helpers + 1 : 1;
}

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
```


