#include "doppler/acc_cf64/acc_cf64_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

typedef double _Complex T;
typedef float W; /* the weights are real in both accumulators */
#define ONE (1.0 + 0.0 * I)
#define COMPONENT "acc_cf64"
#define RESET(o) dp_acc_cf64_reset (o)
#define GET_RE(o) creal (dp_acc_cf64_get (o))

/* One call of block kernel @p k over @p n samples. */
static void
run_kernel (int k, dp_acc_cf64_state_t *obj, const T *x, const W *h, size_t n)
{
  switch (k)
    {
    case 0:
      dp_acc_cf64_madd (obj, x, n, h, n);
      break;
    case 1:
      dp_acc_cf64_add2d (obj, x, n);
      break;
    default:
      dp_acc_cf64_madd2d (obj, x, n, h, n);
      break;
    }
}

int
main (void)
{
  double _Complex *in = malloc (BENCH_N * sizeof (double _Complex));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (int i = 0; i < BENCH_N; i++)
    in[i] = (double)(i) + 0.0 * I;

  dp_acc_cf64_state_t *obj = dp_acc_cf64_create (0.0 + 0.0 * I);

  /* warmup */
  for (int i = 0; i < 16; i++)
    dp_acc_cf64_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== acc_cf64 benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        dp_acc_cf64_step (obj, in[i]);
      t1             = jm_bench_now_ns ();
      _times_step[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "step", _times_step, ITERATIONS, BENCH_N);
  {
    double _s = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_step[r];
    printf ("  step()   %8.1f MSa/s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6);
  }
  double _times_steps[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      dp_acc_cf64_steps (obj, in, BENCH_N);
      t1              = jm_bench_now_ns ();
      _times_steps[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "steps", _times_steps, ITERATIONS, BENCH_N);
  {
    double _s = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_steps[r];
    printf ("  steps()  %8.1f MSa/s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6);
  }

  /* bench: get() */
  {
    double _times_get[ITERATIONS];
    volatile double _Complex get_sink;
    for (int i = 0; i < 16; i++)
      get_sink = dp_acc_cf64_get (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          get_sink = dp_acc_cf64_get (obj);
        t1            = jm_bench_now_ns ();
        _times_get[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add (&_bench, "get", _times_get, ITERATIONS, BENCH_N);
    (void)get_sink; /* consume the DCE sink */
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_get[r];
      printf ("  get()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: dump() */
  {
    double _times_dump[ITERATIONS];
    volatile double _Complex dump_sink;
    for (int i = 0; i < 16; i++)
      dump_sink = dp_acc_cf64_dump (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          dump_sink = dp_acc_cf64_dump (obj);
        t1             = jm_bench_now_ns ();
        _times_dump[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add (&_bench, "dump", _times_dump, ITERATIONS, BENCH_N);
    (void)dump_sink; /* consume the DCE sink */
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_dump[r];
      printf ("  dump()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* madd / add2d / madd2d -- the block kernels, given a real block.
   *
   * These rows used to call each kernel BENCH_N times with (NULL, 0): the
   * kernel returned at once, so the timer saw call overhead, ~0.8 ns, and
   * code alignment moved it 20% (doppler#1366). Now each round is ONE call
   * over BENCH_N samples, credited with BENCH_N, and the result is checked
   * before anything is recorded -- a kernel that stops doing the work makes
   * this binary exit non-zero instead of publishing a faster number. (Under
   * `jm bench` that exit is currently dropped and the component just goes
   * missing from the snapshot: just-makeit#2181.)
   *
   * The sweep below the rows is the evidence the work is real: time per
   * call grows with n while time per sample stays put. It prints, and is
   * not recorded, so the published rows stay one per kernel. */
  {
    T *x = malloc (BENCH_N * sizeof *x);
    W *h = malloc (BENCH_N * sizeof *h);
    if (!x || !h)
      {
        fprintf (stderr, "OOM\n");
        return 1;
      }
    /* x = 1, h = 1/2: every partial sum is exact in T up to 2^24 terms, so
       the expected totals below are equalities, not tolerances. */
    for (int i = 0; i < BENCH_N; i++)
      {
        x[i] = ONE;
        h[i] = 0.5f;
      }

    static const char *const names[3] = { "madd", "add2d", "madd2d" };
    /* madd/madd2d fold sum(x*h) = n/2; add2d folds sum(x) = n. */
    static const double per_sample[3] = { 0.5, 1.0, 0.5 };

    for (int k = 0; k < 3; k++)
      {
        double t[ITERATIONS];
        for (int r = 0; r < ITERATIONS; r++)
          {
            RESET (obj);
            t0 = jm_bench_now_ns ();
            run_kernel (k, obj, x, h, BENCH_N);
            t1   = jm_bench_now_ns ();
            t[r] = jm_bench_elapsed_sec (t0, t1);
          }
        const double want = per_sample[k] * (double)BENCH_N;
        if (!(GET_RE (obj) == want))
          {
            (void)fprintf (stderr,
                           "bench_%s: %s over %d samples gave %g, not %g -- "
                           "the row would time a kernel that did not run\n",
                           COMPONENT, names[k], BENCH_N, GET_RE (obj), want);
            return 1;
          }
        jm_bench_add (&_bench, names[k], t, ITERATIONS, BENCH_N);
        double best = t[0];
        for (int r = 1; r < ITERATIONS; r++)
          if (t[r] < best)
            best = t[r];
        printf ("  %s()  %8.1f MSa/s\n", names[k],
                (double)BENCH_N / best / 1e6);
      }

    printf ("\n  scaling (min over %d rounds; not recorded)\n", ITERATIONS);
    printf ("  %-7s %8s %12s %12s\n", "kernel", "n", "ns/call", "ns/sample");
    static const int ns[4] = { 1024, 4096, 16384, BENCH_N };
    for (int k = 0; k < 3; k++)
      for (int j = 0; j < 4; j++)
        {
          double best = 0.0;
          for (int r = 0; r < ITERATIONS; r++)
            {
              RESET (obj);
              t0 = jm_bench_now_ns ();
              run_kernel (k, obj, x, h, (size_t)ns[j]);
              t1             = jm_bench_now_ns ();
              const double s = jm_bench_elapsed_sec (t0, t1);
              if (r == 0 || s < best)
                best = s;
            }
          printf ("  %-7s %8d %12.1f %12.3f\n", names[k], ns[j], best * 1e9,
                  best * 1e9 / ns[j]);
        }
    free (x);
    free (h);
  }
  jm_bench_write_json (&_bench, "acc_cf64");
  dp_acc_cf64_destroy (obj);
  free (in);

  return 0;
}
