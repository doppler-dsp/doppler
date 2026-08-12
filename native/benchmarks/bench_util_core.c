/* bench_util_core.c — the shared EMA primitive.
 *
 * The question this exists to answer is not "how fast is an EMA" — it is
 * three flops and everyone knows. It is **did routing four call sites
 * through `ema_step` cost anything**, because `ema_step` carries an
 * `alpha >= 1.0` branch that the hand-written recursions did not.
 *
 * So every measurement below is a COMPARISON against the exact source
 * each migrated site used to contain:
 *
 *   raw_incremental   what agc_step and async_dsss_receiver had
 *   two_product       what acc_trace had
 *   ema_step          the primitive, branch included
 *
 * and, for the compounded pole:
 *
 *   repeated_multiply what agc_steps had (a1^d, then 1 - ac)
 *   ema_alpha_decim   the primitive (expm1/log1p)
 *
 * A `volatile` sink prevents the loops being optimised away; the inputs
 * are cheap integers so the arithmetic under test dominates.
 */
#include "jm_bench.h"
#include "util/util_core.h"
#include <math.h>
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

/* The pre-migration bodies, kept here verbatim so the comparison is
   against what the tree actually had rather than an approximation. */
static inline double
raw_incremental (double s, double x, double a)
{
  return s + a * (x - s);
}
static inline double
two_product (double s, double x, double a)
{
  return a * x + (1.0 - a) * s;
}
static inline double
repeated_multiply (double alpha, size_t d)
{
  double a1 = 1.0 - alpha, ac = 1.0;
  for (size_t k = 0; k < d; k++)
    ac *= a1;
  return 1.0 - ac;
}

static double
min_per_op (double *t)
{
  double m = t[0];
  for (int r = 1; r < ITERATIONS; r++)
    if (t[r] < m)
      m = t[r];
  return m / (double)BENCH_N * 1e9;
}

/* MIN over iterations, not mean. A microbenchmark's noise is one-sided —
   interrupts and migrations only ever add time — so the minimum is the
   least-biased estimate of the code's cost, and the mean is dominated by
   whatever else the machine did. Measured the difference: on the mean,
   ema_step against raw_incremental swung from +10.6% to -5.6% across
   three consecutive runs, which is noise reported as a finding. */
static void
report (jm_bench_t *b, const char *name, double *t, double base)
{
  jm_bench_add (b, name, t, ITERATIONS, BENCH_N);
  double per = min_per_op (t);
  if (base > 0.0)
    printf ("  %-20s %7.3f ns/op   %+6.1f%%\n", name, per,
            (per / base - 1.0) * 100.0);
  else
    printf ("  %-20s %7.3f ns/op   (baseline)\n", name, per);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile double sink   = 0.0;
  const double    alpha  = 0.05;

  printf ("=== util (EMA primitive) benchmark ===\n");
  printf ("block = %d ops, %d iterations\n\n", BENCH_N, ITERATIONS);
  printf ("ema_step vs the bodies the migrated sites used to contain:\n");

  static double t_raw[ITERATIONS], t_two[ITERATIONS], t_ema[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      double s = 0.0;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        s = raw_incremental (s, (double)(i & 7), alpha);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink     = s;
      t_raw[r] = elapsed_sec (&t0, &t1);
    }
  double base = min_per_op (t_raw);
  report (&_bench, "raw_incremental", t_raw, 0.0);

  for (int r = 0; r < ITERATIONS; r++)
    {
      double s = 0.0;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        s = two_product (s, (double)(i & 7), alpha);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink     = s;
      t_two[r] = elapsed_sec (&t0, &t1);
    }
  report (&_bench, "two_product", t_two, base);

  for (int r = 0; r < ITERATIONS; r++)
    {
      double s = 0.0;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        s = ema_step (s, (double)(i & 7), alpha);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink     = s;
      t_ema[r] = elapsed_sec (&t0, &t1);
    }
  report (&_bench, "ema_step", t_ema, base);

  printf ("\nthe compounded pole, once per chunk (d = 8):\n");
  static double t_rep[ITERATIONS], t_dec[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      double acc = 0.0;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        acc += repeated_multiply (alpha + (double)(i & 3) * 1e-9, 8);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink     = acc;
      t_rep[r] = elapsed_sec (&t0, &t1);
    }
  double base2 = min_per_op (t_rep);
  report (&_bench, "repeated_multiply", t_rep, 0.0);

  for (int r = 0; r < ITERATIONS; r++)
    {
      double acc = 0.0;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        acc += ema_alpha_decim (alpha + (double)(i & 3) * 1e-9, 8);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      sink     = acc;
      t_dec[r] = elapsed_sec (&t0, &t1);
    }
  report (&_bench, "ema_alpha_decim", t_dec, base2);

  printf ("\n  NB the pole is computed ONCE PER CHUNK, not per sample, so\n"
          "  its cost is amortised over `decim` samples (8-32 in the AGC).\n");

  (void)sink;
  jm_bench_write_json (&_bench, "util");
  return 0;
}
