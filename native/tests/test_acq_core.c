/*
 * test_acq_core.c — DSSS acquisition engine C-level tests.
 *
 * Covers: argument validation, physics auto-config (C/N0 -> snr, the chosen
 * coherent depth doppler_bins, threshold/eta), noise-free localization of a
 * streamed burst to the injected (Doppler bin, code phase), and a real AWGN
 * calibration check that acq_result_t::cn0_dbhz_est tracks a known injected
 * C/N0.
 */
#include "acq/acq_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* A length-7 maximal-length sequence (one period). */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* ── acq_run pure-transducer (state in/out) round-trip ─────────────────────
 * The stateless elastic face: state_in == NULL resets then processes; a fresh
 * engine + state_in reproduces an uninterrupted run; a corrupted blob is
 * rejected (acq_run returns 0). Driven at two operating points so both the
 * coherent and the non-coherent (nc_surface) serialization paths are covered:
 * a strong cn0 (coherent-only) and a weak cn0 + max_noncoh (n_noncoh > 1).
 * @p s0d is the oversampled, code-phase-rolled BPSK replica (length @p nx). */
static int
_acq_run_roundtrip (const float complex *s0d, size_t nx, size_t spc,
                    double crate, double cn0, size_t max_noncoh, int want_nc)
{
  int          _fails = 0;
  const double PI     = acos (-1.0);

  acq_state_t *ra = acq_create (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9,
                                0, max_noncoh, 0.0, 0.0, 0.0);
  CHECK (ra != NULL);
  if (!ra)
    return _fails;
  /* The weak-cn0 call must actually engage non-coherent integration, else it
   * wouldn't exercise the nc_surface serialization branches. */
  CHECK (want_nc ? (ra->n_noncoh > 1) : (ra->n_noncoh == 1));

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
  acq_state_t *r1 = acq_create (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9,
                                0, max_noncoh, 0.0, 0.0, 0.0);
  acq_state_t *r2 = acq_create (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2, 0.9,
                                0, max_noncoh, 0.0, 0.0, 0.0);
  CHECK (r1 && r2);
  if (r1 && r2)
    {
      size_t       cb   = acq_state_bytes (r1);
      void        *blob = malloc (cb);
      acq_result_t hB[16];
      size_t       nB = acq_run (r1, NULL, blob, s, cut, hB, 16);
      nB += acq_run (r2, blob, NULL, s + cut, L - cut, hB + nB, 16 - nB);

      CHECK (nA >= 1 && nB == nA);
      for (size_t i = 0; i < nA && i < nB; i++)
        {
          CHECK (hA[i].doppler_bin == hB[i].doppler_bin);
          CHECK (hA[i].code_phase == hB[i].code_phase);
          CHECK (fabsf (hA[i].test_stat - hB[i].test_stat) < 1e-4f);
        }

      /* a corrupted blob must make acq_run reject (set_state != 0) -> 0 out.
       */
      acq_state_t *r3 = acq_create (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2,
                                    0.9, 0, max_noncoh, 0.0, 0.0, 0.0);
      acq_get_state (r3, blob);
      ((char *)blob)[0] ^= (char)0xFF; /* clobber the state header magic */
      CHECK (acq_run (r3, blob, NULL, s, cut, hB, 16) == 0);
      acq_destroy (r3);

      free (blob);
    }
  acq_destroy (r1);
  acq_destroy (r2);
  free (s);
  return _fails;
}

/* Unit-variance complex Gaussian (Box-Muller from xorshift); 0.5 variance per
 * component so E|z|^2 = 1 (same generator as test_dll_core.c/
 * test_symsync_core.c — no shared test-utils header exists for it yet). */
static float complex
cgauss (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t a = *st;
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t b   = *st;
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1)); /* sqrt(-2 ln u1)/sqrt(2) */
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
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
  int                  _fails = 0;
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc      = 4;
  const double crate    = 1.0e6;
  const double fs       = crate * (double)spc;
  const double cn0_true = 55.0;

  acq_state_t *a = acq_create (CODE31, 31, 16, spc, crate, 45.0, 0.0, 1e-3,
                               0.9, 0, 1, 0.0, 0.0, 0.0);
  CHECK (a != NULL);
  if (!a)
    return _fails;

  const size_t   n = a->n; /* doppler_bins * code_bins */
  float complex *x = malloc (n * sizeof (float complex));
  CHECK (x != NULL);
  if (!x)
    {
      acq_destroy (a);
      return _fails;
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
      x[k]         = c + sigma * cgauss (&st);
    }

  acq_result_t hits[4];
  size_t       nh = acq_push (a, x, n, hits, 4);
  CHECK (nh == 1);
  if (nh == 1)
    CHECK (fabsf (hits[0].cn0_dbhz_est - (float)cn0_true) < 3.0f);

  free (x);
  acq_destroy (a);
  return _fails;
}

/* Data-modulation-aware sizing (symbol_rate > 0): at a realistic weak
 * margin, pd_predicted (derived through the joint (doppler_bins, n_noncoh)
 * search and _data_mod_pd) should track real empirical Pd on a continuous
 * async-data-modulated stream. Zero Doppler / zero code-phase offset
 * isolates the data-transition self-cancellation effect this fix targets
 * from the Doppler/code-phase straddle model _mean_pd already covers
 * elsewhere in this file. Also exercises a genuine (doppler_bins > 1 &&
 * n_noncoh > 1) grid -- the gallery script's own _theoretical_pd (which
 * this ports from) was validated only at doppler_bins == 1 or n_noncoh ==
 * 1, so a real push-through here is the correctness proof for the general
 * case. */
static int
_acq_data_mod_check (void)
{
  int                  _fails = 0;
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc   = 4;
  const double crate = 1.0e6;
  const double fs    = crate * (double)spc;
  /* epochs_per_symbol ~ 1.4: the same regime documented in
   * docs/gallery/dsss-acq-async-data.md. One epoch is sf chips (its
   * duration in seconds doesn't depend on spc, the samples/chip
   * oversample factor) -- epoch_rate = chip_rate/sf, so pick a symbol_rate
   * that lands ~1.4 epochs per data symbol at that epoch rate. */
  const double epoch_sec = 31.0 / crate;
  const double sym_rate  = 1.0 / (1.4 * epoch_sec);
  /* This 31-chip code's per-epoch processing gain is far below the
   * 1023-chip Gold code the gallery script uses, so its "weak, realistic"
   * cn0_dbhz is proportionally higher (~15 dB, matching the shorter code's
   * ~15 dB smaller coherent gain per epoch); chosen empirically so the
   * joint search lands on a genuine doppler_bins > 1 && n_noncoh > 1 grid
   * (D=2, nc=7 here) -- exercising the general case, not just the two
   * special cases the gallery script's own model validated. */
  const double cn0_dbhz = 50.0;

  acq_state_t *a = acq_create (CODE31, 31, 16, spc, crate, cn0_dbhz, 0.0, 1e-3,
                               0.9, 0, 16, sym_rate, 0.0, 0.0);
  CHECK (a != NULL);
  if (!a)
    return _fails;
  CHECK (a->symbol_rate == sym_rate);
  CHECK (fabs (a->epochs_per_symbol - 1.4) < 1e-6);
  CHECK (!a->underpowered);
  CHECK (a->doppler_bins <= a->reps && a->n_noncoh <= a->max_noncoh);

  const size_t cb    = a->code_bins; /* samples in one epoch */
  const size_t depth = a->doppler_bins * a->n_noncoh; /* epochs/decision */
  const size_t opp_target    = 400; /* decision opportunities to gather */
  const size_t decision_samp = depth * cb; /* samples spanning one decision */
  /* Slack epochs beyond one decision's worth so a trial can start at any
   * random sample offset up to a full symbol period and still find
   * decision_samp samples ahead of it -- this is what makes the window's
   * phase relative to the symbol clock (phi0) land uniformly, matching
   * what _data_mod_pd averages over, rather than always the same phase a
   * fixed period-aligned stride would produce (14 epochs == exactly 10
   * symbols here, so a fixed stride would silently repeat one phi0 for
   * every decision). */
  const size_t max_offset = (size_t)(a->epochs_per_symbol * (double)cb) + cb;
  const size_t buf_samp   = decision_samp + max_offset;
  const size_t buf_epochs = (buf_samp + cb - 1) / cb;

  float complex *x = malloc (buf_epochs * cb * sizeof (float complex));
  CHECK (x != NULL);
  if (!x)
    {
      acq_destroy (a);
      return _fails;
    }

  const double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  const float  sigma   = (float)(1.0 / amp_snr);
  uint32_t     rst     = 424242u;

  /* Random i.i.d. +-1 data sign per symbol, looked up by each sample's
   * exact fractional symbol position -- a transition can land mid-epoch
   * (any chip offset), not just on an epoch boundary; getting this right
   * at chip granularity is the whole point of the model under test. */
  const size_t total_chips = buf_epochs * 31;
  const size_t n_symbols
      = (size_t)((double)total_chips / (a->epochs_per_symbol * 31.0)) + 4;
  float   *signs    = malloc (n_symbols * sizeof (float));
  uint32_t sign_rst = 13579u;
  for (size_t i = 0; i < n_symbols; i++)
    {
      sign_rst ^= sign_rst << 13;
      sign_rst ^= sign_rst >> 17;
      sign_rst ^= sign_rst << 5;
      signs[i] = (sign_rst & 1u) ? 1.0f : -1.0f;
    }

  for (size_t e = 0; e < buf_epochs; e++)
    for (size_t k = 0; k < cb; k++)
      {
        size_t chip_global = e * 31 + k / spc;
        size_t sym_id
            = (size_t)((double)chip_global / (a->epochs_per_symbol * 31.0));
        uint8_t chip  = CODE31[(k / spc) % 31];
        float   c     = (chip & 1u) ? -1.0f : 1.0f;
        x[e * cb + k] = signs[sym_id] * c + sigma * cgauss (&rst);
      }
  free (signs);

  /* Each trial: reset to a clean slate, then push decision_samp samples
   * starting at a random offset into the buffer -- the reset makes each
   * trial an independent decision opportunity, and the random start
   * (rather than a fixed stride) samples phi0 uniformly. */
  uint32_t off_rst = 909090u;
  size_t   ndet    = 0;
  for (size_t trial = 0; trial < opp_target; trial++)
    {
      off_rst ^= off_rst << 13;
      off_rst ^= off_rst >> 17;
      off_rst ^= off_rst << 5;
      size_t off = off_rst % (max_offset + 1);

      acq_reset (a);
      acq_result_t hits[4];
      ndet += acq_push (a, x + off, decision_samp, hits, 4);
    }

  double pd_empirical = (double)ndet / (double)opp_target;
  /* Generous tolerance: opp_target=400 gives a binomial SE ~0.015 at
   * pd~0.9, so this is an ~8-sigma band -- wide enough to absorb the
   * model's own known residual (the coherent case's un-modeled code-phase
   * -axis leakage, see the gallery page) without masking a real bug (the
   * pre-fix model's error on this class of scenario was >0.17). */
  CHECK (fabs (pd_empirical - a->pd_predicted) < 0.12);

  free (x);
  acq_destroy (a);
  return _fails;
}

/* configure_search_raw: the advanced escape hatch. Bounds violations leave
 * the engine untouched at its prior grid; a valid pin resizes every
 * grid-dependent buffer/plan, re-derives the threshold ladder, and the
 * result actually detects a noise-free burst at that geometry. */
static int
_acq_configure_search_raw_check (void)
{
  int          _fails = 0;
  const size_t spc    = 2;

  acq_state_t *a = acq_create (CODE7, 7, 8, spc, 1.0e6, 45.0, 0.0, 1e-2, 0.9,
                               0, 4, 0.0, 0.0, 0.0);
  CHECK (a != NULL);
  if (!a)
    return _fails;

  size_t orig_db = a->doppler_bins, orig_nc = a->n_noncoh;

  CHECK (acq_configure_search_raw (a, 0, 1) == -1); /* doppler_bins < 1 */
  CHECK (acq_configure_search_raw (a, 9, 1) == -1); /* > reps (8) */
  CHECK (acq_configure_search_raw (a, 1, 0) == -1); /* n_noncoh < 1 */
  CHECK (acq_configure_search_raw (a, 1, 5) == -1); /* > max_noncoh (4) */
  CHECK (a->doppler_bins == orig_db && a->n_noncoh == orig_nc);

  CHECK (acq_configure_search_raw (a, 3, 2) == 0);
  CHECK (a->doppler_bins == 3 && a->n_noncoh == 2);
  CHECK (a->n == 3 * a->code_bins);
  CHECK (a->eta_nc > 0.0f && a->threshold == 0.0f);

  const size_t   n     = a->n;
  float complex *burst = malloc (2 * n * sizeof (float complex));
  CHECK (burst != NULL);
  if (burst)
    {
      for (size_t k = 0; k < 2 * n; k++)
        {
          uint8_t chip = CODE7[(k / spc) % 7];
          burst[k]     = (chip & 1u) ? -1.0f : 1.0f;
        }
      acq_result_t hits[4];
      size_t       nh = acq_push (a, burst, 2 * n, hits, 4);
      CHECK (nh == 1);
      if (nh == 1)
        {
          CHECK (hits[0].doppler_bin == 0);
          CHECK (hits[0].code_phase == 0);
        }
      free (burst);
    }

  acq_destroy (a);
  return _fails;
}

/* doppler_resolution: the resolution floor added to fix the O(reps^3)
 * construction-time blowup of the joint (doppler_bins, n_noncoh) search once
 * reps is raised to reach a fine resolution (see acq_create()'s doc). Checks
 * the floor is honored and clipped at both ends, a negative value is
 * rejected, and -- the actual point of the fix -- that construction stays
 * fast even at a reps large enough that the unfloored sweep would be
 * O(reps^3). */
static int
_acq_doppler_resolution_check (void)
{
  int                  _fails = 0;
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc      = 4;
  const double crate    = 1.0e6;
  const double sf       = 31.0;
  const double sym_rate = 2000.0; /* > 0 enables the joint search */
  const double cn0_dbhz = 55.0;

  /* Negative doppler_resolution is rejected, same as any other bad arg. */
  CHECK (acq_create (CODE31, 31, 16, spc, crate, cn0_dbhz, 0.0, 1e-3, 0.9, 0,
                     8, sym_rate, -1.0, 0.0)
         == NULL);

  /* Floored: request a resolution finer than the unfloored search would
   * ever pick (it favors doppler_bins=1 whenever that meets pd) -- the
   * chosen grid must floor doppler_bins at
   * ceil(chip_rate/(sf*doppler_resolution)), and its achieved doppler_res_hz
   * must meet the target. */
  {
    const double res_hz  = 100.0;
    const size_t reps    = 400; /* well past the floor this res_hz implies */
    size_t       floor_d = (size_t)ceil (crate / (sf * res_hz));
    if (floor_d < 1)
      floor_d = 1;
    if (floor_d > reps)
      floor_d = reps;

    acq_state_t *a = acq_create (CODE31, 31, reps, spc, crate, cn0_dbhz, 0.0,
                                 1e-3, 0.9, 0, 8, sym_rate, res_hz, 0.0);
    CHECK (a != NULL);
    if (a)
      {
        CHECK (a->doppler_resolution == res_hz);
        CHECK (a->doppler_bins >= floor_d);
        CHECK (a->doppler_res_hz <= res_hz + 1e-9);
        CHECK (!a->underpowered);
        acq_destroy (a);
      }
  }

  /* Clipped high: a resolution finer than reps can ever deliver clips the
   * floor at reps itself. */
  {
    acq_state_t *a = acq_create (CODE31, 31, 8, spc, crate, cn0_dbhz, 0.0,
                                 1e-3, 0.9, 0, 8, sym_rate, 1.0, 0.0);
    CHECK (a != NULL);
    if (a)
      {
        CHECK (a->doppler_bins == 8); /* clipped to reps */
        acq_destroy (a);
      }
  }

  /* Clipped low: a resolution coarser than doppler_bins=1 already achieves
   * floors at 1, i.e. the floored search starts exactly where the unfloored
   * one does. They aren't guaranteed to agree in general past that (the
   * floored search stops at the first D that meets pd; the unfloored one
   * keeps searching for the true minimum-total D, which need not be 1) --
   * but at a cn0 strong enough that D=1,nc=1 alone already meets pd, total=1
   * is the global minimum no search order can beat, so both must land on
   * the same (1, 1) regardless. */
  {
    const double cn0_strong = 80.0;
    acq_state_t *unfloored
        = acq_create (CODE31, 31, 16, spc, crate, cn0_strong, 0.0, 1e-3, 0.9,
                      0, 8, sym_rate, 0.0, 0.0);
    acq_state_t *coarse
        = acq_create (CODE31, 31, 16, spc, crate, cn0_strong, 0.0, 1e-3, 0.9,
                      0, 8, sym_rate, 1.0e9, 0.0);
    CHECK (unfloored != NULL && coarse != NULL);
    if (unfloored && coarse)
      {
        CHECK (unfloored->doppler_bins == 1 && unfloored->n_noncoh == 1);
        CHECK (unfloored->doppler_bins == coarse->doppler_bins
               && unfloored->n_noncoh == coarse->n_noncoh);
      }
    if (unfloored)
      acq_destroy (unfloored);
    if (coarse)
      acq_destroy (coarse);
  }

  /* The actual point of the fix: construction must stay fast even at a reps
   * large enough that the unfloored sweep would be O(reps^3) -- measured
   * ~145s (extrapolated from the cubic fit) before the floor-anchored early
   * exit; a generous 5s ceiling leaves >20x margin while still catching a
   * regression back to the full sweep. */
  {
    const clock_t t0 = clock ();
    acq_state_t  *a  = acq_create (CODE31, 31, 100, spc, crate, cn0_dbhz, 0.0,
                                   1e-3, 0.9, 0, 8, sym_rate, 20.0, 0.0);
    double        dt = (double)(clock () - t0) / CLOCKS_PER_SEC;
    CHECK (a != NULL);
    CHECK (dt < 5.0);
    if (a)
      acq_destroy (a);
  }

  return _fails;
}

/* doppler_rate: caps the coherent-depth ceiling from the opposite direction
 * to doppler_resolution's floor -- past D < chip_rate/(sf*sqrt(doppler_rate)),
 * in-window Doppler drift would smear the FFT peak across a resolution bin.
 * Checks it tightens both the unfloored search's own range and a caller-
 * requested doppler_resolution's floor, and rejects a negative value. */
static int
_acq_doppler_rate_check (void)
{
  int                  _fails = 0;
  static const uint8_t CODE31[31]
      = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
          1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };
  const size_t spc      = 4;
  const double crate    = 1.0e6;
  const double sf       = 31.0;
  const double sym_rate = 2000.0;
  const double cn0_dbhz = 55.0;

  /* Negative doppler_rate is rejected, same as any other bad arg. */
  CHECK (acq_create (CODE31, 31, 16, spc, crate, cn0_dbhz, 0.0, 1e-3, 0.9, 0,
                     8, sym_rate, 0.0, -1.0)
         == NULL);

  /* rate_ceiling_d well below both reps and the resolution floor it's paired
   * with -- must win over both. */
  const double rate_hz      = 1.0e6;
  size_t       rate_ceiling = (size_t)floor (crate / (sf * sqrt (rate_hz)));
  CHECK (rate_ceiling >= 1 && rate_ceiling < 400);

  /* Tightens a caller-requested doppler_resolution's floor: 100 Hz alone
   * would ask for doppler_bins ~323 (see the doppler_resolution test above),
   * far past rate_ceiling. */
  {
    acq_state_t *a = acq_create (CODE31, 31, 400, spc, crate, cn0_dbhz, 0.0,
                                 1e-3, 0.9, 0, 8, sym_rate, 100.0, rate_hz);
    CHECK (a != NULL);
    if (a)
      {
        CHECK (a->doppler_bins <= rate_ceiling);
        acq_destroy (a);
      }
  }

  /* Tightens the unfloored search's own range too: reps=400 would otherwise
   * let it search all the way up. */
  {
    acq_state_t *a = acq_create (CODE31, 31, 400, spc, crate, cn0_dbhz, 0.0,
                                 1e-3, 0.9, 0, 8, sym_rate, 0.0, rate_hz);
    CHECK (a != NULL);
    if (a)
      {
        CHECK (a->doppler_bins <= rate_ceiling);
        acq_destroy (a);
      }
  }

  /* Never loosens: with reps already below the rate ceiling, doppler_rate
   * changes nothing. */
  {
    const double loose_rate = 1.0; /* rate_ceiling_d in the thousands */
    acq_state_t *unrated
        = acq_create (CODE31, 31, 16, spc, crate, cn0_dbhz, 0.0, 1e-3, 0.9, 0,
                      8, sym_rate, 0.0, 0.0);
    acq_state_t *rated
        = acq_create (CODE31, 31, 16, spc, crate, cn0_dbhz, 0.0, 1e-3, 0.9, 0,
                      8, sym_rate, 0.0, loose_rate);
    CHECK (unrated != NULL && rated != NULL);
    if (unrated && rated)
      CHECK (unrated->doppler_bins == rated->doppler_bins
             && unrated->n_noncoh == rated->n_noncoh);
    if (unrated)
      acq_destroy (unrated);
    if (rated)
      acq_destroy (rated);
  }

  return _fails;
}

int
main (void)
{
  int          _fails = 0;
  const double PI     = acos (-1.0);

  const size_t spc   = 2;
  const size_t nx    = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate = 1.0e6;   /* 1 MHz chips */
  const double span  = crate / (2.0 * 7.0);

  /* ── argument validation ────────────────────────────────────────────── */
  CHECK (acq_create (NULL, 0, 8, spc, crate, 45.0, 0.0, 1e-3, 0.9, 0, 1, 0.0,
                     0.0, 0.0)
         == NULL);
  CHECK (acq_create (CODE7, 7, 8, spc, 0.0, 45.0, 0.0, 1e-3, 0.9, 0, 1, 0.0,
                     0.0, 0.0)
         == NULL); /* chip_rate <= 0 */
  CHECK (acq_create (CODE7, 7, 8, spc, crate, 0.0, 0.0, 1e-3, 0.9, 0, 1, 0.0,
                     0.0, 0.0)
         == NULL); /* cn0_dbhz <= 0 */
  CHECK (acq_create (CODE7, 7, 8, spc, crate, 45.0, 0.0, 0.0, 0.9, 0, 1, 0.0,
                     0.0, 0.0)
         == NULL); /* pfa out of range */
  CHECK (acq_create (CODE7, 7, 8, spc, crate, 45.0, span * 2.0, 1e-3, 0.9, 0,
                     1, 0.0, 0.0, 0.0)
         == NULL); /* doppler_uncertainty > span */

  /* ── auto-config: a strong C/N0 needs only one coherent rep ──────────── */
  acq_state_t *a = acq_create (CODE7, 7, 8, spc, crate, 65.0, 0.0, 1e-2, 0.9,
                               0, 1, 0.0, 0.0, 0.0);
  CHECK (a != NULL);
  if (!a)
    return 1;
  CHECK (a->sf == 7);
  CHECK (a->code_bins == nx);
  /* Sizing averages Pd over the straddle priors (Jensen-honest): with a
   * 7-chip code the loss tail is heavy enough that one rep's AVERAGE Pd
   * falls short of 0.9 even at 65 dB-Hz, so the engine buys a second. */
  CHECK (a->doppler_bins == 2);
  CHECK (a->n == a->doppler_bins * nx);
  CHECK (!a->underpowered && a->pd_predicted >= 0.9);
  CHECK (a->noise_lo == 0 && a->noise_hi == a->n - 1);
  CHECK (a->fs == crate * (double)spc);
  CHECK (fabs (a->doppler_span_hz - span) < 1e-6);
  /* threshold = eta * sqrt(2/pi); eta = sqrt(-2 ln pfa_cell) > 0 */
  CHECK (a->eta > 0.0f);
  CHECK (fabsf (a->threshold - a->eta * 0.7978845608f) < 1e-4f);
  acq_destroy (a);

  /* ── noise-free localization (force a multi-bin Doppler axis) ─────────── */
  /* A very weak target C/N0 makes the D-search exhaust to reps, so
   * doppler_bins == reps and the slow-time axis has bins to localize on.  The
   * injected burst is noise-free, so it clears the (best-effort) gate anyway.
   */
  acq_state_t *b = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9,
                               0, 1, 0.0, 0.0, 0.0);
  CHECK (b != NULL);
  if (!b)
    return 1;
  CHECK (b->doppler_bins == 8); /* exhausted to reps */
  const size_t ny = b->doppler_bins;
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
  CHECK (nh == 1); /* one frame -> one dump */
  if (nh == 1)
    {
      CHECK (hits[0].doppler_bin == u);
      CHECK (hits[0].code_phase == d);
      CHECK (hits[0].test_stat > b->threshold);
      CHECK (isfinite (hits[0].cn0_dbhz_est) && hits[0].cn0_dbhz_est > 0.0f);
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
    acq_state_t *ra = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2,
                                  0.9, 0, 1, 0.0, 0.0, 0.0);
    CHECK (ra != NULL);
    if (ra)
      {
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
        acq_state_t *r1 = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2,
                                      0.9, 0, 1, 0.0, 0.0, 0.0);
        acq_state_t *r2 = acq_create (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2,
                                      0.9, 0, 1, 0.0, 0.0, 0.0);
        CHECK (r1 && r2);
        if (r1 && r2)
          {
            acq_result_t hB[8];
            size_t       nB = acq_push (r1, s, cut, hB, 8);

            size_t cb   = acq_state_bytes (r1);
            void  *blob = malloc (cb);
            acq_get_state (r1, blob);
            CHECK (acq_set_state (r2, blob) == DP_OK);
            /* standard envelope: a magic-clobbered blob is rejected directly,
             * r2 left untouched (validate runs before any mutation). */
            ((char *)blob)[0] ^= (char)0xFF;
            CHECK (acq_set_state (r2, blob) == DP_ERR_INVALID);
            ((char *)blob)[0] ^= (char)0xFF;

            nB += acq_push (r2, s + cut, L3 - cut, hB + nB, 8 - nB);

            CHECK (nA == 3 && nB == nA); /* both see all 3 full frames */
            for (size_t i = 0; i < nA && i < nB; i++)
              {
                CHECK (hA[i].doppler_bin == hB[i].doppler_bin);
                CHECK (hA[i].code_phase == hB[i].code_phase);
                CHECK (fabsf (hA[i].peak_mag - hB[i].peak_mag) < 1e-5f);
                CHECK (fabsf (hA[i].test_stat - hB[i].test_stat) < 1e-5f);
                CHECK (fabsf (hA[i].cn0_dbhz_est - hB[i].cn0_dbhz_est)
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

  /* acq_run pure-transducer round-trip at two operating points: coherent
   * (n_noncoh == 1) and non-coherent (weak cn0 + max_noncoh -> n_noncoh > 1),
   * the latter covering the nc_surface serialize/restore paths. */
  _fails += _acq_run_roundtrip (s0d, nx, spc, crate, 20.0, 1, 0);
  _fails += _acq_run_roundtrip (s0d, nx, spc, crate, 30.0, 8, 1);

  _fails += _acq_cn0_calibration ();
  _fails += _acq_data_mod_check ();
  _fails += _acq_configure_search_raw_check ();
  _fails += _acq_doppler_resolution_check ();
  _fails += _acq_doppler_rate_check ();

  if (_fails)
    {
      fprintf (stderr, "test_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acq_core PASSED\n");
  return 0;
}
