/* bench_frame_meter_core.c — the frame-outcome accumulator.
 *
 * This file was a jm scaffold until now -- a `TODO` with no `jm_bench_add`,
 * so it ran under every `jm bench` and recorded nothing. See the same note
 * in bench_frame_core.c; the gate that now catches it is
 * scripts/check_bench_coverage.py.
 *
 * `frame_meter` is a counter, so the interesting rows are not "is it fast"
 * but where its cost actually is:
 *
 *   add              one frame outcome. Called once per frame by a
 *                    receiver harness -- this should be a handful of
 *                    increments and it is worth confirming that it is
 *   fer              the Clopper-Pearson interval over the counters. This
 *                    is the expensive one: it is not a counter read, it
 *                    inverts a beta distribution, and a caller that polls
 *                    it per frame rather than at the end is paying for a
 *                    statistic that cannot have moved much
 *   sync_miss        the same computation over the sync counters. It
 *                    reads cheap here only because this stimulus never
 *                    misses sync, so the interval short-circuits at zero
 *                    errors -- the row is not evidence that it is cheaper
 *   get_frames       a plain counter read, as the baseline the other two
 *                    are read against
 *
 * The `fer`/`add` ratio is the number worth having: it says how wrong it is
 * to poll the interval in the frame loop.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "frame_meter/frame_meter_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100
/* The interval is per-call, not per-frame: it needs its own inner loop to
   rise above the clock. */
#define STAT_N 10000

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile double sink   = 0.0;

  /* target_errors = 10 at 99% is the harness default; `conf` is what the
     interval below is computed at. */
  frame_meter_state_t *m = frame_meter_create (10, 0.99);
  if (!m)
    return 1;

  printf ("=== frame_meter benchmark ===\n");
  printf ("block = %d frame outcomes, %d rounds\n\n", BENCH_N, ITERATIONS);

  static double t_add[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      frame_meter_reset (m);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        frame_meter_add (m, 1, (i % 97) != 0);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_add[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "add", t_add, ITERATIONS, BENCH_N);
  printf ("  %-14s %8.2f ns/frame\n", "add",
          min_sec (t_add, ITERATIONS) / BENCH_N * 1e9);

  static double t_get[ITERATIONS], t_fer[ITERATIONS], t_sync[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < STAT_N; i++)
        sink += (double)frame_meter_get_frames (m);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_get[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "get_frames", t_get, ITERATIONS, STAT_N);
  printf ("  %-14s %8.2f ns/call\n", "get_frames",
          min_sec (t_get, ITERATIONS) / STAT_N * 1e9);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < STAT_N; i++)
        sink += frame_meter_fer (m).lo;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_fer[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "fer", t_fer, ITERATIONS, STAT_N);
  printf ("  %-14s %8.2f ns/call\n", "fer",
          min_sec (t_fer, ITERATIONS) / STAT_N * 1e9);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < STAT_N; i++)
        sink += frame_meter_sync_miss (m).lo;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_sync[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "sync_miss", t_sync, ITERATIONS, STAT_N);
  printf ("  %-14s %8.2f ns/call\n", "sync_miss",
          min_sec (t_sync, ITERATIONS) / STAT_N * 1e9);

  printf ("\n  fer / add = %.0fx. The interval is a Clopper-Pearson\n"
          "  inversion, not a counter read -- compute it when the run ends\n"
          "  or on a schedule, not inside the frame loop.\n"
          "\n  sync_miss reads far cheaper here than fer, and the reason is\n"
          "  the INPUT, not the algorithm: this stimulus reports sync_ok on\n"
          "  every frame, so its interval short-circuits at zero errors.\n"
          "  A run with sync misses pays what fer pays.\n",
          (min_sec (t_fer, ITERATIONS) / STAT_N)
              / (min_sec (t_add, ITERATIONS) / BENCH_N));

  (void)sink;
  frame_meter_destroy (m);
  jm_bench_write_json (&_bench, "frame_meter");
  return 0;
}
