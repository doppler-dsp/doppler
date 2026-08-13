/**
 * @file ratesync_scurve.c
 * @brief Validation: each TED's own S-curve, measured against the
 *        construct-time constant that is supposed to flatten it.
 *
 * `ratesync_loop_t::ted_scale` is `1 / symsync_ted_slope(ted, pulse, beta,
 * span)`, and the whole argument for a construct-time normaliser rests on
 * that reciprocal being right: divide the raw detector output by the
 * detector's OWN slope and `bn` names one loop bandwidth, at every roll-off
 * and on either detector. Nothing measured whether it does.
 *
 * This harness measures it the direct way, which is the way no Python
 * validator can:
 *
 *   - **No cascade.** The matched pair's composite is a raised cosine in
 *     closed form (`wfm_rc_h`), so the on-time and half-symbol samples at an
 *     ARBITRARY offset `tau` are evaluated analytically rather than
 *     resampled. What that buys is attribution: a discrepancy measured here
 *     is the detector's or the model's, and cannot be the polyphase bank's,
 *     the CIC's or the loop's.
 *   - **The raw numerator.** `gardner_ted` / `dttl_ted` are called directly,
 *     so the number compared against `symsync_ted_slope()` is the same
 *     quantity that function claims to return, not a normalised error read
 *     back through a binding.
 *   - **Across beta.** `symsync_ted_slope`'s own doxygen records that the
 *     shipped normalisation's slope varies 10.6x between beta 0.1 and 0.9.
 *     A single roll-off cannot see that; this sweeps the supported range.
 *
 * The measurement is a paired central difference: the SAME symbol sequence
 * drives `+d` and `-d`, so the data-dependent part of the S-curve cancels in
 * the difference and a few hundred thousand symbols give a slope stable to
 * several digits. Both the on-time and half-symbol streams are formed by
 * convolving the symbol sequence with the composite sampled at the offset,
 * which is what a matched receiver at that timing phase actually sees.
 *
 * @par What it found, and what it exonerated
 * BOTH detectors' measured slopes match `symsync_ted_slope()` across the
 * whole roll-off range, to better than 2%. That matters because it settles a
 * question the Python side could not: the RateSync validation report measures
 * a normalised S-curve slope of ~1.00 for Gardner and ~2.60 for DTTL THROUGH
 * THE CASCADE, and the obvious reading -- that the construct-time normaliser
 * is wrong for DTTL -- is the reading this harness rules out. The normaliser
 * is right, for both, at every roll-off; `ratesync_create()` installs
 * `1 / symsync_ted_slope()` correctly for both (checked below); so whatever
 * moves the through-cascade number sits between the analytic composite and
 * what the terminal stage actually hands the detector, and is NOT this
 * function. See F15 in the RateSync validation report, and gh-669.
 *
 * That is the whole reason to measure a detector without the cascade around
 * it: a loop that locks tells you the composite works, and only a direct
 * sweep tells you which half of it is wrong.
 *
 * Usage:  ratesync_scurve [--check]
 */
#include "ratesync/ratesync_core.h"
#include "symsync/symsync_core.h"
#include "wfm/wfm_dsp.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One-sided pulse span in symbols; matches RateSync's default and is what
   symsync_ted_slope() is handed, so the two see the same truncation. */
#define SPAN 8
/* Neighbours summed into one sample. The composite is Nyquist, so the
   far tails contribute only ISI, but they are cheap here: the taps are
   precomputed once per offset and reused for every symbol. */
#define KMAX (SPAN + 2)
#define NTAP (2 * KMAX + 1)
/* Symbols averaged per offset. The pairing below removes almost all of the
   data variance, so this is generous rather than marginal. */
#define NSYM 200000
/* Central-difference half-step, in symbols. Small enough to sit inside every
   pulse's linear region and large enough that the composite's own rounding
   does not show -- the same reasoning, and nearly the same value, as
   symsync_ted_slope's own `d`. */
#define DTAU 1e-3

static uint32_t
xs32 (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  return *st;
}

/**
 * Mean raw detector output over NSYM i.i.d. BPSK symbols, at timing offset
 * @p tau, with no cascade anywhere in the path.
 *
 * `gy[j]` is the composite sampled at the on-time instant `tau + j` and
 * `gm[j]` at the half-symbol-early gate `tau - 0.5 + j`, so one symbol's
 * samples are a dot product of the tap arrays with the symbols around it.
 */
static double
s_curve_measured (int ted, double beta, double tau, uint32_t seed)
{
  double gy[NTAP], gm[NTAP];
  for (int j = -KMAX; j <= KMAX; j++)
    {
      gy[j + KMAX] = wfm_rc_h (tau + (double)j, beta);
      gm[j + KMAX] = wfm_rc_h (tau - 0.5 + (double)j, beta);
    }

  /* Ring of the most recent symbols, newest last. */
  double   a[NTAP];
  uint32_t st = seed;
  for (int i = 0; i < NTAP; i++)
    a[i] = (xs32 (&st) & 1u) ? 1.0 : -1.0;

  double sum    = 0.0;
  double prev_y = 0.0;
  int    have   = 0;
  long   n      = 0;
  for (long k = 0; k < NSYM; k++)
    {
      /* y and mid for the symbol currently at the ring's centre. */
      double y = 0.0, mid = 0.0;
      for (int j = 0; j < NTAP; j++)
        {
          /* a[j] is the symbol (KMAX - j) periods AHEAD of centre, so it is
             weighted by the tap at that displacement. */
          y += a[j] * gy[NTAP - 1 - j];
          mid += a[j] * gm[NTAP - 1 - j];
        }
      if (have)
        {
          double e = (ted == SYMSYNC_TED_DTTL)
                         ? dttl_ted ((float)mid, (float)y, (float)prev_y)
                         : gardner_ted ((float)mid, (float)(y - prev_y));
          sum += e;
          n++;
        }
      prev_y = y;
      have   = 1;
      /* Slide one symbol in. */
      memmove (a, a + 1, (NTAP - 1) * sizeof (double));
      a[NTAP - 1] = (xs32 (&st) & 1u) ? 1.0 : -1.0;
    }
  return n ? sum / (double)n : 0.0;
}

/* Measured |dS/dtau| at the lock point, paired so the data cancels. */
static double
slope_measured (int ted, double beta)
{
  const uint32_t seed = 0x5eed1234u;
  double         sp   = s_curve_measured (ted, beta, DTAU, seed);
  double         sm   = s_curve_measured (ted, beta, -DTAU, seed);
  return fabs ((sp - sm) / (2.0 * DTAU));
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);

  const double betas[] = { 0.1, 0.2, 0.35, 0.5, 0.9 };
  const size_t nbeta   = sizeof betas / sizeof *betas;
  const int    teds[]  = { SYMSYNC_TED_GARDNER, SYMSYNC_TED_DTTL };
  const char  *names[] = { "gardner", "dttl" };

  /* Both detectors are expected to match, so this is a real gate on both
     rather than a ratchet on known breakage. The measured spread is under
     2% (worst at beta = 0.1, where the pulse's tails carry most of the
     slope and both this harness and symsync_ted_slope truncate them), so 5%
     leaves room for the truncation without admitting a real error: a
     normaliser that stopped matching its detector moves by a FACTOR, which
     is what the through-cascade DTTL number does. */
  const double TOL = 0.05;

  int fail = 0;

  printf ("TED S-curve slope vs symsync_ted_slope() -- RRC, span %d, "
          "%d symbols/point, no cascade\n\n",
          SPAN, NSYM);
  printf ("  %-8s %6s %14s %14s %8s\n", "ted", "beta", "measured", "declared",
          "ratio");
  for (size_t t = 0; t < 2; t++)
    {
      for (size_t b = 0; b < nbeta; b++)
        {
          double meas = slope_measured (teds[t], betas[b]);
          double decl
              = symsync_ted_slope (teds[t], SYMSYNC_PULSE_RRC, betas[b], SPAN);
          double ratio = (decl > 0.0) ? meas / decl : 0.0;
          printf ("  %-8s %6.2f %14.6f %14.6f %8.4f\n", names[t], betas[b],
                  meas, decl, ratio);

          if (fabs (ratio - 1.0) > TOL)
            {
              fprintf (stderr,
                       "  %s beta=%.2f: ratio %.4f is outside 1 +- %.2f -- "
                       "the construct-time normaliser no longer matches the "
                       "detector it normalises, so `bn` has stopped naming "
                       "one bandwidth\n",
                       names[t], betas[b], ratio, TOL);
              fail = 1;
            }
        }
      printf ("\n");
    }

  /* The reciprocal the object actually installs, against the analytic slope
     it is supposed to be. Separate from the sweep above on purpose: that one
     validates the FORMULA, this one validates that create() reaches it with
     the right arguments -- a correct formula wired to the wrong pulse, beta
     or detector would pass the sweep and still mis-scale every loop. */
  printf ("  what ratesync_create() installs (sps 4, rrc, beta 0.35, "
          "span %d, m 2):\n",
          SPAN);
  for (size_t t = 0; t < 2; t++)
    {
      ratesync_state_t *rs = ratesync_create (
          4.0, RATESYNC_PULSE_RRC, 0.35, SPAN, 2, 1024, 0.01, 0.707, teds[t]);
      if (!rs)
        {
          fprintf (stderr, "  %s: create failed\n", names[t]);
          fail = 1;
          continue;
        }
      double installed = 1.0 / rs->loop.ted_scale;
      double want = symsync_ted_slope (teds[t], SYMSYNC_PULSE_RRC, 0.35, SPAN);
      printf ("    %-8s 1/ted_scale = %.6f   symsync_ted_slope = %.6f\n",
              names[t], installed, want);
      if (fabs (installed / want - 1.0) > 1e-9)
        {
          fprintf (stderr,
                   "  %s: create() installed a reciprocal of %.6f where the "
                   "detector's own slope is %.6f\n",
                   names[t], installed, want);
          fail = 1;
        }
      ratesync_destroy (rs);
    }

  if (check && fail)
    {
      fprintf (stderr, "ratesync_scurve FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: both detectors' normalisers match their own slope across "
            "beta, and create() installs each correctly\n");
  return 0;
}
