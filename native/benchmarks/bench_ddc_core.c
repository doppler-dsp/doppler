/* bench_ddc_core.c -- the decimation is not what you pay for, and the
 * closed loop is.
 *
 * A DDC is an LO mix, a filter cascade and a resampler, and the cost of
 * all three is set by the INPUT rate: every input sample is de-rotated and
 * filtered whether or not the resampler keeps it. That is easy to state
 * and easy to disbelieve -- `rate = 0.05` throws away 95% of the samples
 * and feels like it should be cheaper than `rate = 0.5`. The first axis
 * here is four rates spanning 20:1, all measured per INPUT sample and
 * printed beside the output count each one actually produced.
 *
 * Expect that column to be neither flat nor monotone, and do not read it
 * as a trend: a rate is not a dial on one filter, it decomposes into a
 * cascade of stages, so what a rate costs is what its DECOMPOSITION costs.
 * The thing the column settles is the disbelief above -- whether cost
 * tracks the output count at all. It does not.
 *
 * The second axis is the one that costs real money. `ddc_execute` takes a
 * block; `ddc_execute_ctrl_push` takes ONE sample, and it is the only form
 * a closed loop can use -- a carrier or timing loop computes each
 * correction from outputs already emitted, so it cannot hand a whole
 * block's control history over in advance. Feeding the same samples one
 * at a time turns every per-block cost into a per-sample cost, and the
 * ratio between the two rows is what a tracking loop pays for being a
 * loop. Nothing in either signature hints at the size of that.
 *
 * `execute_ctrl` sits between them: a block call that still steers both
 * control ports, for a caller who knows its control history up front. If
 * it costs what plain `execute` costs, then steering is free and only the
 * per-sample granularity is expensive -- which is the useful decomposition,
 * because a receiver chooses granularity and cannot choose to stop
 * steering.
 */
#include "ddc/ddc_core.h"
#include "dp_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define BLOCK 32768
#define N_RATE 4

static const double rates[N_RATE] = { 0.5, 0.25, 0.1, 0.05 };
#define REF_RATE_IDX 1

enum
{
  CFG_EXECUTE,
  CFG_CTRL,
  CFG_PUSH,
  N_FACE
};

static const char *face_name[N_FACE]
    = { "execute", "execute_ctrl", "execute_ctrl_push" };

/* The rate sweep runs on `execute` alone; the three faces are compared at
   one rate. Sweeping both would be twelve rows answering two questions. */
#define N_CFG (N_RATE + (N_FACE - 1))

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];
  ddc_state_t    *ddc[N_RATE] = { 0 };
  float complex  *in = NULL, *out = NULL;
  size_t          cap;
  size_t          emitted[N_RATE] = { 0 };
  char            name[64];

  in = malloc (BLOCK * sizeof *in);
  if (!in)
    return 1;
  for (size_t i = 0; i < BLOCK; i++)
    in[i] = (float complex) (cosf (0.11f * (float)i)
                             + I * sinf (0.11f * (float)i));

  for (int s = 0; s < N_RATE; s++)
    {
      ddc[s] = ddc_create (-0.1, rates[s]);
      if (!ddc[s])
        return 1;
    }

  /* One buffer, sized for the loosest decimation in the sweep. */
  cap = ddc_execute_max_out (ddc[0], BLOCK);
  out = malloc ((cap ? cap : BLOCK) * sizeof *out);
  if (!out)
    return 1;

  printf ("=== ddc (LO mix + cascade + resample, %d input samples) ===\n",
          BLOCK);
  printf ("%d rounds, min over rounds, ns per INPUT sample\n\n", ITERATIONS);

  DP_BENCH_SETTLE ((void)ddc_execute (ddc[REF_RATE_IDX], in, BLOCK, out, cap));

  /* Rounds outside, configurations inside. Both questions here are ratios
     -- rate against rate, face against face -- so no row may own the ramp.
     The push row is the same BLOCK samples, one call each. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      for (int s = 0; s < N_RATE; s++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          emitted[s] = ddc_execute (ddc[s], in, BLOCK, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t[s][r] = dp_bench_elapsed (&t0, &t1);
        }

      clock_gettime (CLOCK_MONOTONIC, &t0);
      (void)ddc_execute_ctrl (ddc[REF_RATE_IDX], in, BLOCK, 0.0, 0.0, out,
                              cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t[N_RATE][r] = dp_bench_elapsed (&t0, &t1);

      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (size_t i = 0; i < BLOCK; i++)
        (void)ddc_execute_ctrl_push (ddc[REF_RATE_IDX], in[i], 0.0, 0.0, out,
                                     cap);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t[N_RATE + 1][r] = dp_bench_elapsed (&t0, &t1);
    }

  for (int s = 0; s < N_RATE; s++)
    {
      (void)snprintf (name, sizeof name, "execute[rate=%.2f]", rates[s]);
      dp_bench_record (&_bench, name, t[s], ITERATIONS, BLOCK, "in-sample");
    }
  for (int f = 1; f < N_FACE; f++)
    {
      (void)snprintf (name, sizeof name, "%s[rate=%.2f]", face_name[f],
                      rates[REF_RATE_IDX]);
      dp_bench_record (&_bench, name, t[N_RATE + f - 1], ITERATIONS, BLOCK,
                       "in-sample");
    }

  printf ("\n  rate sweep, per INPUT sample (rate=%.2f = 1.00x):\n",
          rates[REF_RATE_IDX]);
  for (int s = 0; s < N_RATE; s++)
    printf ("    rate=%.2f  %.2fx   (%zu outputs from %d inputs)\n", rates[s],
            dp_bench_min (t[s], ITERATIONS)
                / dp_bench_min (t[REF_RATE_IDX], ITERATIONS),
            emitted[s], BLOCK);
  printf ("  Read this against the output counts, not as a trend. A rate\n"
          "  is not a dial on one filter: it decomposes into a CASCADE,\n"
          "  and two rates a factor of five apart can land on cascades of\n"
          "  similar cost while a rate between them lands on a dearer one.\n"
          "  What the column does rule out is buying speed by decimating\n"
          "  harder -- nothing here falls with the output count.\n");

  printf ("\n  face cost at rate=%.2f (%s = 1.00x):\n", rates[REF_RATE_IDX],
          face_name[CFG_EXECUTE]);
  for (int f = 1; f < N_FACE; f++)
    printf ("    %-20s %.2fx\n", face_name[f],
            dp_bench_min (t[N_RATE + f - 1], ITERATIONS)
                / dp_bench_min (t[REF_RATE_IDX], ITERATIONS));
  printf ("  The push row is what a closed loop pays for being closed: it\n"
          "  cannot hand its control history over in advance, so it gets\n"
          "  the per-sample form whether or not it wants it.\n");

  for (int s = 0; s < N_RATE; s++)
    ddc_destroy (ddc[s]);
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "ddc");
  return 0;
}
