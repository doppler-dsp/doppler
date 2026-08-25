/*
 * test_acq_core.c — DSSS acquisition engine C-level tests.
 *
 * Covers: argument validation, physics auto-config (C/N0 -> snr, the chosen
 * coherent depth coherent_bins, threshold/eta) for both acq_create_burst()
 * and acq_create_continuous(), noise-free localization of a streamed burst
 * to the injected (Doppler bin, code phase), and a real AWGN calibration
 * check that acq_result_t::cn0_dbhz_est tracks a known injected C/N0.
 */
#include "acq/acq_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A length-7 maximal-length sequence (one period). */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* ── acq_run pure-transducer (state in/out) round-trip ─────────────────────
 * The stateless elastic face: state_in == NULL resets then processes; a fresh
 * engine + state_in reproduces an uninterrupted run; a corrupted blob is
 * rejected (acq_run returns 0). Driven at two explicitly PINNED grids (via
 * acq_configure_search_raw -- deterministic, not left to the auto-sizer's own
 * physics-driven choice, since there's no caller-facing max_noncoh knob left
 * to lean on) so both the coherent and the non-coherent (nc_surface)
 * serialization paths are covered: @p n_noncoh_pin == 1 (coherent-only) vs.
 * > 1 (exercises nc_surface). @p s0d is the oversampled, code-phase-rolled
 * BPSK replica (length @p nx). */
static int
_acq_run_roundtrip (const float complex *s0d, size_t nx, size_t spc,
                    double crate, double cn0, size_t n_noncoh_pin)
{
  const double PI = acos (-1.0);

  acq_state_t *ra
      = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9, 0);
  DP_CHECK (ra != NULL);
  if (!ra)
    return 0;
  DP_CHECK (acq_configure_search_raw (ra, 8, n_noncoh_pin) == 0);
  DP_CHECK (ra->n_noncoh == n_noncoh_pin);

  const size_t rn = ra->n;
  /* A non-coherent dump lands every n_noncoh frames; size for >= 2 dumps. */
  const size_t L   = (2 * ra->n_noncoh + 1) * rn + 5;
  const size_t cut = rn + rn / 2; /* split mid first accumulation         */
  const double rf  = 1.0 / (double)rn; /* inject Doppler bin u = 1 */

  float complex *s = malloc (L * sizeof (float complex));
  for (size_t k = 0; k < L; k++)
    {
      double ph = 2.0 * PI * rf * (double)k;
      s[k]      = s0d[k % nx] * (float complex) (cos (ph) + I * sin (ph));
    }

  /* reference: the whole stream via acq_run, state_in == NULL (-> reset). */
  acq_result_t hA[16];
  size_t       nA = acq_run (ra, NULL, NULL, s, L, hA, 16);
  acq_destroy (ra);

  /* split: engine1 emits state_out; a fresh engine2 restores it via state_in.
   */
  acq_state_t *r1
      = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9, 0);
  acq_state_t *r2
      = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9, 0);
  DP_CHECK (r1 && r2);
  if (r1 && r2)
    {
      DP_CHECK (acq_configure_search_raw (r1, 8, n_noncoh_pin) == 0);
      DP_CHECK (acq_configure_search_raw (r2, 8, n_noncoh_pin) == 0);
      size_t       cb   = acq_state_bytes (r1);
      void        *blob = malloc (cb);
      acq_result_t hB[16];
      size_t       nB = acq_run (r1, NULL, blob, s, cut, hB, 16);
      nB += acq_run (r2, blob, NULL, s + cut, L - cut, hB + nB, 16 - nB);

      DP_CHECK (nA >= 1 && nB == nA);
      for (size_t i = 0; i < nA && i < nB; i++)
        {
          DP_CHECK (hA[i].doppler_bin == hB[i].doppler_bin);
          DP_CHECK (hA[i].code_phase == hB[i].code_phase);
          DP_CHECK (fabsf (hA[i].test_stat - hB[i].test_stat) < 1e-4f);
        }

      /* a corrupted blob must make acq_run reject (set_state != 0) -> 0 out.
       */
      acq_state_t *r3
          = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9, 0);
      DP_CHECK (acq_configure_search_raw (r3, 8, n_noncoh_pin) == 0);
      acq_get_state (r3, blob);
      ((char *)blob)[0] ^= (char)0xFF; /* clobber the state header magic */
      DP_CHECK (acq_run (r3, blob, NULL, s, cut, hB, 16) == 0);
      acq_destroy (r3);

      free (blob);
    }
  acq_destroy (r1);
  acq_destroy (r2);
  free (s);
  return 0;
}

/* C/N0 calibration: acq_result_t::cn0_dbhz_est should track a known injected
 * C/N0 while AWGN dominates the CFAR noise estimate -- the entire point of
 * reporting a bandwidth-normalised C/N0 instead of a raw per-sample or
 * coherently-integrated ratio (both scale with spc/reps and so aren't
 * portable across configurations). A 31-chip MLS code at 4x oversample, 16
 * coherent reps (the header's own @code example geometry) keeps the code's
 * autocorrelation-sidelobe floor well below a 55 dB-Hz injected AWGN floor,
 * so the estimate should land within a couple dB of truth -- previously
 * nothing checked this field (formerly snr_est) against ground truth, only
 * finiteness and cross-call determinism. */
static int
_acq_cn0_calibration (void)
{
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc      = 4;
  const double crate    = 1.0e6;
  const double fs       = crate * (double)spc;
  const double cn0_true = 55.0;

  acq_state_t *a
      = acq_create_burst (CODE31, 31, 16, spc, crate, 45.0, 0.0, 1e-3, 0.9, 0);
  DP_CHECK (a != NULL);
  if (!a)
    return 0;
  /* Pin coherent-only (n_noncoh == 1): the test below pushes exactly one
   * frame expecting exactly one immediate dump. */
  DP_CHECK (acq_configure_search_raw (a, 16, 1) == 0);

  const size_t   n = a->n; /* coherent_bins * code_bins */
  float complex *x = malloc (n * sizeof (float complex));
  DP_CHECK (x != NULL);
  if (!x)
    {
      acq_destroy (a);
      return 0;
    }

  /* Exact inverse of the sizing transform: amp_snr = sqrt(C/N0 / fs); sigma
   * is the total complex noise RMS matching a unit chip amplitude. */
  const double amp_snr = sqrt (pow (10.0, cn0_true / 10.0) / fs);
  const float  sigma   = (float)(1.0 / amp_snr);
  uint32_t     st      = 12345u;
  for (size_t k = 0; k < n; k++)
    {
      uint8_t chip = CODE31[(k / spc) % 31];
      float   c    = (chip & 1u) ? -1.0f : 1.0f; /* unit chip amplitude */
      x[k]         = c + sigma * dp_cgauss (&st);
    }

  acq_result_t hits[4];
  size_t       nh = acq_push (a, x, n, hits, 4);
  DP_CHECK (nh == 1);
  if (nh == 1)
    DP_CHECK (fabsf (hits[0].cn0_dbhz_est - (float)cn0_true) < 3.0f);

  free (x);
  acq_destroy (a);
  return 0;
}

/* configure_search_raw: the advanced escape hatch. Bounds violations leave
 * the engine untouched at its prior grid; a valid pin resizes every
 * grid-dependent buffer/plan, re-derives the threshold ladder, and the
 * result actually detects a noise-free burst at that geometry. */
static int
_acq_configure_search_raw_check (void)
{
  const size_t spc = 2;

  acq_state_t *a
      = acq_create_burst (CODE7, 7, 8, spc, 1.0e6, 45.0, 0.0, 1e-2, 0.9, 0);
  DP_CHECK (a != NULL);
  if (!a)
    return 0;

  size_t orig_db = a->coherent_bins, orig_nc = a->n_noncoh;

  DP_CHECK (acq_configure_search_raw (a, 0, 1) == -1); /* doppler_bins < 1 */
  DP_CHECK (acq_configure_search_raw (a, 9, 1) == -1); /* > reps (8) */
  DP_CHECK (acq_configure_search_raw (a, 1, 0) == -1); /* n_noncoh < 1 */
  DP_CHECK (acq_configure_search_raw (a, 1, ACQ_N_NONCOH_SAFETY_CEILING + 1)
            == -1); /* > the internal safety-valve ceiling */
  DP_CHECK (a->coherent_bins == orig_db && a->n_noncoh == orig_nc);

  DP_CHECK (acq_configure_search_raw (a, 3, 2) == 0);
  DP_CHECK (a->coherent_bins == 3 && a->n_noncoh == 2);
  DP_CHECK (a->n == 3 * a->code_bins);
  DP_CHECK (a->eta_nc > 0.0f && a->threshold == 0.0f);

  const size_t   n     = a->n;
  float complex *burst = malloc (2 * n * sizeof (float complex));
  DP_CHECK (burst != NULL);
  if (burst)
    {
      for (size_t k = 0; k < 2 * n; k++)
        {
          uint8_t chip = CODE7[(k / spc) % 7];
          burst[k]     = (chip & 1u) ? -1.0f : 1.0f;
        }
      acq_result_t hits[4];
      size_t       nh = acq_push (a, burst, 2 * n, hits, 4);
      DP_CHECK (nh == 1);
      if (nh == 1)
        {
          DP_CHECK (hits[0].doppler_bin == 0);
          DP_CHECK (hits[0].code_phase == 0);
        }
      free (burst);
    }

  acq_destroy (a);
  return 0;
}

/* Wideband mode (doppler_uncertainty > the native span): coherent_bins is
 * forced to 1 and the uncertainty is tiled with window_bins parallel
 * roll-FFT frequency-window hypotheses (see acq_core.h's file doc comment,
 * and the frequency-bank benchmark for the roll-vs-bank comparison that
 * settled roll-FFT over a tuned-mixer bank). One noise-free epoch (frame_n
 * == code_bins here, not a multi-epoch tile like the native localization
 * test above needs) should localize to the injected (frequency window, code
 * phase) -- including a NEGATIVE frequency window, which exercises the
 * modulo-nx wraparound the roll amount needs (a naive modulo-window_bins
 * roll would silently fold negative windows back onto the positive side).
 * acq_configure_search_raw can't pin a wideband grid (it always exits
 * wideband mode, per its own documented contract -- see the exit check
 * below), so nc=1 here comes from a deliberately very strong cn0_dbhz
 * (rather than a caller cap, which no longer exists) making the auto-sizer's
 * n_noncoh ascend land on 1 -- the actual pushed burst is noise-free anyway,
 * so cn0_dbhz only steers the SIZING decision, not detectability. */
/* gh-1002: a burst at exactly HALF a coherent Doppler bin must still be
 * detected.
 *
 * The slow-time FFT's scalloping loss is ~3.9 dB at the worst case, exactly
 * between two bins -- and that was not a margin a caller could buy back with
 * signal, because `test_stat` saturates against the code's own
 * autocorrelation-sidelobe floor. A half-bin burst was therefore invisible
 * at ANY C/N0. The engine now zero-pads its column transform, so the surface
 * is sampled between the native bins too.
 *
 * The geometry is chosen so the defect actually EXISTS here: a 31-chip code
 * at reps=4 buys a coherent depth of 4, which is where the slow-time null is
 * sharp. The first attempt used the 7-chip code the rest of this file shares
 * (coherent depth 2) and passed with the fix reverted -- a regression test
 * that cannot fail is decoration, and the structural `interp > 1` assert
 * below would have hidden that had it been the only check.
 *
 * The sweep runs the ends as well as the half: half a bin alone would pass
 * against an engine that had merely lowered its threshold. The claim is a
 * FLAT response across the bin. */
static int
_acq_half_bin_check (void)
{
  const double   PI  = acos (-1.0);
  const size_t   spc = 4, sf = 31, reps = 4;
  const double   crate = 1.0e6;
  static uint8_t code31[31];
  for (size_t i = 0; i < sf; i++)
    code31[i] = (uint8_t)(((i * 2654435761u) >> 13) & 1u);

  acq_state_t *a = acq_create_burst (code31, sf, reps, spc, crate, 55.0, 0.0,
                                     1e-3, 0.9, 0);
  DP_CHECK (a != NULL);
  if (!a)
    return 0;

  /* The depth that makes the null sharp -- asserted so a future sizing
     change cannot quietly move this test off the geometry it needs. */
  DP_CHECK (a->coherent_bins == reps);
  DP_CHECK (a->interp > 1);
  DP_CHECK (a->n_surf == a->n * a->interp);

  const size_t nx  = sf * spc;        /* code_bins                   */
  const size_t n   = reps * nx;       /* one frame = the preamble    */
  const double res = 1.0 / (double)n; /* one Doppler bin, cyc/sample */

  /* A whole DWELL, not one frame: at this C/N0 the sizer may buy
     non-coherent looks, and a single frame then never completes one -- the
     first version of this test read that as "not detected" and failed with
     the fix in place. */
  const size_t looks = a->n_noncoh;
  DP_REQUIRE (looks >= 1 && looks <= 8);
  const size_t n_tot = looks * n;

  static float complex frame[8 * 4 * 31 * 4];
  for (int q = 0; q <= 4; q++) /* 0, 1/4, 1/2, 3/4, 1 bin */
    {
      double f_norm = 0.25 * (double)q * res;
      acq_reset (a);
      for (size_t k = 0; k < n_tot; k++)
        {
          uint8_t chip = code31[((k % nx) / spc) % sf];
          double  s    = (chip & 1u) ? -1.0 : 1.0;
          double  ph   = 2.0 * PI * f_norm * (double)k;
          frame[k]     = (float complex) (s * cos (ph) + I * s * sin (ph));
        }

      acq_result_t hits[8];
      size_t       nh = acq_push (a, frame, n_tot, hits, 8);
      DP_CHECK_MSG (nh >= 1, "no detection at this quarter-bin offset");
      if (nh >= 1)
        {
          /* Reported on the NATIVE grid whatever the surface resolved: the
             half-bin case may legitimately name either neighbour, but never
             a row that does not exist on the caller's grid. */
          DP_CHECK (hits[0].doppler_bin < a->coherent_bins);
          DP_CHECK (hits[0].test_stat > a->threshold);
        }
    }

  acq_destroy (a);
  return 0;
}

static int
_acq_wideband_check (void)
{
  const double PI    = acos (-1.0);
  const size_t spc   = 2;
  const size_t nx    = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate = 1.0e6;
  const double span  = crate / (2.0 * 7.0);

  /* 3.5 * span -> window_bins = ceil(3.5) = 4 (even, so window_bins/2 = 2
   * lands exactly on the convention's positive/negative boundary). */
  acq_state_t *w = acq_create_burst (CODE7, 7, 8, spc, crate, 90.0 /* strong:
                                     forces n_noncoh=1 -- see doc above */
                                     ,
                                     3.5 * span, 1e-2, 0.9, 0);
  DP_CHECK (w != NULL);
  if (!w)
    return 0;

  DP_CHECK (w->coherent_bins == 1);
  /* Coverage-sized and ODD: du = 3.5*span, bins are spaced doppler_res_hz =
     2*span apart, so each side needs ceil((3.5 - 0.5)*span / (2*span)) = 2
     hypotheses beyond DC -> 5.  Odd is required, not incidental: an even
     count folds asymmetrically and puts a bin at exactly n/2, the one index
     the search and acq_build_handoff() used to read with opposite signs. */
  DP_CHECK (w->window_bins == 5);
  DP_CHECK (w->window_bins % 2 == 1);
  DP_CHECK (w->n == w->window_bins * nx);
  DP_CHECK (w->frame_n == nx); /* one epoch, not window_bins epochs */
  DP_CHECK (w->n_noncoh == 1);
  DP_CHECK (w->searched_bins == w->window_bins);
  DP_CHECK (fabs (w->doppler_res_hz - crate / 7.0) < 1e-6); /* chip_rate/sf */

  /* row r=3 -> signed_r = 3 - 4 = -1 (one window NEGATIVE of DC): the
   * wraparound case. */
  const size_t row      = 3;
  const long   signed_r = (long)row - (long)w->window_bins;
  const double f_norm   = (double)signed_r / (double)nx; /* cycles/sample */
  const size_t d        = 5;                             /* code phase */

  float complex s0d[14];
  for (size_t q = 0; q < nx; q++)
    {
      size_t  src  = (q + nx - (d % nx)) % nx; /* roll by +d */
      uint8_t chip = CODE7[(src / spc) % 7];
      s0d[q]       = (chip & 1u) ? -1.0f : 1.0f;
    }
  float complex burst[14];
  for (size_t k = 0; k < nx; k++)
    {
      double ph = 2.0 * PI * f_norm * (double)k;
      burst[k]  = s0d[k] * (float complex) (cos (ph) + I * sin (ph));
    }

  acq_result_t hits[8];
  size_t       nh = acq_push (w, burst, nx, hits, 8);
  DP_CHECK (nh == 1); /* one epoch -> one dump */
  if (nh == 1)
    {
      DP_CHECK (hits[0].doppler_bin == row);
      DP_CHECK (hits[0].code_phase == d);
      DP_CHECK (hits[0].test_stat > w->threshold);
      DP_CHECK (isfinite (hits[0].cn0_dbhz_est)
                && hits[0].cn0_dbhz_est > 0.0f);
    }

  /* configure_search_raw always exits wideband mode back to the native
   * (doppler_bins, n_noncoh) grid, per its documented contract. */
  DP_CHECK (acq_configure_search_raw (w, 2, 1) == 0);
  DP_CHECK (w->window_bins == 1 && w->coherent_bins == 2);

  acq_destroy (w);
  return 0;
}

/* The wideband grid's real contract is COVERAGE, not bin count: every Doppler
 * in [-du, +du] must have a hypothesis within half a bin, and the search and
 * acq_build_handoff() must agree on which one.
 *
 * This is the regression test for the bug that motivated dp_fftfreq_index().
 * The two used to carry separate fold formulas that disagreed at exactly one
 * index -- bin == window_bins/2, reachable only for an EVEN window_bins. The
 * search read that row as +n/2 and the handoff as -n/2, so a true Doppler
 * landing there was reported with the wrong sign and a full-span magnitude
 * error (at SPEC.md's geometry: +50 kHz truth -> -51 kHz estimate, 101 kHz
 * off, while the receiver still reported tracking == 1).
 *
 * Asserting bin counts would not have caught it; asserting coverage does. */
static int
_acq_wideband_coverage_check (void)
{

  /* SPEC.md's own geometry -- span 1500 Hz, doppler_res_hz 3000 Hz, and a
     +/-50 kHz uncertainty whose maximum is exactly where the bug lived. */
  const size_t sf = 1023, spc = 2;
  const double crate = 3.069e6, du = 50000.0;
  uint8_t     *code = malloc (sf);
  DP_CHECK (code != NULL);
  if (!code)
    return 0;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(i & 1u);

  acq_state_t *w = acq_create_continuous (code, sf, spc, crate, 2700.0, 44.31,
                                          du, 1e-3, 0.9, 0);
  DP_CHECK (w != NULL);
  if (!w)
    {
      free (code);
      return 0;
    }

  /* Odd, so the fold is symmetric and no ambiguous n/2 index exists. */
  DP_CHECK (w->window_bins % 2 == 1);

  const double res  = w->doppler_res_hz;
  const long   kmax = (long)(w->window_bins / 2);

  /* Reach must cover the requested uncertainty on BOTH sides. The old sizing
     reached [-48000, +51000] for this config -- symmetric on paper, short by
     2 kHz at -50 kHz in practice. */
  DP_CHECK ((double)kmax * res >= du - 0.5 * res);

  /* Every bin round-trips: the search's row->signed mapping and the handoff's
     bin->signed mapping are now the same function, so this holds by
     construction -- the assertion documents the invariant and fails loudly if
     either side ever grows a private copy again. */
  for (size_t b = 0; b < w->window_bins; b++)
    {
      long k = dp_fftfreq_index (b, w->window_bins);
      DP_CHECK (k >= -kmax && k <= kmax);
      acq_result_t  hit = { .doppler_bin = b, .code_phase = 0 };
      acq_handoff_t ho;
      acq_build_handoff (w, &hit, sf, spc, &ho);
      DP_CHECK (fabs (ho.doppler_hz_est - (double)k * res) < 1e-6);
    }

  /* Coverage sweep: every true Doppler across the band has a hypothesis
     within half a bin. */
  for (double truth = -du; truth <= du + 1.0; truth += res / 4.0)
    {
      double best = 1e300;
      for (size_t b = 0; b < w->window_bins; b++)
        {
          double hz = (double)dp_fftfreq_index (b, w->window_bins) * res;
          double e  = fabs (hz - truth);
          if (e < best)
            best = e;
        }
      DP_CHECK (best <= 0.5 * res + 1e-6);
    }

  acq_destroy (w);
  free (code);
  return 0;
}

/* acq_create_continuous: ALWAYS window-tiles, even when doppler_uncertainty
 * is narrower than one native span -- unlike acq_create_burst's wideband
 * fallback (only gated on du > span), this is unconditional and never
 * attempts coherent multi-epoch combining at all (coherent_bins pinned to 1
 * regardless of du). This is the one behavior with zero prior coverage
 * (today's wideband path used to be exercised only via du > span). */
static int
_acq_continuous_check (void)
{
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc      = 4;
  const double crate    = 1.0e6;
  const double sf       = 31.0;
  const double span     = crate / (2.0 * sf);
  const double sym_rate = 2700.0;
  const double cn0_dbhz = 55.0;

  /* du == 0 (no uncertainty prior at all): still window-tiled at
   * window_bins == 1 -- native span, single window -- never a coherent
   * axis. */
  acq_state_t *narrow = acq_create_continuous (
      CODE31, 31, spc, crate, sym_rate, cn0_dbhz, 0.0, 1e-3, 0.9, 0);
  DP_CHECK (narrow != NULL);
  if (narrow)
    {
      DP_CHECK (narrow->coherent_bins == 1);
      DP_CHECK (narrow->window_bins == 1);
      acq_destroy (narrow);
    }

  /* 0 < du <= span: still window-tiled (window_bins == 1, same as above) --
   * the point being it's the SAME mechanism/formula as the du > span case
   * below, not a different code path. */
  acq_state_t *within = acq_create_continuous (
      CODE31, 31, spc, crate, sym_rate, cn0_dbhz, 0.5 * span, 1e-3, 0.9, 0);
  DP_CHECK (within != NULL);
  if (within)
    {
      DP_CHECK (within->coherent_bins == 1);
      DP_CHECK (within->window_bins == 1);
      acq_destroy (within);
    }

  /* du > span: window_bins tiles the uncertainty, exactly like
   * acq_create_burst's wideband fallback -- coherent_bins stays pinned at 1
   * either way. */
  acq_state_t *wide = acq_create_continuous (
      CODE31, 31, spc, crate, sym_rate, cn0_dbhz, 3.5 * span, 1e-3, 0.9, 0);
  DP_CHECK (wide != NULL);
  if (wide)
    {
      DP_CHECK (wide->coherent_bins == 1);
      DP_CHECK (wide->window_bins == 5); /* covers +/-3.5*span, odd */
      DP_CHECK (wide->window_bins % 2 == 1);
      DP_CHECK (wide->symbol_rate == sym_rate);
      DP_CHECK (fabs (wide->epochs_per_symbol - (crate / sf) / sym_rate)
                < 1e-6);
      acq_destroy (wide);
    }

  return 0;
}

int
main (void)
{
  const double PI = acos (-1.0);

  const size_t spc   = 2;
  const size_t nx    = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate = 1.0e6;   /* 1 MHz chips */
  const double span  = crate / (2.0 * 7.0);

  /* ── argument validation ────────────────────────────────────────────── */
  DP_CHECK (acq_create_burst (NULL, 0, 8, spc, crate, 45.0, 0.0, 1e-3, 0.9, 0)
            == NULL);
  DP_CHECK (acq_create_burst (CODE7, 7, 8, spc, 0.0, 45.0, 0.0, 1e-3, 0.9, 0)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (acq_create_burst (CODE7, 7, 8, spc, crate, 0.0, 0.0, 1e-3, 0.9, 0)
            == NULL); /* cn0_dbhz <= 0 */
  DP_CHECK (acq_create_burst (CODE7, 7, 8, spc, crate, 45.0, 0.0, 0.0, 0.9, 0)
            == NULL); /* pfa out of range */
  /* doppler_uncertainty > span used to be rejected; it now engages wideband
   * mode instead (see _acq_wideband_check below) -- must succeed here. */
  {
    acq_state_t *wide = acq_create_burst (CODE7, 7, 8, spc, crate, 45.0,
                                          span * 2.0, 1e-3, 0.9, 0);
    DP_CHECK (wide != NULL);
    if (wide)
      {
        DP_CHECK (wide->coherent_bins == 1);
        DP_CHECK (wide->window_bins == 3); /* covers +/-2*span, odd */
        DP_CHECK (wide->window_bins % 2 == 1);
        acq_destroy (wide);
      }
  }

  /* ── auto-config: a strong C/N0 needs only one coherent rep ──────────── */
  acq_state_t *a
      = acq_create_burst (CODE7, 7, 8, spc, crate, 65.0, 0.0, 1e-2, 0.9, 0);
  DP_CHECK (a != NULL);
  if (!a)
    return 1;
  DP_CHECK (a->sf == 7);
  DP_CHECK (a->code_bins == nx);
  /* Sizing averages Pd over the straddle priors (Jensen-honest): with a
   * 7-chip code the loss tail is heavy enough that one rep's AVERAGE Pd
   * falls short of 0.9 even at 65 dB-Hz, so the engine buys a second. */
  DP_CHECK (a->coherent_bins == 2);
  DP_CHECK (a->n == a->coherent_bins * nx);
  DP_CHECK (!a->underpowered && a->pd_predicted >= 0.9);
  /* The CFAR reference spans the SURFACE, which is the interpolated grid
     the peak search runs over -- not the native cell count `n`. The two
     differ by `interp` since gh-1002; asserting `n - 1` here would pin the
     reference to a fraction of the cells it actually reads. */
  DP_CHECK (a->n_surf == a->n * a->interp);
  DP_CHECK (a->noise_lo == 0 && a->noise_hi == a->n_surf - 1);
  DP_CHECK (a->fs == crate * (double)spc);
  DP_CHECK (fabs (a->doppler_span_hz - span) < 1e-6);
  /* threshold = eta * sqrt(2/pi); eta = sqrt(-2 ln pfa_cell) > 0 */
  DP_CHECK (a->eta > 0.0f);
  DP_CHECK (fabsf (a->threshold - a->eta * 0.7978845608f) < 1e-4f);
  acq_destroy (a);

  /* ── noise-free localization (force a multi-bin Doppler axis) ─────────── */
  /* A very weak target C/N0 makes the D-search exhaust to reps, so
   * coherent_bins == reps and the slow-time axis has bins to localize on.
   * The injected burst is noise-free, so it clears the (best-effort) gate
   * anyway. Pinned explicitly to n_noncoh=1 (rather than left to the
   * auto-sizer's non-coherent fallback, which would now ascend past 1 with
   * no caller cap to stop it) since the test below pushes exactly one frame
   * expecting exactly one immediate dump. */
  acq_state_t *b
      = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0);
  DP_CHECK (b != NULL);
  if (!b)
    return 1;
  DP_CHECK (b->coherent_bins == 8); /* exhausted to reps */
  DP_CHECK (acq_configure_search_raw (b, 8, 1) == 0);
  const size_t ny = b->coherent_bins;
  const size_t n  = b->n; /* ny * nx */

  const size_t u = 1;                             /* Doppler bin           */
  const size_t d = 5;                             /* code phase (samples)  */
  const double f = (double)u / (double)(nx * ny); /* carrier, f*nx*ny = u  */

  /* Oversampled, code-phase-rolled BPSK replica (one segment, length nx). */
  float complex s0d[14];
  for (size_t q = 0; q < nx; q++)
    {
      size_t  src  = (q + nx - (d % nx)) % nx; /* roll by +d */
      uint8_t chip = CODE7[(src / spc) % 7];
      s0d[q]       = (chip & 1u) ? -1.0f : 1.0f;
    }

  /* Tile ny segments with the continuous carrier; push the raw frame. */
  float complex *burst = malloc (n * sizeof (float complex));
  for (size_t k = 0; k < n; k++)
    {
      double ph = 2.0 * PI * f * (double)k;
      burst[k]  = s0d[k % nx] * (float complex) (cos (ph) + I * sin (ph));
    }

  acq_result_t hits[8];
  size_t       nh = acq_push (b, burst, n, hits, 8);
  DP_CHECK (nh == 1); /* one frame -> one dump */
  if (nh == 1)
    {
      DP_CHECK (hits[0].doppler_bin == u);
      DP_CHECK (hits[0].code_phase == d);
      DP_CHECK (hits[0].test_stat > b->threshold);
      DP_CHECK (isfinite (hits[0].cn0_dbhz_est)
                && hits[0].cn0_dbhz_est > 0.0f);
    }

  /* reset drains the ring and clears the accumulator. */
  acq_reset (b);

  free (burst);
  acq_destroy (b);
  acq_destroy (NULL); /* must not crash */

  /* ── state round-trip: split a stream across two engines ─────────────────
   * A fresh engine + the state blob must reproduce an uninterrupted run
   * exactly — the elastic-resume (pod handoff) guarantee. */
  {
    acq_state_t *ra
        = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0);
    DP_CHECK (ra != NULL);
    if (ra)
      {
        DP_CHECK (acq_configure_search_raw (ra, 8, 1) == 0); /* pin nc=1 */
        const size_t rn  = ra->n;       /* frame size (ny*nx)            */
        const size_t L3  = 3 * rn + 5;  /* 3 full frames + a partial tail */
        const size_t cut = rn + rn / 2; /* split mid-frame (1.5 frames)   */
        const double rf  = 1.0 / (double)rn; /* Doppler bin u=1 (rf*rn = 1)  */

        float complex *s = malloc (L3 * sizeof (float complex));
        for (size_t k = 0; k < L3; k++)
          {
            double ph = 2.0 * PI * rf * (double)k;
            s[k] = s0d[k % nx] * (float complex) (cos (ph) + I * sin (ph));
          }

        /* Run A — uninterrupted. */
        acq_result_t hA[8];
        size_t       nA = acq_push (ra, s, L3, hA, 8);

        /* Run B — engine1 takes [0,cut), hands its state to a fresh engine2
         * which takes [cut,L3). */
        acq_state_t *r1 = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0,
                                            1e-2, 0.9, 0);
        acq_state_t *r2 = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0,
                                            1e-2, 0.9, 0);
        DP_CHECK (r1 && r2);
        if (r1 && r2)
          {
            DP_CHECK (acq_configure_search_raw (r1, 8, 1) == 0);
            DP_CHECK (acq_configure_search_raw (r2, 8, 1) == 0);
            acq_result_t hB[8];
            size_t       nB = acq_push (r1, s, cut, hB, 8);

            size_t cb   = acq_state_bytes (r1);
            void  *blob = malloc (cb);
            acq_get_state (r1, blob);
            DP_CHECK (acq_set_state (r2, blob) == DP_OK);
            /* standard envelope: a magic-clobbered blob is rejected directly,
             * r2 left untouched (validate runs before any mutation). */
            ((char *)blob)[0] ^= (char)0xFF;
            DP_CHECK (acq_set_state (r2, blob) == DP_ERR_INVALID);
            ((char *)blob)[0] ^= (char)0xFF;

            nB += acq_push (r2, s + cut, L3 - cut, hB + nB, 8 - nB);

            DP_CHECK (nA == 3 && nB == nA); /* both see all 3 full frames */
            for (size_t i = 0; i < nA && i < nB; i++)
              {
                DP_CHECK (hA[i].doppler_bin == hB[i].doppler_bin);
                DP_CHECK (hA[i].code_phase == hB[i].code_phase);
                DP_CHECK (fabsf (hA[i].peak_mag - hB[i].peak_mag) < 1e-5f);
                DP_CHECK (fabsf (hA[i].test_stat - hB[i].test_stat) < 1e-5f);
                DP_CHECK (fabsf (hA[i].cn0_dbhz_est - hB[i].cn0_dbhz_est)
                          < 1e-5f);
              }
            free (blob);
          }
        acq_destroy (r1);
        acq_destroy (r2);
        free (s);
      }
    acq_destroy (ra);
  }

  /* acq_run pure-transducer round-trip at two explicitly pinned grids:
   * n_noncoh_pin == 1 (coherent-only) and > 1, the latter covering the
   * nc_surface serialize/restore paths. */
  (void)_acq_run_roundtrip (s0d, nx, spc, crate, 20.0, 1);
  (void)_acq_run_roundtrip (s0d, nx, spc, crate, 30.0, 8);

  (void)_acq_cn0_calibration ();
  (void)_acq_configure_search_raw_check ();
  (void)_acq_half_bin_check ();
  (void)_acq_wideband_check ();
  (void)_acq_wideband_coverage_check ();
  (void)_acq_continuous_check ();

  /* ── samples_consumed: a per-hit anchor, not a per-call one ───────────
   *
   * acq_result_t::samples_consumed is documented as "the raw sample offset
   * (since this engine's own stream start) this detection's epoch ended
   * at ... the per-hit anchor a caller needs to derive a precise timestamp
   * instead of reusing one message-level timestamp for every hit -- a
   * single push() call spanning multiple epochs can emit several hits at
   * different sample offsets."
   *
   * That is a specific claim about the SHAPE of the answer, and it had zero
   * mentions in this file and zero in test_acq.py. The failure it guards
   * against is quiet and plausible: stamping every hit from one call with
   * the same offset still produces detections in the right place, and only
   * a caller correlating hits to wall-clock would ever notice.
   *
   * One push, many epochs, and the offsets must be strictly increasing and
   * land on epoch boundaries. */
  {
    const size_t sf = 7, spcl = 2, nxl = sf * spcl;
    acq_state_t *a = acq_create_burst (CODE7, sf, 8, spcl, 1.0e6, 60.0, 0.0,
                                       1e-2, 0.9, 0);
    DP_CHECK (a != NULL);
    if (a)
      {
        const size_t   eps = 24;
        float complex *x   = malloc (eps * nxl * sizeof *x);
        DP_CHECK (x != NULL);
        if (x)
          {
            for (size_t k = 0; k < eps * nxl; k++)
              {
                uint8_t chip = CODE7[((k % nxl) / spcl) % sf];
                x[k]         = (chip & 1u) ? -1.0f : 1.0f;
              }
            /* Pinned for the same reason as the noise-mode section
               below: an auto-sized n_noncoh > 1 makes one dump span
               several frames, and the anchor stride would then be a
               multiple of that rather than of a->n. Pinning makes the
               stride claim exact instead of approximately right. */
            DP_CHECK (acq_configure_search_raw (a, 8, 1) == 0);
            acq_result_t hits[32];
            size_t       nh = acq_push (a, x, eps * nxl, hits, 32);
            DP_CHECK (nh >= 2); /* several epochs in ONE call */
            for (size_t i = 1; i < nh; i++)
              {
                /* strictly increasing -- not one stamp reused */
                DP_CHECK (hits[i].samples_consumed
                          > hits[i - 1].samples_consumed);
                /* and the stride is a whole number of frames */
                uint64_t d
                    = hits[i].samples_consumed - hits[i - 1].samples_consumed;
                DP_CHECK (d % (uint64_t)a->n == 0);
              }
            if (nh >= 1)
              {
                /* the first anchor is one frame in, not zero */
                DP_CHECK (hits[0].samples_consumed >= (uint64_t)a->n);
                /* and no anchor runs past what was pushed */
                DP_CHECK (hits[nh - 1].samples_consumed
                          <= (uint64_t)(eps * nxl));
              }
            free (x);
          }
        acq_destroy (a);
      }
  }

  /* ── noise_mode: four CFAR references, ~15 dB apart ───────────────────
   *
   * The constructor's last argument selects the CFAR noise aggregation
   * (0=mean, 1=median, 2=min, 3=max) and had zero mentions in this file
   * and zero in test_acq.py -- only the default was ever used. noise_est
   * is the denominator of the gating statistic, so the choice moves the
   * effective sensitivity of the whole engine.
   *
   * Measured rather than assumed: on the same burst the statistic spans
   * more than an order of magnitude across the four, because dividing by
   * the smallest reference cell is a far more optimistic detector than
   * dividing by the largest. Asserted as the ORDERING, which is a property
   * of the aggregation and not of the draw: min is the most sensitive, max
   * the least, and mean/median sit between. */
  {
    const size_t   sf = 7, spcl = 2, nxl = sf * spcl, eps = 16;
    float complex *x = malloc (eps * nxl * sizeof *x);
    DP_CHECK (x != NULL);
    if (x)
      {
        uint32_t st = 99u;
        for (size_t k = 0; k < eps * nxl; k++)
          {
            uint8_t chip = CODE7[((k % nxl) / spcl) % sf];
            float   s    = (chip & 1u) ? -1.0f : 1.0f;
            /* Named locals, not two draws in one expression: the order
               of the two dp_gauss() calls would be the compiler's, and
               gcc and clang differ -- so the same seed gave a different
               noise realization per toolchain (make tests-ssot). */
            float re = (float)(0.30 * dp_gauss (&st));
            float im = (float)(0.30 * dp_gauss (&st));
            x[k]     = s + re + im * I;
          }
        double stat[4];
        int    have[4] = { 0, 0, 0, 0 };
        for (int m = 0; m < 4; m++)
          {
            acq_state_t *a = acq_create_burst (CODE7, sf, 8, spcl, 1.0e6, 55.0,
                                               0.0, 1e-2, 0.9, m);
            DP_CHECK (a != NULL);
            if (a)
              {
                /* Pin the grid, for the reason the localization section
                   above already gives: the auto-sizer ascends past
                   n_noncoh = 1 with no caller cap, and at these settings
                   it picks 4 -- so one dump needs 4*n samples and a push
                   sized in epochs quietly produces NO hits in every mode.
                   Measured while writing this, and it reads as "the modes
                   are broken" rather than "the dwell never completed". */
                DP_CHECK (acq_configure_search_raw (a, 8, 1) == 0);
                acq_result_t hits[16];
                size_t       nh = acq_push (a, x, eps * nxl, hits, 16);
                if (nh >= 1)
                  {
                    stat[m] = hits[0].test_stat;
                    have[m] = 1;
                    DP_CHECK (hits[0].noise_est > 0.0f);
                    DP_CHECK (hits[0].test_stat > 0.0f);
                  }
                acq_destroy (a);
              }
          }
        /* min divides by the smallest reference cell, so it is the most
           optimistic; mean and median sit between it and max. Only the
           pairs that both produced a hit can be compared -- an aggressive
           enough reference legitimately suppresses detection entirely,
           which is itself the point of offering the choice. */
        if (have[2] && have[0])
          DP_CHECK (stat[2] > stat[0]);
        if (have[2] && have[1])
          DP_CHECK (stat[2] > stat[1]);
        if (have[3] && have[0])
          DP_CHECK (stat[3] <= stat[0]);
        /* At least the default and the most optimistic must detect, or the
           comparison above is vacuous. */
        DP_CHECK (have[0] && have[2]);
        free (x);
      }
  }

  DP_TEST_END ("test_acq_core");
}
