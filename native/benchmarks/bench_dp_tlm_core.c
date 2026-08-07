/*
 * bench_dp_tlm_core.c — what instrumentation actually costs.
 *
 * The whole telemetry design rests on one claim: an object you are not
 * watching pays essentially nothing, and one you are watching pays a bounded,
 * known amount. Until now that claim had no number attached anywhere in the
 * tree, which made "don't regress the emit path" unfalsifiable — the very
 * constraint that shapes dll_core.c's compile-time `tlm_on` dispatch.
 *
 * Four arms, in the order a reader cares about:
 *
 *   detached   NULL context — one predicted-not-taken branch. The number every
 *              uninstrumented pipeline in doppler actually pays.
 *   decimated  attached at decim=16: the phase counter increments and returns.
 *              What a probe costs on the 15 events out of 16 it stays quiet.
 *   emit       attached at decim=1, drained between rounds so the ring never
 *              fills: the real per-record cost, one 16-byte store plus the
 *              acquire/release pair.
 *   full       attached at decim=1 with NOBODY draining. Exercises the
 *              overrun branch — "what if the consumer dies?".
 *
 * One measured result is worth stating up front because it is the opposite of
 * what you would guess: `full` is the SLOWEST arm, several times slower than
 * `emit`. Dropping costs an atomic read-modify-write on the shared `dropped`
 * counter, where writing costs a plain store plus a release. So a dead
 * consumer still cannot stall the DSP — the write remains wait-free and
 * bounded — but it does not make the emit path cheaper either. Another reason
 * to size a capture (dp_tlm_capture/dp_tlm_capture_core.h) rather than run
 * into the ring's ceiling.
 *
 * This file is jm-SCAFFOLDED, not jm-generated: `jm apply` materialises it
 * once and never rewrites it. That matters because what it materialised was a
 * stub that measured nothing ("no step() to benchmark") and wrote an empty
 * benchmarks[] JSON — which silently displaced the real bench, leaving it
 * orphaned in the tree, built by no target and run by nothing. Filed upstream
 * as just-makeit#806. The name follows the COMPONENT (dp_tlm), like
 * test_dp_tlm_core.c, so the file jm scaffolds and the file that holds the
 * real measurements are one file and cannot diverge again.
 *
 * The volatile sink and the read-back keep the optimiser from deleting the
 * loops outright; `emit` deliberately drains OUTSIDE the timed region so the
 * drain's memcpy is not attributed to the producer.
 */
#include "dp_tlm/dp_tlm_core.h"
#include "jm_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  /* Ring comfortably larger than one round, so the `emit` arm measures the
     write and not the overrun branch — `full` is the arm for that. */
  dp_tlm_t *t = dp_tlm_create (1 << 17);
  if (!t)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  int id = dp_tlm_probe (t, "bench.x", 1);

  /* The detached arm needs a context pointer the compiler cannot PROVE is
     NULL. A literal NULL lets it fold dp_tlm_emit away entirely and the arm
     reports terabytes per second — measuring the optimiser, not the branch.
     `volatile` forces the load and the test, which is exactly the cost a
     real uninstrumented object pays. */
  dp_tlm_t *volatile detached = NULL;

  dp_tlm_rec_t *sink = malloc (BENCH_N * sizeof (dp_tlm_rec_t));
  if (!sink)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  double          times[ITERATIONS];
  double          _s;

  printf ("=== dp_tlm benchmark ===\n");
  printf ("block = %d events,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* warmup */
  for (int i = 0; i < 1024; i++)
    dp_tlm_emit (t, id, (double)i);
  dp_tlm_read (t, BENCH_N, sink, BENCH_N);

#define ARM(label, setup, body, teardown)                                     \
  do                                                                          \
    {                                                                         \
      setup;                                                                  \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        {                                                                     \
          clock_gettime (CLOCK_MONOTONIC, &t0);                               \
          for (int i = 0; i < BENCH_N; i++)                                   \
            body;                                                             \
          clock_gettime (CLOCK_MONOTONIC, &t1);                               \
          times[r] = elapsed_sec (&t0, &t1);                                  \
          teardown;                                                           \
        }                                                                     \
      jm_bench_add (&_bench, label, times, ITERATIONS, BENCH_N);              \
      _s = 0.0;                                                               \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        _s += times[r];                                                       \
      printf ("  %-10s %8.1f Mevent/s\n", label,                              \
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);                     \
    }                                                                         \
  while (0)

  ARM ("detached", (void)0, dp_tlm_emit (detached, id, (double)i), (void)0);
  ARM ("decimated", dp_tlm_probe (t, "bench.x", 16),
       dp_tlm_emit (t, id, (double)i),
       dp_tlm_read (t, BENCH_N, sink, BENCH_N));
  ARM ("emit", dp_tlm_probe (t, "bench.x", 1), dp_tlm_emit (t, id, (double)i),
       dp_tlm_read (t, BENCH_N, sink, BENCH_N));
  ARM ("full", (void)0, dp_tlm_emit (t, id, (double)i), (void)0);

#undef ARM

  jm_bench_write_json (&_bench, "dp_tlm");
  free (sink);
  dp_tlm_destroy (t);
  return 0;
}
