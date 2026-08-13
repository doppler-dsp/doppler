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
#include "dp_tx_test.h"
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
/* Phase 3, through the cascade. A larger step than DTAU: the cascade's
   output carries the detector's self-noise, so the difference has to clear
   it, and a thirty-second of a symbol is still well inside every S-curve's
   linear region. */
#define CASC_DTAU 0.03125
#define CASC_NSYM 4000
#define CASC_SKIP 300
/* Set from measurement, not chosen: see the printed table. */
#define CASC_GARDNER_TOL 0.20
#define CASC_DTTL_RATCHET 11.0

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

  /* ── Phase 3: the same slope THROUGH the cascade ──────────────────────
   *
   * Phases 1 and 2 exonerate the formula and its wiring. This is the half
   * that does not agree with them, and it is here rather than in a
   * throwaway probe because a measurement quoted as evidence has to be
   * re-runnable: the numbers below are cited on gh-669.
   *
   * The raw numerator is recovered as `last_error / ted_scale` rather than
   * by recomputing the detector from `loop.ring[]`. That matters. The ring
   * advances per terminal OUTPUT, not per strobe, and one input can
   * complete two outputs, so reading it from outside `ratesync_step` is
   * occasionally one output stale -- a race that produced a confident and
   * wrong answer when this measurement lived in a scratch probe.
   * `last_error` is written inside `ratesync_loop_take_output` for the
   * strobe that just fired, so it needs no such assumption.
   *
   * Stimulus is dp_tx_make(), the shared harness stimulus the unit tests
   * drive -- not a pulse re-shaped here. A second implementation of the
   * transmitter is exactly the peer that drifts. */
  printf ("\n  through the cascade (bn = 0, sps 4, m 2, dp_tx stimulus):\n");
  printf ("  %-8s %6s %14s %14s %8s\n", "ted", "beta", "raw slope", "declared",
          "ratio");
  double worst_gardner = 0.0, dttl_lo = 1e30, dttl_hi = 0.0;
  for (size_t t = 0; t < 2; t++)
    {
      for (size_t b = 0; b < nbeta; b++)
        {
          double       mean[2] = { 0.0, 0.0 };
          const double taus[2] = { -CASC_DTAU, +CASC_DTAU };
          for (int si = 0; si < 2; si++)
            {
              dp_tx_cfg_t cfg  = dp_tx_defaults ();
              cfg.sps          = 4.0;
              cfg.beta         = betas[b];
              cfg.span         = SPAN;
              cfg.tau          = taus[si];
              cfg.nsym         = CASC_NSYM;
              size_t         n = 0;
              float complex *x = dp_tx_make (&cfg, NULL, &n);
              if (!x)
                continue;
              ratesync_state_t *rs
                  = ratesync_create (4.0, RATESYNC_PULSE_RRC, betas[b], SPAN,
                                     2, 1024, 0.0, 0.707, teds[t]);
              if (!rs)
                {
                  free (x);
                  continue;
                }
              double        sum  = 0.0;
              long          used = 0, cnt = 0;
              float complex sym;
              for (size_t i = 0; i < n; i++)
                if (ratesync_step (rs, x[i], &sym))
                  {
                    /* Discard the cascade's fill; the loop is open, so
                       there is no transient beyond that to wait out. */
                    if (++cnt > CASC_SKIP)
                      {
                        sum += rs->loop.last_error / rs->loop.ted_scale;
                        used++;
                      }
                  }
              mean[si] = used ? sum / (double)used : 0.0;
              ratesync_destroy (rs);
              free (x);
            }
          double meas = fabs ((mean[1] - mean[0]) / (2.0 * CASC_DTAU));
          double decl
              = symsync_ted_slope (teds[t], SYMSYNC_PULSE_RRC, betas[b], SPAN);
          double ratio = (decl > 0.0) ? meas / decl : 0.0;
          printf ("  %-8s %6.2f %14.6f %14.6f %8.4f\n", names[t], betas[b],
                  meas, decl, ratio);
          if (teds[t] == SYMSYNC_TED_GARDNER)
            {
              if (fabs (ratio - 1.0) > worst_gardner)
                worst_gardner = fabs (ratio - 1.0);
            }
          else
            {
              if (ratio < dttl_lo)
                dttl_lo = ratio;
              if (ratio > dttl_hi)
                dttl_hi = ratio;
            }
        }
    }

  /* Gardner's raw slope through the cascade agrees with the analytic one,
     which is what makes the DTTL row a finding rather than a property of
     the measurement. A real gate. */
  if (worst_gardner > CASC_GARDNER_TOL)
    {
      fprintf (stderr,
               "  gardner's THROUGH-CASCADE slope drifted %.3f from its "
               "declared one — phases 1 and 2 say the formula and the "
               "wiring are right, so this is the cascade\n",
               worst_gardner);
      fail = 1;
    }
  /* DTTL's does not, and by a factor that grows with roll-off. This is the
     open defect (F15, gh-669), so the gate is a RATCHET on how bad it is,
     and it may only ever shrink. It is NOT an endorsement: the correct
     value for this spread is 1.0. */
  double spread = (dttl_lo > 0.0) ? dttl_hi / dttl_lo : 0.0;
  printf ("  dttl through-cascade ratio %.2f..%.2f, spread %.1fx "
          "(ratchet %.1f; correct value 1.0 — gh-669)\n",
          dttl_lo, dttl_hi, spread, CASC_DTTL_RATCHET);
  if (spread > CASC_DTTL_RATCHET)
    {
      fprintf (stderr,
               "  dttl's roll-off dependence grew to %.1fx, past the %.1fx "
               "ratchet — the gh-669 defect got WORSE\n",
               spread, CASC_DTTL_RATCHET);
      fail = 1;
    }
  if (spread < 2.0)
    printf ("  NOTE: dttl's spread has collapsed toward 1.0 — if gh-669 was "
            "fixed, TIGHTEN the ratchet.\n");

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
