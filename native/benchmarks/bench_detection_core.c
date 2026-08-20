/* bench_detection_core.c -- the detector design helpers.
 *
 * These already have a Python face that runs and publishes
 * (`src/doppler/detection/benchmarks/bench_detection.py`), so this file is
 * not filling a measurement hole. What it adds is the number with the
 * binding overhead taken OUT, and that changes what the numbers say,
 * because the interesting quantity here is a RATIO between rows:
 *
 *   det_pd     one marcum_q evaluation
 *   det_dwell  det_pd once per candidate dwell, 1..max_dwell
 *   det_snr    det_pd 64 times, plus the doubling that brackets the search
 *
 * At the Python face a call costs tens of nanoseconds before any
 * arithmetic happens, which compresses those three toward each other and
 * hides the structure. At the C face the ratio is the structure -- and it
 * is the number a caller needs, because `det_dwell` is what a link budget
 * sweep calls in its inner loop, not `det_pd`.
 *
 * `marcum_q` is the row everything else is built on: `det_pd` is one call
 * to it, so the whole family's floor is here. `det_q_inv` and
 * `det_threshold` are the Gaussian side, which does no series at all.
 *
 * Inner counts differ per entry -- a search costs hundreds of times what a
 * threshold does -- so each entry carries its own, and `ops` in the JSON
 * is calls per second in every row regardless.
 */
#include "detection/detection_core.h"
#include "dp_bench.h"
#include <stdio.h>

#define ITERATIONS 200

enum
{
  C_MARCUM_M1,
  C_MARCUM_M4,
  C_THRESHOLD,
  C_THRESH_NC,
  C_Q_INV,
  C_EMA_ALPHA,
  C_PD_D1,
  C_PD_D64,
  C_DWELL,
  C_SNR,
  N_CFG
};

static const char *const cfg_name[N_CFG] = {
  "marcum_q[m=1]",      "marcum_q[m=4]",
  "det_threshold",      "det_threshold_noncoherent[n=8]",
  "det_q_inv",          "det_ema_alpha",
  "det_pd[dwell=1]",    "det_pd[dwell=64]",
  "det_dwell[max=256]", "det_snr[dwell=8]",
};

/* Calls per round. A search runs det_pd tens of times, so giving every
   entry the same count would spend the run inside two of them. */
static const int cfg_iters[N_CFG] = {
  4096, 4096, 8192, 4096, 8192, 8192, 4096, 4096, 64, 64,
};

static volatile double sink = 0.0;

/* eta for Pfa = 1e-6, hoisted so det_pd measures det_pd. */
static double eta;

static void
run (int cfg, int i)
{
  /* Inputs vary with i so the run covers a spread of the argument
     domain rather than one branch of it -- marcum_q's series length
     depends on how far `a` is from `b`. */
  const double frac = (double)(i & 63) / 64.0;

  switch (cfg)
    {
    case C_MARCUM_M1:
      sink += marcum_q (1, 0.5 + 4.0 * frac, 4.75);
      break;
    case C_MARCUM_M4:
      sink += marcum_q (4, 0.5 + 4.0 * frac, 4.75);
      break;
    case C_THRESHOLD:
      sink += det_threshold (1e-4 * (1.0 + frac));
      break;
    case C_THRESH_NC:
      sink += det_threshold_noncoherent (1e-4 * (1.0 + frac), 8);
      break;
    case C_Q_INV:
      sink += det_q_inv (1e-3 + 0.4 * frac);
      break;
    case C_EMA_ALPHA:
      sink += det_ema_alpha (0.0, -10.0 - 10.0 * frac);
      break;
    case C_PD_D1:
      sink += det_pd (0.5 + 2.0 * frac, 1, eta);
      break;
    case C_PD_D64:
      sink += det_pd (0.5 + 2.0 * frac, 64, eta);
      break;
    case C_DWELL:
      sink += det_dwell (0.4 + 0.2 * frac, 0.9, 1e-6, 256);
      break;
    default:
      sink += det_snr (8, 0.9, 1e-6);
      break;
    }
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  static double   t[N_CFG][ITERATIONS];

  eta = det_threshold (1e-6);

  printf ("=== detection (detector design helpers) ===\n");
  printf ("%d rounds; inner count differs per entry (a search is not a "
          "threshold)\n\n",
          ITERATIONS);

  DP_BENCH_SETTLE (run (C_PD_D1, 0));

  /* Rounds outside, entries inside -- these ten are read against each
     other, and the ratio is the whole point of the file. */
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

  {
    const double pd
        = dp_bench_min (t[C_PD_D1], ITERATIONS) / (double)cfg_iters[C_PD_D1];
    printf ("\n  det_dwell costs %.0f det_pd calls and det_snr %.0f -- the\n"
            "  search structure, which the Python face's per-call overhead\n"
            "  compresses out. A sweep that calls det_dwell per link-budget\n"
            "  point is paying that multiplier, not the det_pd row.\n",
            dp_bench_min (t[C_DWELL], ITERATIONS) / (double)cfg_iters[C_DWELL]
                / pd,
            dp_bench_min (t[C_SNR], ITERATIONS) / (double)cfg_iters[C_SNR]
                / pd);
  }

  (void)sink;
  jm_bench_write_json (&_bench, "detection");
  return 0;
}
