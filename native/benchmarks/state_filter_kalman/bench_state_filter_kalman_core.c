#include "doppler/clib_common.h"
#include "doppler/state_filter_kalman/state_filter_kalman_core.h"
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
    in[i] = (double)(i) * 1e-4;

  dp_state_filter_kalman_state_t *obj
      = dp_state_filter_kalman_create_args (0.0, 0.0, 1.0, 0.0, 0.01);

  /* volatile sink prevents DCE of the step() loop */
  volatile double _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = dp_state_filter_kalman_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== state_filter_kalman benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        _sink = dp_state_filter_kalman_step (obj, in[i]);
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
      dp_state_filter_kalman_steps (obj, in, out, BENCH_N);
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

  /* configure: the doubling solve inside the bisection on the noise
   * ratio, paid once per retune. */
  {
    enum
    {
      CALLS = 200
    };
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < CALLS; i++)
          dp_state_filter_kalman_configure (obj, 0.0, 0.0, 1.0, 0.0,
                                            0.005 + 0.0001 * (double)(i & 31));
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "configure", times, ITERATIONS, CALLS, "call");
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  configure() %8.1f ns/call\n", sum / ITERATIONS / CALLS * 1e9);
  }

  /* create with a start above 1: the constructor walks the covariance's
   * trajectory (8/(bn*t) updates, sampled) to check it against the stability
   * rectangle, so the cost scales with 1/(bn*t): paid once per object, and per
   * configure with a start above 1. */
  {
    static const struct
    {
      const char *unit;
      double      bnt;
      int         calls, iters;
    } cs[2] = {
      { "create_start_1e2", 1e-2, 400, 50 },
      { "create_start_1e4", 1e-4, 4, 10 },
    };
    for (int u = 0; u < 2; u++)
      {
        double       times[ITERATIONS];
        const double q
            = dp_state_filter_kalman_ratio_for_bandwidth (cs[u].bnt);
        for (int r = 0; r < cs[u].iters; r++)
          {
            t0 = jm_bench_now_ns ();
            for (int i = 0; i < cs[u].calls; i++)
              {
                dp_state_filter_kalman_state_t *k
                    = dp_state_filter_kalman_create_qr (q, 1.0, 1.0, 3.0);
                if (k)
                  dp_state_filter_kalman_destroy (k);
              }
            t1       = jm_bench_now_ns ();
            times[r] = jm_bench_elapsed_sec (t0, t1);
          }
        jm_bench_add_unit (&_bench, cs[u].unit, times, cs[u].iters,
                           cs[u].calls, "call");
        double sum = 0.0;
        for (int r = 0; r < cs[u].iters; r++)
          sum += times[r];
        printf ("  %s() %8.1f us/call\n", cs[u].unit,
                sum / cs[u].iters / cs[u].calls * 1e6);
      }
  }

  (void)_sink;
  jm_bench_write_json (&_bench, "state_filter_kalman");
  dp_state_filter_kalman_destroy (obj);
  free (in);
  free (out);
  return 0;
}
