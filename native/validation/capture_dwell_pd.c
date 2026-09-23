/**
 * @file capture_dwell_pd.c
 * @brief The Pd a burst CAPTURE delivers as its coherent depth grows past
 *        half the preamble (doppler#1470 phase 5).
 *
 * A burst engine searches non-overlapping dwells of D repetitions, aligned
 * to the stream and not to the preamble. A preamble of R repetitions at an
 * unknown offset holds a whole dwell at every alignment only when
 * R >= 2D - 1, the rule acq_create_continuous() already applies to its block
 * depth. Past D = (R + 1)/2 some alignments leave every dwell straddling the
 * preamble's edge -- into the noise before it, or the DATA after it -- while
 * `pd_predicted` assumes a whole dwell of preamble. The burst sizer lets D run
 * to R regardless.
 *
 * This measures what that costs a caller of the CAPTURE, which is the unit
 * that resolves which repetition a burst began on:
 *
 *   a Zadoff-Chu 127 preamble, R = 8 repetitions, then 2000 samples of random
 *   QPSK -- data, not preamble; for each D in 1..R the grid is pinned at D and
 *   one look, and the C/N0 is the one at which the engine itself predicts Pd
 *   0.6, so every row asks the same question: does the capture deliver the Pd
 *   it predicts? The burst lands at a uniform random offset in noise, with a
 *   Doppler uniform over the native span. A trial hits when the capture emits
 *   a window whose preamble_start is within 3 samples of the truth.
 *
 * Usage:
 *   validate_capture_dwell_pd           every D, 1000 trials a row: reports,
 *                                       decides nothing
 *   validate_capture_dwell_pd --check   the spot checks CTest runs, 300
 *                                       trials each: D = 4 (a whole dwell
 *                                       always fits) delivers at least its
 *                                       prediction, and D = 8 (every dwell
 *                                       can straddle) delivers LESS -- the
 *                                       open defect, pinned so a fix to the
 *                                       model has to update this file
 *
 * Measured 2026-09-23 at 1000 trials a row. The prediction is wrong in BOTH
 * directions, and for two different reasons:
 *
 *   D     1      2      3      4      5      6      7      8
 *   pred  0.611  0.604  0.623  0.608  0.631  0.628  0.608  0.614
 *   meas  0.945  0.922  0.898  0.837  0.768  0.653  0.532  0.402
 *
 * - Small D is PESSIMISTIC: `pd_predicted` is one dwell's Pd, and a burst of
 *   R repetitions offers about R/D dwells to detect in.
 * - Past (R + 1)/2 it is OPTIMISTIC: some alignments leave no whole dwell
 *   of preamble, and the one that detects is diluted by noise or data.
 *
 * A hard cap at (R + 1)/2 would stop the optimism but give up real
 * sensitivity (D = 6 at -14.25 dB delivers what D = 4 needs -12.75 dB for).
 * The fix is a Pd model over the burst's alignment that credits every dwell
 * the preamble spans (doppler#1498).
 */
#include "awgn/awgn_core.h"
#include "burst_capture/burst_capture_core.h"
#include "dp_complex.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 127u
#define R 8u
#define PAYLOAD 2000u
#define BURST (R * N + PAYLOAD)
#define PFA 1e-3
#define TOL 3.0

static float _Complex ZC[N];

static void
zadoff_chu (void)
{
  for (size_t k = 0; k < N; k++)
    ZC[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k * (double)(k + 1)
                                  / (double)N);
}

static burst_capture_state_t *
make (size_t depth, double cn0)
{
  burst_capture_state_t *s = burst_capture_create (ZC, N, BURST, R, 1.0, cn0,
                                                   0.0, PFA, 0.9, 0, 0.0);
  if (!s || burst_capture_configure_search_raw (s, depth, 1) != 0)
    {
      fprintf (stderr, "capture at D=%zu refused\n", depth);
      abort ();
    }
  return s;
}

/* The C/N0 (per-sample SNR, dB, at fs = 1) at which the engine pinned at
   `depth` first predicts `target`, in quarter-dB steps. */
static double
cn0_for (size_t depth, double target)
{
  for (int step = 0; step <= 200; step++)
    {
      double                 c = -30.0 + 0.25 * (double)step;
      burst_capture_state_t *s = make (depth, c);
      double                 p = burst_capture_get_pd_predicted (s);
      burst_capture_destroy (s);
      if (p >= target)
        return c;
    }
  return NAN;
}

typedef struct
{
  size_t depth;
  double cn0, pred, meas, se;
} row_t;

static row_t
measure (size_t depth, int trials, uint32_t seed)
{
  row_t r = { 0 };
  r.depth = depth;
  r.cn0   = cn0_for (depth, 0.6);
  if (isnan (r.cn0))
    {
      fprintf (stderr, "no C/N0 reaches Pd 0.6 at D=%zu\n", depth);
      abort ();
    }

  burst_capture_state_t *s = make (depth, r.cn0);
  r.pred                   = burst_capture_get_pd_predicted (s);
  const size_t    lead_max = 3u * depth * N;
  const size_t    len      = lead_max + BURST + 2u * s->refine_span + 4u * N;
  float _Complex *x        = dp_xmalloc (len * sizeof *x);
  float _Complex *nz       = dp_xmalloc (len * sizeof *nz);
  const size_t    cap      = burst_capture_push_max_out (s, len);
  float _Complex *out      = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
  awgn_state_t   *g        = dp_xnn (
      awgn_create (seed, awgn_amplitude_for_snr ((float)r.cn0, 1.0f)));
  const double span = 1.0 / (2.0 * (double)N); /* cycles/sample */
  uint32_t     st   = seed;
  int          hits = 0;

  for (int t = 0; t < trials; t++)
    {
      /* A lead of at least one dwell, so the burst's offset against the
         stream-aligned dwells is uniform, not pinned to the first. */
      const size_t at
          = depth * N + (size_t)(dp_uni (&st) * (double)(2u * depth * N));
      const double f = (2.0 * dp_uni (&st) - 1.0) * span;
      memset (x, 0, len * sizeof *x);
      for (size_t i = 0; i < BURST; i++)
        {
          float _Complex v
              = i < R * N
                    ? ZC[i % N]
                    : (float _Complex)cexp (I * M_PI / 2.0
                                            * (double)(dp_xs32 (&st) >> 30));
          x[at + i]
              = v * (float _Complex)cexp (I * 2.0 * M_PI * f * (double)i);
        }
      awgn_generate (g, len, nz, len);
      for (size_t i = 0; i < len; i++)
        x[i] += nz[i];

      burst_capture_reset (s);
      (void)burst_capture_push (s, x, len, out, cap);
      int hit = 0;
      for (size_t k = 0; k < burst_capture_ready (s); k++)
        {
          const burst_capture_event_t *ev = burst_capture_event_at (s, k);
          if (ev && fabs ((double)ev->preamble_start - (double)at) <= TOL)
            hit = 1;
        }
      hits += hit;
    }
  r.meas = (double)hits / trials;
  r.se   = sqrt (fmax (r.meas * (1.0 - r.meas), 1e-9) / trials);
  awgn_destroy (g);
  free (out);
  free (nz);
  free (x);
  burst_capture_destroy (s);
  return r;
}

int
main (int argc, char **argv)
{
  const int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  zadoff_chu ();
  printf ("Zadoff-Chu %u x %u then %u samples of QPSK; grid pinned at D, one "
          "look, pfa %g; each row at the C/N0 the engine predicts Pd 0.6 at\n",
          N, R, PAYLOAD, PFA);
  printf (
      "a whole dwell fits at every alignment while D <= (R + 1)/2 = %u\n\n",
      (R + 1u) / 2u);
  printf ("%3s %8s %6s %6s %6s %8s\n", "D", "C/N0", "pred", "meas", "1sig",
          "gap");
  for (size_t depth = 1; depth <= R; depth++)
    {
      if (check && depth != 4u && depth != R)
        continue;
      row_t r = measure (depth, check ? 300 : 1000, 1470u + (uint32_t)depth);
      printf ("%3zu %8.2f %6.3f %6.3f %6.3f %+8.3f%s\n", r.depth, r.cn0,
              r.pred, r.meas, r.se, r.meas - r.pred,
              r.depth > (R + 1u) / 2u ? "   (a dwell can straddle)" : "");
      if (check && depth == 4u)
        DP_CHECK (r.meas >= r.pred - 2.0 * r.se);
      if (check && depth == R) /* the open defect, doppler#1498 */
        DP_CHECK (r.meas < r.pred - 2.0 * r.se);
    }
  DP_TEST_END ("validate_capture_dwell_pd");
}
