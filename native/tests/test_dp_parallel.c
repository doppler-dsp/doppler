/* test_dp_parallel.c — the parallel-for and the persistent pool keep one
 * contract: every index to exactly one worker, the result bit-identical to
 * the serial loop, the range always completed, the caller draining
 * alongside. The pool adds: helpers created once and reused across runs,
 * a pool of one (and a NULL pool) running everything on the caller, and a
 * clean stop. Runs under TSan in the C suite (make test-tsan). */
#include "dp_parallel.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
  uint64_t   *out;   /* per-index output: a distinct slot per i         */
  atomic_int *calls; /* per-index call count: must end at exactly one   */
  atomic_int  live;  /* bodies running right now, for the peak below    */
  atomic_int  peak;
  size_t      n;
} work_t;

static uint64_t
f (size_t i)
{
  /* deliberately cheap per item; the slow tail is where the time goes */
  uint64_t x = (uint64_t)i * 2654435761u + 12345u; /* nonzero seed */
  for (int k = 0; k < 200; k++) /* enough work to make the fan real */
    (void)dp_xs64 (&x);
  return x;
}

static void
body (size_t i, void *ctx)
{
  work_t *w = (work_t *)ctx;
  int     l = atomic_fetch_add (&w->live, 1) + 1;
  int     p = atomic_load (&w->peak);
  while (l > p && !atomic_compare_exchange_weak (&w->peak, &p, l))
    ;
  uint64_t v = f (i);
  /* The tail of the range is slow, so a helper is still inside its last
     item when a caller that did not wait for the helpers returns -- the
     missing join shows as an unwritten slot, not as luck. */
  if (i + 8 >= w->n)
    for (int k = 0; k < 400; k++)
      v ^= f (i + (size_t)k);
  w->out[i] = v;
  atomic_fetch_add (&w->calls[i], 1);
  atomic_fetch_sub (&w->live, 1);
}

static uint64_t
expect (size_t i, size_t n)
{
  uint64_t v = f (i);
  if (i + 8 >= n)
    for (int k = 0; k < 400; k++)
      v ^= f (i + (size_t)k);
  return v;
}

static int
run_and_check (const char *what, dp_pool_t *pool, int use_pool,
               int max_threads, size_t n, const uint64_t *ref)
{
  (void)ref; /* the expectation depends on n: computed per call below */
  uint64_t   *out   = (uint64_t *)calloc (n ? n : 1, sizeof *out);
  atomic_int *calls = (atomic_int *)calloc (n ? n : 1, sizeof *calls);
  work_t      w     = { out, calls, 0, 0, n };
  if (use_pool)
    dp_pool_run (pool, n, body, &w);
  else
    dp_parallel_for (n, body, &w, max_threads);
  /* The contract's first clause: return only once all n have completed --
     so nothing is still inside body() when the call comes back. */
  DP_CHECK_MSG (atomic_load (&w.live) == 0,
                "no body is still running when the fan returns");
  int once = 1, same = 1;
  for (size_t i = 0; i < n; i++)
    {
      if (atomic_load (&calls[i]) != 1)
        once = 0;
      if (out[i] != expect (i, n))
        same = 0;
    }
  DP_CHECK_MSG (once, what);
  DP_CHECK_MSG (same, what);
  int peak = atomic_load (&w.peak);
  free (out);
  free (calls);
  return peak;
}

int
main (void)
{
  const size_t n   = 4096;
  uint64_t    *ref = (uint64_t *)malloc (n * sizeof *ref);
  for (size_t i = 0; i < n; i++)
    {
      uint64_t v = f (i);
      if (i + 8 >= n)
        for (int k = 0; k < 400; k++)
          v ^= f (i + (size_t)k);
      ref[i] = v;
    }

  /* ── the per-call fan, as it was ──────────────────────────────────── */
  (void)run_and_check ("parallel_for serial: once each, bit-identical", NULL,
                       0, 1, n, ref);
  int pk = run_and_check ("parallel_for x4: once each, bit-identical", NULL, 0,
                          4, n, ref);
  DP_CHECK (pk >= 1 && pk <= 4);
  (void)run_and_check ("parallel_for n < threads", NULL, 0, 8, 3, ref);
  (void)run_and_check ("parallel_for n == 0", NULL, 0, 4, 0, ref);

  /* ── the pool: created once, reused ───────────────────────────────── */
  dp_pool_t *p4 = dp_pool_create (4);
  DP_REQUIRE (p4 != NULL);
  DP_CHECK (dp_pool_threads (p4) >= 1 && dp_pool_threads (p4) <= 4);
  for (int r = 0; r < 5; r++) /* five runs on the same helpers */
    {
      int peak = run_and_check ("pool x4: once each, bit-identical", p4, 1, 0,
                                n, ref);
      DP_CHECK (peak >= 1 && peak <= dp_pool_threads (p4));
    }
  (void)run_and_check ("pool: n < threads", p4, 1, 0, 2, ref);
  (void)run_and_check ("pool: n == 1", p4, 1, 0, 1, ref);
  (void)run_and_check ("pool: n == 0", p4, 1, 0, 0, ref);
  /* Runs of different lengths back to back: the cursor is re-armed. */
  (void)run_and_check ("pool: 1000 after 4096", p4, 1, 0, 1000, ref);
  (void)run_and_check ("pool: 4096 after 1000", p4, 1, 0, n, ref);
  dp_pool_destroy (p4);

  /* A pool of one has no helpers: everything on the caller, in order. */
  dp_pool_t *p1 = dp_pool_create (1);
  DP_REQUIRE (p1 != NULL);
  DP_CHECK (dp_pool_threads (p1) == 1);
  int pk1 = run_and_check ("pool of one: serial", p1, 1, 0, n, ref);
  DP_CHECK (pk1 == 1);
  dp_pool_destroy (p1);
  /* NULL is the same pool of one, so an optional pool needs no branch. */
  int pk0 = run_and_check ("NULL pool: serial", NULL, 1, 0, n, ref);
  DP_CHECK (pk0 == 1);
  dp_pool_destroy (NULL); /* must not crash */

  /* Auto-sizing picks at least one worker. */
  dp_pool_t *pa = dp_pool_create (0);
  DP_REQUIRE (pa != NULL);
  DP_CHECK (dp_pool_threads (pa) >= 1);
  (void)run_and_check ("pool auto: once each, bit-identical", pa, 1, 0, n,
                       ref);
  dp_pool_destroy (pa);

  free (ref);
  DP_TEST_END ("test_dp_parallel");
}
