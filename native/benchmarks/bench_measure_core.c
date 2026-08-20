/* bench_measure_core.c -- the capture-planning helpers.
 *
 * This file was written expecting to prove that planning is free -- six
 * rows of a handful of flops each, measured because "it is only a few
 * flops" is a claim and not a reason to leave a surface unmeasured.
 * Four of the six came out that way, at 2-3 ns.
 *
 * The other two did not, and they are why the file is worth having.
 *
 * **measure_min_samples costs ~25 us**, four orders of magnitude above
 * its neighbours and more than the FFT it is sizing. It is not
 * arithmetic: it resolves a dynamic-range target, picks a window that
 * meets it via `kaiser_beta_for_sidelobe`, and works back to a length.
 * Once per capture that is invisible; called per candidate in a planner
 * sweep it is the sweep. Nothing in the signature says so, and until this
 * benchmark nothing measured it.
 *
 * **dp_coherent_freq's cost is set by N's FACTORISATION, not N's size.**
 * It snaps a target to `J * fs / N` with J coprime to N, and finds that J
 * by walking outward from the nearest candidate until the gcd comes back
 * 1. So what it costs is how many integers near J share a factor with N:
 *
 *   N = 4096  = 2^12          half of all J are coprime; the walk is short
 *   N = 30030 = 2.3.5.7.11.13 phi(N)/N ~ 0.19; four in five candidates fail
 *
 * Same order of magnitude in N, one a multiple of the other's cost. A
 * caller choosing a capture length for an unrelated reason -- a power of
 * two for the FFT, say -- is choosing this too.
 */
#include "dp_bench.h"
#include "measure/measure_core.h"
#include <stdio.h>

#define ITERATIONS 200

enum
{
  C_DR_FROM_BITS,
  C_MIN_SAMPLES,
  C_REC_NFFT,
  C_PROC_GAIN,
  C_COHERENT_P2,
  C_COHERENT_SMOOTH,
  N_CFG
};

static const char *const cfg_name[N_CFG] = {
  "measure_dr_from_bits[inline]",
  "measure_min_samples",
  "measure_rec_nfft",
  "measure_proc_gain",
  "dp_coherent_freq[N=4096]",
  "dp_coherent_freq[N=30030]",
};

static const int cfg_iters[N_CFG] = { 8192, 8192, 8192, 8192, 4096, 4096 };

static volatile double sink = 0.0;

static void
run (int cfg, int i)
{
  const double frac = (double)(i & 63) / 64.0;
  const size_t bits = 8u + (size_t)(i & 7);

  switch (cfg)
    {
    case C_DR_FROM_BITS:
      sink += measure_dr_from_bits (bits);
      break;
    case C_MIN_SAMPLES:
      sink += (double)measure_min_samples (1e6 * (1.0 + frac), 100.0, bits,
                                           0.0, 1);
      break;
    case C_REC_NFFT:
      sink += (double)measure_rec_nfft (1000u + (size_t)(i & 1023), 4u);
      break;
    case C_PROC_GAIN:
      sink += measure_proc_gain (1024u << (i & 3));
      break;
    case C_COHERENT_P2:
      sink += dp_coherent_freq (1e6, 1e5 * (1.0 + frac), 4096u);
      break;
    default:
      sink += dp_coherent_freq (1e6, 1e5 * (1.0 + frac), 30030u);
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];

  printf ("=== measure (capture-planning helpers) ===\n");
  printf ("%d rounds; planning is once per capture, not per sample\n\n",
          ITERATIONS);

  DP_BENCH_SETTLE (run (C_MIN_SAMPLES, 0));

  /* Rounds outside, helpers inside: the two dp_coherent_freq rows are
     read against each other and differ only in N's factorisation, so a
     drift landing on one of them would BE the finding. */
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

  printf ("\n  measure_min_samples costs %.0fx measure_rec_nfft -- it is not\n"
          "  arithmetic, it designs a window to hit a dynamic-range target.\n"
          "  Once per capture that is invisible; once per candidate in a\n"
          "  planner sweep it IS the sweep.\n",
          (dp_bench_min (t[C_MIN_SAMPLES], ITERATIONS)
           / (double)cfg_iters[C_MIN_SAMPLES])
              / (dp_bench_min (t[C_REC_NFFT], ITERATIONS)
                 / (double)cfg_iters[C_REC_NFFT]));

  printf ("\n  dp_coherent_freq costs %.2fx more at N = 30030 than at\n"
          "  N = 4096, for a capture 7x longer -- the coprime walk, not\n"
          "  the length. A power-of-two N is the cheap case here for the\n"
          "  same reason it is the cheap case for the FFT, and that is a\n"
          "  coincidence a caller should not have to rediscover.\n",
          dp_bench_min (t[C_COHERENT_SMOOTH], ITERATIONS)
              / dp_bench_min (t[C_COHERENT_P2], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "measure");
  return 0;
}
