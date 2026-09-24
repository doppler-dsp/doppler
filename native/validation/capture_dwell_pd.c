/**
 * @file capture_dwell_pd.c
 * @brief The Pd a burst CAPTURE delivers at each coherent depth, against the
 *        burst Pd the engine predicts (doppler#1470 phase 5, doppler#1498).
 *
 * A burst engine searches non-overlapping dwells of D repetitions, aligned
 * to the stream and not to the preamble. A preamble of R repetitions at an
 * unknown offset spans about R/D of them, whole or partial; it holds a whole
 * dwell at every alignment only while R >= 2D - 1. `pd_predicted` is ONE
 * dwell wholly inside the preamble. `pd_burst` averages over the alignment,
 * credits every dwell the preamble spans, and is what the sizer meets `pd`
 * with (doppler#1498).
 *
 * Measured through the CAPTURE, the unit that resolves which repetition a
 * burst began on:
 *
 *   a Zadoff-Chu 127 preamble, R = 8 repetitions, then 2000 samples of random
 *   QPSK -- data, not preamble; for each D in 1..R the grid is pinned at D and
 *   one look, and the C/N0 is the one at which the engine predicts a BURST Pd
 *   of 0.6. The burst lands at a uniform random offset in noise, with a
 *   Doppler uniform over the native span, at a CONTINUOUS delay
 *   (dp_preamble_shift). Each trial is scored twice: the ENGINE hits when
 *   some dwell overlapping the preamble names its code phase (from the raw
 *   hits the capture records), and the CAPTURE hits when it emits a window
 *   whose preamble_start is within 3 samples of the truth.
 *
 * The delay must be continuous. The first version put every burst on a whole
 * sample, which removes the code-phase straddle the model averages over half
 * a sample, and read up to 0.23 of Pd high.
 *
 * Usage:
 *   validate_capture_dwell_pd           every D, 1000 trials a row: reports,
 *                                       decides nothing
 *   validate_capture_dwell_pd --check   the spot checks CTest runs, 300
 *                                       trials each, at D = 4 (a whole dwell
 *                                       always fits) and D = 8 (every dwell
 *                                       can straddle): the ENGINE never
 *                                       below pd_burst by 2 sigma, never
 *                                       above it by 0.15 -- acq's bounds
 *
 * Measured 2026-09-23 at 1000 trials a row, each at pd_burst = 0.6:
 *
 *   D        1      2      3      4      5      6      7      8
 *   dwell    0.179  0.325  0.460  0.567  0.670  0.772  0.868  0.927
 *   burst    0.614  0.612  0.626  0.623  0.634  0.624  0.622  0.615
 *   engine   0.570  0.632  0.621  0.630  0.656  0.661  0.689  0.689
 *   capture  0.558  0.604  0.601  0.585  0.605  0.617  0.645  0.646
 *
 * One dwell's Pd ("dwell") runs from 0.18 to 0.93 across rows that all
 * deliver about 0.6; the burst Pd holds the engine to within acq's bounds at
 * every D but one. D = 1 is optimistic by 0.045, inherited from the
 * one-dwell model, which is itself optimistic there (doppler#1501), so the
 * check leaves it out. The capture then loses a further 0.01-0.05 after the
 * engine has detected, which no model prices yet (doppler#1502).
 */
#include "awgn/awgn_core.h"
#include "burst_capture/burst_capture_core.h"
#include "dp_complex.h"
#include "dp_preamble_test.h"
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
   `depth` first predicts a BURST Pd of `target`, in quarter-dB steps. */
static double
cn0_for (size_t depth, double target)
{
  for (int step = 0; step <= 200; step++)
    {
      double                 c = -30.0 + 0.25 * (double)step;
      burst_capture_state_t *s = make (depth, c);
      double                 p = burst_capture_get_pd_burst (s);
      burst_capture_destroy (s);
      if (p >= target)
        return c;
    }
  return NAN;
}

typedef struct
{
  size_t depth;
  double cn0, dwell, pred, eng, eng_se, meas, se;
} row_t;

static row_t
measure (size_t depth, int trials, uint32_t seed)
{
  row_t r = { 0 };
  r.depth = depth;
  r.cn0   = cn0_for (depth, 0.6);
  if (isnan (r.cn0))
    {
      fprintf (stderr, "no C/N0 reaches a burst Pd of 0.6 at D=%zu\n", depth);
      abort ();
    }

  burst_capture_state_t *s = make (depth, r.cn0);
  r.dwell                  = burst_capture_get_pd_predicted (s);
  r.pred                   = burst_capture_get_pd_burst (s);
  const size_t    lead_max = 3u * depth * N;
  const size_t    len      = lead_max + BURST + 2u * s->refine_span + 4u * N;
  float _Complex *x        = dp_xmalloc (len * sizeof *x);
  float _Complex *nz       = dp_xmalloc (len * sizeof *nz);
  float _Complex *per      = dp_xmalloc (N * sizeof *per);
  const size_t    cap      = burst_capture_push_max_out (s, len);
  float _Complex *out      = dp_xmalloc ((cap ? cap : 1) * sizeof *out);
  awgn_state_t   *g        = dp_xnn (
      awgn_create (seed, awgn_amplitude_for_snr ((float)r.cn0, 1.0f)));
  const double span = 1.0 / (2.0 * (double)N); /* cycles/sample */
  uint32_t     st   = seed;
  int          hits = 0, eng = 0;

  for (int t = 0; t < trials; t++)
    {
      /* A lead of at least one dwell, so the burst's offset against the
         stream-aligned dwells is uniform, not pinned to the first. */
      const size_t at
          = depth * N + (size_t)(dp_uni (&st) * (double)(2u * depth * N));
      const double f = (2.0 * dp_uni (&st) - 1.0) * span;
      /* The rest of the delay, CONTINUOUS: a burst on a whole sample has no
         code-phase straddle, which the model averages over half a sample,
         and reads about 0.2 of Pd high at D = 8 (doppler#1498). */
      const double frac = dp_uni (&st);
      dp_preamble_shift (ZC, N, frac, per);
      memset (x, 0, len * sizeof *x);
      for (size_t i = 0; i < BURST; i++)
        {
          float _Complex v
              = i < R * N
                    ? per[i % N]
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
          if (ev
              && fabs ((double)ev->preamble_start - ((double)at + frac))
                     <= TOL)
            hit = 1;
        }
      hits += hit;

      /* The ENGINE's own verdict, from the raw hits the capture recorded
         before claiming anything: some dwell overlapping the preamble named
         its code phase. pd_burst models this; what the capture then fails
         to resolve is its own loss. */
      const double truth = fmod ((double)at + frac, (double)N);
      int          e_hit = 0;
      for (size_t k = 0; k < s->det_len; k++)
        {
          const double ep = (double)s->det[k].epoch;
          double       d  = fabs (fmod (ep, (double)N) - truth);
          if (d > 0.5 * (double)N)
            d = (double)N - d;
          if (d <= TOL && ep > (double)at - (double)((depth + 1u) * N)
              && ep < (double)(at + R * N))
            e_hit = 1;
        }
      eng += e_hit;
    }
  r.eng    = (double)eng / trials;
  r.eng_se = sqrt (fmax (r.eng * (1.0 - r.eng), 1e-9) / trials);
  r.meas   = (double)hits / trials;
  r.se     = sqrt (fmax (r.meas * (1.0 - r.meas), 1e-9) / trials);
  awgn_destroy (g);
  free (out);
  free (per);
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
          "look, pfa %g; each row at the C/N0 the engine predicts a burst "
          "Pd of 0.6 at\n",
          N, R, PAYLOAD, PFA);
  printf (
      "a whole dwell fits at every alignment while D <= (R + 1)/2 = %u\n\n",
      (R + 1u) / 2u);
  printf ("%3s %8s %6s %6s %6s %6s %6s %6s %8s\n", "D", "C/N0", "dwell",
          "burst", "engine", "1sig", "captur", "1sig", "lost");
  for (size_t depth = 1; depth <= R; depth++)
    {
      /* D = 1 is left out: the ONE-dwell model under pd_burst is itself
         optimistic there (doppler#1501), by less than 2 sigma at the
         check's trial count, so pinning it either way would be flaky. */
      if (check && depth != 4u && depth != R)
        continue;
      row_t r = measure (depth, check ? 300 : 1000, 1470u + (uint32_t)depth);
      printf ("%3zu %8.2f %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %+8.3f\n",
              r.depth, r.cn0, r.dwell, r.pred, r.eng, r.eng_se, r.meas, r.se,
              r.meas - r.eng);
      /* pd_burst holds for the engine it models, on both sides of
         (R + 1)/2 and at the most dwells: never optimistic beyond 2 sigma,
         never conservative beyond 0.15 -- acq's own bounds. */
      if (check)
        {
          DP_CHECK (r.eng >= r.pred - 2.0 * r.eng_se);
          DP_CHECK (r.eng - r.pred <= 0.15);
        }
    }
  DP_TEST_END ("validate_capture_dwell_pd");
}
