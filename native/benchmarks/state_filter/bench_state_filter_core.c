#include "doppler/clib_common.h"
#include "doppler/state_filter/state_filter_core.h"
#include "jm_bench.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  double *in = malloc (BENCH_N * sizeof (double));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  double *out = malloc (BENCH_N * sizeof (double));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      free (in);
      return 1;
    }
  for (int i = 0; i < BENCH_N; i++)
    in[i] = (double)(i);

  dp_state_filter_state_t *obj = dp_state_filter_create (0.01, 1.0);

  /* volatile sink prevents DCE of the step() loop */
  volatile double _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = dp_state_filter_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== state_filter benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        _sink = dp_state_filter_step (obj, in[i]);
      t1             = jm_bench_now_ns ();
      _times_step[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add_unit (&_bench, "step", _times_step, ITERATIONS, BENCH_N,
                     JM_BENCH_SAMPLE);
  {
    double _s = 0.0;
    char   _rate[JM_BENCH_RATE_LEN];
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_step[r];
    printf ("  step()   %8.1f M%s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6,
            jm_bench_rate_label (JM_BENCH_SAMPLE, _rate, sizeof (_rate)));
  }
  double _times_steps[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      dp_state_filter_steps (obj, in, out, BENCH_N);
      t1              = jm_bench_now_ns ();
      _times_steps[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add_unit (&_bench, "steps", _times_steps, ITERATIONS, BENCH_N,
                     JM_BENCH_SAMPLE);
  {
    double _s = 0.0;
    char   _rate[JM_BENCH_RATE_LEN];
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_steps[r];
    printf ("  steps()  %8.1f M%s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6,
            jm_bench_rate_label (JM_BENCH_SAMPLE, _rate, sizeof (_rate)));
  }

  /* create: construction pays the same bisection as configure, plus the
   * allocation and the free; a caller that builds a filter per burst feels
   * it. bn varies per call so the compiler cannot hoist the solve. */
  {
    enum
    {
      CALLS = 500
    };
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < CALLS; i++)
          {
            dp_state_filter_state_t *f = dp_state_filter_create (
                0.005 + 0.0001 * (double)(i & 31), 1.0);
            _sink = f ? f->kp : 0.0;
            dp_state_filter_destroy (f);
          }
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "create", times, ITERATIONS, CALLS, "call");
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  create()    %8.1f ns/call\n", sum / ITERATIONS / CALLS * 1e9);
  }

  /* configure: the exact solve for the gains (a bisection on the closed
   * form of the delay-free loop). Not the hot path, but an embedder that
   * retunes every symbol would feel it, so it is a number. */
  {
    enum
    {
      CALLS = 2000
    };
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < CALLS; i++)
          dp_state_filter_configure (obj, 0.005 + 0.0001 * (double)(i & 31),
                                     1.0);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "configure", times, ITERATIONS, CALLS, "call");
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  configure() %8.1f ns/call\n", sum / ITERATIONS / CALLS * 1e9);
  }

  /* stable() and bandwidth(): the exact loop test (the doubling sum over the
   * balanced state matrix), at a delay of two updates and a loop gain of 1.000
   * to 1.031 (varied per call so the compiler cannot hoist it).
   * Not the hot path, but the Kalman flavor's start rule calls stable() along
   * a trajectory and its bandwidth bisection calls bandwidth() a hundred
   * times, so they are numbers. */
  {
    enum
    {
      CALLS = 2000
    };
    double          ts[ITERATIONS], tb[ITERATIONS];
    uint64_t        t2, t3;
    volatile int    ok = 0;
    volatile double bw = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < CALLS; i++)
          ok += dp_state_filter_stable (obj, 2.0,
                                        1.0 + 0.001 * (double)(i & 31));
        t1    = jm_bench_now_ns ();
        ts[r] = jm_bench_elapsed_sec (t0, t1);
        t2    = jm_bench_now_ns ();
        for (int i = 0; i < CALLS; i++)
          bw += dp_state_filter_bandwidth (obj, 2.0,
                                           1.0 + 0.001 * (double)(i & 31));
        t3    = jm_bench_now_ns ();
        tb[r] = jm_bench_elapsed_sec (t2, t3);
      }
    jm_bench_add_unit (&_bench, "stable", ts, ITERATIONS, CALLS, "call");
    jm_bench_add_unit (&_bench, "bandwidth", tb, ITERATIONS, CALLS, "call");
    double ss = 0.0, sb = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      {
        ss += ts[r];
        sb += tb[r];
      }
    printf ("  stable() %8.1f ns/call\n", ss / ITERATIONS / CALLS * 1e9);
    printf ("  bandwidth() %8.1f ns/call\n", sb / ITERATIONS / CALLS * 1e9);
    (void)ok;
    (void)bw;
  }

  (void)_sink;
  jm_bench_write_json (&_bench, "state_filter");
  dp_state_filter_destroy (obj);
  free (in);
  free (out);
  return 0;
}
