/**
 * @file dp_bench.h
 * @brief The parts of a benchmark that are the same in every benchmark.
 *
 * `jm_bench.h` (vendored, jm-owned) records timings and writes the JSON.
 * What it does not carry is the part every file here was copying by hand:
 * the monotonic-clock delta, the MIN over rounds, and the settle that has
 * to happen ONCE per process rather than once per configuration. This
 * header owns those three, and lives beside the benchmarks rather than
 * inside `jm_bench.h` because a local edit to a vendored file is destroyed
 * by the next re-vendor -- `JM_SUMSQ_F32` was lost that way and now lives
 * in `dp_simd.h`. Same split, same reason.
 *
 * **MIN over rounds, never mean.** A microbenchmark's noise is one-sided:
 * an interrupt, a migration or a frequency step can only ever add time.
 * So the minimum is the least-biased estimate of what the code costs and
 * the mean is an estimate of what else the machine was doing. Measured in
 * `bench_util_core.c`: on the mean, one kernel swung from +10.6% to -5.6%
 * against its baseline across three consecutive runs of the same binary.
 *
 * **Settle once, then interleave.** `DP_BENCH_SETTLE` runs a statement
 * until the clock has stopped ramping, before ANY configuration is timed;
 * the caller then puts rounds on the outside and configurations on the
 * inside, so drift the settle did not catch lands on every configuration
 * instead of on whichever was measured first. `bench_viterbi_core.c`
 * measured what skipping this costs: a real 1.41x read as 1.03x, and once
 * as 0.99x -- a 95-step traceback appearing cheaper than a 34-step one,
 * which is not a thing that can happen. See doppler#896.
 */
#ifndef DP_BENCH_H
#define DP_BENCH_H

#include "jm_bench.h"
#include <stdio.h>
#include <time.h>

/** @brief Seconds of untimed work before the first measurement. */
#define DP_BENCH_WARMUP_S 0.25

/** @brief Monotonic seconds between two samples. */
static inline double
dp_bench_elapsed (const struct timespec *t0, const struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/** @brief The fastest of @p rounds timings -- see the note above. */
static inline double
dp_bench_min (const double *t, int rounds)
{
  double m = t[0];
  for (int r = 1; r < rounds; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/**
 * @brief Run @p stmt until the clock has settled, timing nothing.
 *
 * Once per process, before the first configuration is timed. A per-config
 * warm-up does not do this: the first config is still the one paying the
 * ramp from whatever the machine was doing before the process started.
 */
#define DP_BENCH_SETTLE(stmt)                                                 \
  do                                                                          \
    {                                                                         \
      struct timespec _w0, _w1;                                               \
      clock_gettime (CLOCK_MONOTONIC, &_w0);                                  \
      do                                                                      \
        {                                                                     \
          stmt;                                                               \
          clock_gettime (CLOCK_MONOTONIC, &_w1);                              \
        }                                                                     \
      while (dp_bench_elapsed (&_w0, &_w1) < DP_BENCH_WARMUP_S);              \
    }                                                                         \
  while (0)

/**
 * @brief Record one entry and print its min-derived rate.
 *
 * @param b       Accumulator passed to `jm_bench_write_json` at the end.
 * @param name    Entry name; jm prefixes it with the component.
 * @param t       Per-round elapsed seconds, length @p rounds.
 * @param rounds  Outer iteration count.
 * @param iters   Units of work per round -- so `ops` in the JSON is the
 *                natural throughput unit rather than calls per second.
 * @param unit    Noun for that unit ("sample", "bin", "bit", "call").
 */
static inline void
dp_bench_record (jm_bench_t *b, const char *name, const double *t, int rounds,
                 size_t iters, const char *unit)
{
  jm_bench_add (b, name, t, rounds, (int)iters);
  const double sec = dp_bench_min (t, rounds);
  printf ("  %-34s %9.3f ns/%-7s %9.2f M%s/s\n", name,
          sec / (double)iters * 1e9, unit, (double)iters / sec / 1e6, unit);
}

#endif /* DP_BENCH_H */
