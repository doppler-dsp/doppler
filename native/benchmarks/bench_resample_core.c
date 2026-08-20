/* bench_resample_core.c -- the resampler's construction-time helpers.
 *
 * `bench_RateConverter_core.c`, `bench_Resampler_core.c` and
 * `bench_cic_core.c` all measure the steady state: samples through an
 * object that already exists. Nothing measured what it costs to BUILD
 * one, and that is the number a caller needs the moment reconfiguration
 * stops being a start-up event -- a rate that follows a Doppler profile,
 * a decimator retuned per burst.
 *
 * Three helpers, in the order a constructor calls them:
 *
 *   kaiser_beta      attenuation -> window shape. Closed form, no loop.
 *   kaiser_num_taps  attenuation and band edges -> how long the filter is.
 *   ciccompmf        the CIC droop compensator, M taps from a Bernoulli
 *                    series. M is capped at 19 by the table, so unlike a
 *                    filter design this one has a bounded worst case --
 *                    which is worth pinning, because a bounded cost is a
 *                    cost a real-time path can budget for.
 *
 * NB `kaiser_beta` here takes a stopband attenuation. `spectral` carries a
 * second beta formula for a different criterion; they are not
 * interchangeable and neither is wrong. Do not read this row as the cost
 * of "the" Kaiser beta.
 */
#include "dp_bench.h"
#include "resample/resample_core.h"
#include <stdio.h>

#define ITERATIONS 200
#define CIC_MAX_M 19

enum
{
  C_BETA,
  C_NUM_TAPS,
  C_CICCOMP_5,
  C_CICCOMP_19,
  N_CFG
};

static const char *const cfg_name[N_CFG] = {
  "kaiser_beta",
  "kaiser_num_taps",
  "ciccompmf[M=5]",
  "ciccompmf[M=19]",
};

static const int cfg_iters[N_CFG] = { 8192, 8192, 4096, 4096 };

static double          comp[CIC_MAX_M];
static volatile double sink = 0.0;

static void
run (int cfg, int i)
{
  const double atten = 40.0 + (double)(i & 63);

  switch (cfg)
    {
    case C_BETA:
      sink += kaiser_beta (atten);
      break;
    case C_NUM_TAPS:
      sink += (double)kaiser_num_taps (1, atten, 0.10, 0.15);
      break;
    case C_CICCOMP_5:
      ciccompmf (comp, 4u, 16u + (uint32_t)(i & 15), 5u);
      sink += comp[0];
      break;
    default:
      ciccompmf (comp, 4u, 16u + (uint32_t)(i & 15), CIC_MAX_M);
      sink += comp[0];
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];

  printf ("=== resample (construction-time design helpers) ===\n");
  printf ("%d rounds; these run once per object, not once per sample\n\n",
          ITERATIONS);

  DP_BENCH_SETTLE (run (C_CICCOMP_19, 0));

  /* Rounds outside, helpers inside -- the two ciccompmf rows differ only
     in M and the ratio between them is what bounds the worst case. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < cfg_iters[c]; i++)
          run (c, i);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    dp_bench_record (&_bench, cfg_name[c], t[c], ITERATIONS,
                     (size_t)cfg_iters[c], "call");

  printf ("\n  ciccompmf at its M = %d ceiling costs %.2fx the M = 5 case,\n"
          "  and that ceiling is a table bound rather than a caller's\n"
          "  choice -- so the worst case above is THE worst case, which is\n"
          "  what makes it budgetable on a path that retunes per burst.\n",
          CIC_MAX_M,
          dp_bench_min (t[C_CICCOMP_19], ITERATIONS)
              / dp_bench_min (t[C_CICCOMP_5], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "resample");
  return 0;
}
