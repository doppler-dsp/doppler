/*
 * bench_dp_event_log_core.c — what one event costs, and where the cost is.
 *
 * The design puts one hard constraint on this object (async-dsss-receiver.md
 * §11.5): a pool that runs for hours must allocate nothing per state change,
 * and the holder writes events from the same thread that pushes samples. So
 * the number that matters is per EVENT, not per sample, and it has to be
 * small enough that a transition never lands in a block's budget.
 *
 * Three arms, which together say where the microseconds go:
 *
 *   bare      an event with a label and nothing else — the JSON render, the
 *             write and the flush, which is the floor.
 *   fields    the same event carrying eight staged `doppler:` fields, the
 *             shape a receiver transition actually has. The difference from
 *             `bare` is what the staging table costs.
 *   noflush   the bare event written to /dev/null. `bare` minus this is the
 *             price of being crash-safe, and it is the one number that would
 *             make anyone consider batching. Measure it rather than assume
 *             it.
 *
 * jm scaffolds this file once and never rewrites it, and what it scaffolds
 * calls `dp_event_log_create`, which does not exist — the constructor is
 * `dp_event_log_open` via `create_fn`. Hand-owned by necessity, like
 * bench_dp_tlm_capture_core.c; see just-makeit#806.
 */
#include "dp_event_log/dp_event_log_core.h"
#include "jm_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Per EVENT, so the loop counts events. A pool of ten receivers changing
   state a few times a minute emits maybe a thousand an hour; 4096 per
   iteration is far past anything real and keeps the timer honest. */
#define EVENTS 4096
#define ITERATIONS 50
#define BENCH_PATH "bench_dp_evlog.events"

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  double          times[ITERATIONS];
  double          _s;

  printf ("=== dp_event_log benchmark ===\n");
  printf ("%d events per iteration, %d iterations\n\n", EVENTS, ITERATIONS);

#define ARM(label, path, per_event)                                           \
  do                                                                          \
    {                                                                         \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        {                                                                     \
          dp_event_log_t *log = dp_event_log_open (path, 2.4e9);              \
          if (!log)                                                           \
            {                                                                 \
              fprintf (stderr, "open failed\n");                              \
              return 1;                                                       \
            }                                                                 \
          clock_gettime (CLOCK_MONOTONIC, &t0);                               \
          for (int e = 0; e < EVENTS; e++)                                    \
            {                                                                 \
              per_event;                                                      \
              dp_event_log_append (log, (uint64_t)e * 4096u, "tracking", 0,   \
                                   1.0e4, 4.0e6);                             \
            }                                                                 \
          clock_gettime (CLOCK_MONOTONIC, &t1);                               \
          times[r] = elapsed_sec (&t0, &t1);                                  \
          dp_event_log_destroy (log);                                         \
        }                                                                     \
      jm_bench_add (&_bench, label, times, ITERATIONS, EVENTS);               \
      _s = 0.0;                                                               \
      for (int r = 0; r < ITERATIONS; r++)                                    \
        _s += times[r];                                                       \
      printf ("  %-10s %10.3f us/event\n", label,                             \
              (_s / ITERATIONS) / EVENTS * 1e6);                              \
    }                                                                         \
  while (0)

  ARM ("bare", BENCH_PATH, (void)0);

  ARM (
      "fields", BENCH_PATH, do {
        dp_event_log_field (log, "emitter", 3);
        dp_event_log_field (log, "cn0_db_hz", 47.5);
        dp_event_log_field (log, "doppler_hz", 1234.5);
        dp_event_log_field (log, "code_phase_chips", 511.25);
        dp_event_log_field (log, "code_rate", 1.000002);
        dp_event_log_field (log, "since_state_samples", 400000);
        dp_event_log_field (log, "lock_metric", 0.62);
        dp_event_log_field_str (log, "state", "tracking");
      } while (0));

  ARM ("noflush", "/dev/null", (void)0);

#undef ARM

  jm_bench_write_json (&_bench, "dp_event_log");
  remove (BENCH_PATH);
  return 0;
}
