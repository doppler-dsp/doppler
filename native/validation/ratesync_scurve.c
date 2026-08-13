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
 *   - **No cascade.** The stimulus is `dp_tx_make()` with `DP_TX_RC` -- the
 *     raised cosine, which is the matched pair already collapsed, i.e. what
 *     a matched receiver hands its detector. At `sps = 2` the even samples
 *     are the on-time instants and the odd ones the half-symbol gate, so
 *     both detector inputs come off the shared stimulus with no resampling.
 *     What that buys is attribution: a discrepancy measured here is the
 *     detector's or the model's, and cannot be the polyphase bank's, the
 *     CIC's or the loop's.
 *   - **The raw numerator.** `gardner_ted` / `dttl_ted` are called directly,
 *     so the number compared against `symsync_ted_slope()` is the same
 *     quantity that function claims to return, not a normalised error read
 *     back through a binding.
 *   - **Across beta.** `symsync_ted_slope`'s own doxygen records that the
 *     shipped normalisation's slope varies 10.6x between beta 0.1 and 0.9.
 *     A single roll-off cannot see that; this sweeps the supported range.
 *
 * The measurement is a paired central difference AVERAGED OVER SEEDS: the
 * same symbol sequence drives `+d` and `-d` so the data-dependent part
 * cancels in the difference, and several independent sequences are then
 * averaged because what is left is still a draw. A TED's S-curve amplitude
 * carries the transition density of the stream driving it, and the design
 * assigns that to nobody because it is data -- an m-sequence moves Gardner's
 * slope 15% against i.i.d. symbols (F12). Every table below therefore prints
 * `sd/mean` across realizations beside the value, and both gates refuse to
 * run when the mean's standard error is not comfortably inside the tolerance
 * they are about to apply. A tolerance tighter than the measurement's own
 * scatter gates noise.
 *
 * It matters in practice, not just in principle: on one seed Gardner's
 * through-cascade agreement read 0.896 at beta 0.1, and over ten it reads
 * 0.944. The single-seed number was mostly data.
 *
 * Nothing here synthesises its own waveform. An earlier version rolled a
 * private xorshift for the symbols and a hand-written tap-array dot product
 * for the pulse; `dp_tx_test.h` exists precisely because "the same analytic
 * direct-form synthesis loop had been written three times", and its symbol
 * source is the library's own PRBS (`pn_core` via `wfm_synth_mls_poly`).
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
/* Symbols per realization. The pairing below removes most of the data
   variance and the seed average removes the rest, so this trades length
   for independent draws rather than spending everything on one. */
#define NSYM 20000
/* Independent symbol sequences averaged per point. One realization is a
   sample of a data-dependent quantity -- the S-curve amplitude carries the
   transition density, which is data (F12) -- so a single seed states a
   number the next seed will not reproduce. */
#define NSEED 8
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
/* Realizations averaged per point, as in phase 1. */
#define CASC_NSEED 10
/* Set from measurement, not chosen: see the printed table. */
#define CASC_GARDNER_TOL 0.12
#define CASC_DTTL_RATCHET 11.0

/**
 * Mean raw detector output over the shared stimulus, at timing offset
 * @p tau, with no cascade anywhere in the path.
 *
 * Driven by `dp_tx_make()` with `DP_TX_RC` -- the raised cosine, which is
 * the matched pair already collapsed, i.e. exactly what a matched receiver
 * hands its detector. At `sps = 2` the even samples are the on-time
 * instants and the odd ones the half-symbol gate, so the detector's two
 * inputs come straight off the shared stimulus with no resampling and no
 * cascade.
 *
 * This used to synthesise its own stream: a private xorshift for the
 * symbols and a hand-rolled tap-array dot product for the pulse. Both were
 * wrong to write. `dp_tx_test.h` exists because "the same analytic
 * direct-form synthesis loop had been written three times", and its symbol
 * source is the library's own PRBS (`pn_core` via `wfm_synth_mls_poly`) --
 * so a private one here is a fourth copy and a second random source, in a
 * directory the `no private RNG` rule does not currently scan.
 */
static double
s_curve_measured (int ted, double beta, double tau, uint32_t seed)
{
  dp_tx_cfg_t cfg = dp_tx_defaults ();
  cfg.pulse       = DP_TX_RC; /* TX*RX already collapsed */
  cfg.sps         = 2.0;      /* on-time + half-symbol gate, nothing else */
  cfg.beta        = beta;
  cfg.span        = SPAN;
  cfg.tau         = tau;
  cfg.nsym        = NSYM;
  cfg.seed        = seed;

  size_t          n = 0;
  float _Complex *x = dp_tx_make (&cfg, NULL, &n);
  if (!x)
    return 0.0;

  /* Symbol k's centre is `lead + k*sps` in samples (dp_tx_make's origin),
     and the gate is one sample -- half a symbol -- before it. */
  const size_t lead   = (size_t)((double)SPAN * cfg.sps);
  double       sum    = 0.0;
  long         cnt    = 0;
  double       prev_y = 0.0;
  int          have   = 0;
  for (size_t k = 1; k + 1 < NSYM; k++)
    {
      size_t i = lead + k * 2u;
      if (i >= n || i == 0)
        break;
      double y   = (double)crealf (x[i]);
      double mid = (double)crealf (x[i - 1]);
      if (have)
        {
          sum += (ted == SYMSYNC_TED_DTTL)
                     ? dttl_ted ((float)mid, (float)y, (float)prev_y)
                     : gardner_ted ((float)mid, (float)(y - prev_y));
          cnt++;
        }
      prev_y = y;
      have   = 1;
    }
  free (x);
  return cnt ? sum / (double)cnt : 0.0;
}

/* Measured |dS/dtau| at the lock point, paired so the data cancels. */
/* Slope from ONE realization. Paired: the same seed drives both offsets,
   so the data-dependent part of the S-curve cancels in the difference. */
static double
slope_one (int ted, double beta, uint32_t seed)
{
  double sp = s_curve_measured (ted, beta, DTAU, seed);
  double sm = s_curve_measured (ted, beta, -DTAU, seed);
  return fabs ((sp - sm) / (2.0 * DTAU));
}

/**
 * Mean slope over NSEED independent symbol sequences, with the seed-to-seed
 * spread reported alongside.
 *
 * The spread is not decoration. A TED's S-curve amplitude carries the
 * transition density of the stream driving it, and the design assigns that
 * to nobody because it is data (F12 in the RateSync report: an m-sequence
 * moves Gardner's slope 15% against i.i.d. symbols). So a slope from one
 * seed is a draw, not a constant, and any ratio built on it inherits that.
 * Quoting the spread is what makes the scaling factor below a measurement
 * with an uncertainty instead of a number the next seed contradicts.
 *
 * @param sd_out  receives the sample standard deviation across seeds.
 */
static double
slope_measured (int ted, double beta, double *sd_out)
{
  double v[NSEED], sum = 0.0;
  for (int i = 0; i < NSEED; i++)
    {
      /* Distinct, non-zero, and not a sequence the PRBS treats specially. */
      v[i] = slope_one (ted, beta, (uint32_t)(7u + 1000u * (unsigned)i));
      sum += v[i];
    }
  double mean = sum / (double)NSEED;
  double acc  = 0.0;
  for (int i = 0; i < NSEED; i++)
    acc += (v[i] - mean) * (v[i] - mean);
  if (sd_out)
    *sd_out = (NSEED > 1) ? sqrt (acc / (double)(NSEED - 1)) : 0.0;
  return mean;
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
  printf ("  %-8s %6s %12s %9s %12s %9s\n", "ted", "beta", "measured",
          "sd/mean", "declared", "scale");
  for (size_t t = 0; t < 2; t++)
    {
      for (size_t b = 0; b < nbeta; b++)
        {
          double sd   = 0.0;
          double meas = slope_measured (teds[t], betas[b], &sd);
          double decl
              = symsync_ted_slope (teds[t], SYMSYNC_PULSE_RRC, betas[b], SPAN);
          double ratio = (decl > 0.0) ? meas / decl : 0.0;
          double rsd   = (meas != 0.0) ? sd / fabs (meas) : 0.0;
          printf ("  %-8s %6.2f %12.6f %8.2f%% %12.6f %9.4f\n", names[t],
                  betas[b], meas, 100.0 * rsd, decl, ratio);
          /* A tolerance below the measurement's own scatter would gate
             noise. NSEED draws put the mean's standard error at
             sd/sqrt(NSEED); if that is not comfortably inside TOL the
             gate is not measuring what it claims to. */
          if (rsd / sqrt ((double)NSEED) > TOL / 3.0)
            {
              fprintf (stderr,
                       "  %s beta=%.2f: the seed-to-seed scatter (%.2f%%, "
                       "standard error %.2f%%) is too close to the %.0f%% "
                       "tolerance — raise NSEED or NSYM rather than the "
                       "tolerance\n",
                       names[t], betas[b], 100.0 * rsd,
                       100.0 * rsd / sqrt ((double)NSEED), 100.0 * TOL);
              fail = 1;
            }

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
  printf ("  %-8s %6s %12s %9s %12s %9s\n", "ted", "beta", "raw slope",
          "sd/mean", "declared", "scale");
  double worst_gardner = 0.0, dttl_lo = 1e30, dttl_hi = 0.0;
  for (size_t t = 0; t < 2; t++)
    {
      for (size_t b = 0; b < nbeta; b++)
        {
          const double taus[2] = { -CASC_DTAU, +CASC_DTAU };
          /* One realization per seed; the mean and its scatter below are
             what the scale factor is quoted from. */
          double slopes[CASC_NSEED];
          for (int sd_i = 0; sd_i < CASC_NSEED; sd_i++)
            {
              double mean[2] = { 0.0, 0.0 };
              for (int si = 0; si < 2; si++)
                {
                  dp_tx_cfg_t cfg  = dp_tx_defaults ();
                  cfg.sps          = 4.0;
                  cfg.beta         = betas[b];
                  cfg.span         = SPAN;
                  cfg.tau          = taus[si];
                  cfg.nsym         = CASC_NSYM;
                  cfg.seed         = (uint32_t)(7u + 1000u * (unsigned)sd_i);
                  size_t         n = 0;
                  float complex *x = dp_tx_make (&cfg, NULL, &n);
                  if (!x)
                    continue;
                  ratesync_state_t *rs
                      = ratesync_create (4.0, RATESYNC_PULSE_RRC, betas[b],
                                         SPAN, 2, 1024, 0.0, 0.707, teds[t]);
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
              slopes[sd_i] = fabs ((mean[1] - mean[0]) / (2.0 * CASC_DTAU));
            }
          double acc = 0.0;
          for (int i2 = 0; i2 < CASC_NSEED; i2++)
            acc += slopes[i2];
          double meas = acc / (double)CASC_NSEED;
          double var  = 0.0;
          for (int i2 = 0; i2 < CASC_NSEED; i2++)
            var += (slopes[i2] - meas) * (slopes[i2] - meas);
          double csd
              = (CASC_NSEED > 1) ? sqrt (var / (double)(CASC_NSEED - 1)) : 0.0;
          double decl
              = symsync_ted_slope (teds[t], SYMSYNC_PULSE_RRC, betas[b], SPAN);
          double ratio = (decl > 0.0) ? meas / decl : 0.0;
          printf ("  %-8s %6.2f %12.6f %8.2f%% %12.6f %9.4f\n", names[t],
                  betas[b], meas, 100.0 * (meas ? csd / meas : 0.0), decl,
                  ratio);
          /* The same rule phase 1 applies: a tolerance below the
             measurement's own scatter gates noise, not behaviour. */
          double crsd = meas ? csd / meas : 0.0;
          if (crsd / sqrt ((double)CASC_NSEED) > CASC_GARDNER_TOL / 3.0)
            {
              fprintf (stderr,
                       "  %s beta=%.2f: through-cascade scatter %.2f%% "
                       "(standard error %.2f%%) is too close to the %.0f%% "
                       "tolerance — raise CASC_NSEED or CASC_NSYM\n",
                       names[t], betas[b], 100.0 * crsd,
                       100.0 * crsd / sqrt ((double)CASC_NSEED),
                       100.0 * CASC_GARDNER_TOL);
              fail = 1;
            }
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
