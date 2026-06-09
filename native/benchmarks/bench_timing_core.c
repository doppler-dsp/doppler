/* bench_timing_core.c — sample-clock stamp + pace-overhead throughput.
 *
 * Pacing's wall-clock cost is the sleep itself (that's the point), so the
 * interesting numbers are the per-call overheads: stamp() is pure arithmetic,
 * and pace() at an impossibly high fs never sleeps, so it measures the
 * scheduling overhead (a CLOCK_MONOTONIC read + the deadline math). Emits
 * pytest-benchmark-compatible JSON via make bench. */
#define _POSIX_C_SOURCE 200809L

#include "jm_bench.h"
#include "timing/timing_core.h"

#include <stdio.h>
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
  jm_bench_t      bench = { 0 };
  struct timespec t0, t1;
  double          times[ITERATIONS];

  printf ("=== timing benchmark ===\n");
  printf ("block = %d calls, %d iterations\n\n", BENCH_N, ITERATIONS);

  /* stamp(): pure arithmetic off a fixed epoch. */
  dp_sample_clock_t c;
  dp_sample_clock_init (&c, 1e6, 0);
  volatile uint64_t sink = 0;
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        {
          c.n = (uint64_t)i;
          sink += dp_sample_clock_stamp (&c);
        }
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  printf ("  %-26s %8.1f M/s\n", "stamp",
          BENCH_N / (times[0] > 0 ? times[0] : 1e-9) / 1e6);
  jm_bench_add (&bench, "stamp", times, ITERATIONS, BENCH_N);

  /* pace() with an impossible fs → every deadline is past, never sleeps;
     measures the per-call scheduling overhead (clock read + math). */
  dp_sample_clock_init (&c, 1e15, 1); /* resync: don't accumulate backlog */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        dp_sample_clock_pace (&c, 1);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&bench, "pace (no-wait overhead)", times, ITERATIONS, BENCH_N);

  (void)sink;
  jm_bench_write_json (&bench, "timing");
  return 0;
}
