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
  int          _fails = 0;
  const double PI     = acos (-1.0);

  acq_state_t *ra = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2,
                                      0.9, 0);
  CHECK (ra != NULL);
  if (!ra)
    return _fails;
  CHECK (acq_configure_search_raw (ra, 8, n_noncoh_pin) == 0);
  CHECK (ra->n_noncoh == n_noncoh_pin);

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
  acq_state_t *r1 = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2,
                                      0.9, 0);
  acq_state_t *r2 = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0, 1e-2,
                                      0.9, 0);
  CHECK (r1 && r2);
  if (r1 && r2)
    {
      CHECK (acq_configure_search_raw (r1, 8, n_noncoh_pin) == 0);
      CHECK (acq_configure_search_raw (r2, 8, n_noncoh_pin) == 0);
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
      acq_state_t *r3 = acq_create_burst (CODE7, 7, 8, spc, crate, cn0, 0.0,
                                         1e-2, 0.9, 0);
      CHECK (acq_configure_search_raw (r3, 8, n_noncoh_pin) == 0);
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

  acq_state_t *a = acq_create_burst (CODE31, 31, 16, spc, crate, 45.0, 0.0,
                                     1e-3, 0.9, 0);
  CHECK (a != NULL);
  if (!a)
    return _fails;
  /* Pin coherent-only (n_noncoh == 1): the test below pushes exactly one
   * frame expecting exactly one immediate dump. */
  CHECK (acq_configure_search_raw (a, 16, 1) == 0);

  const size_t   n = a->n; /* coherent_bins * code_bins */
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

/* configure_search_raw: the advanced escape hatch. Bounds violations leave
 * the engine untouched at its prior grid; a valid pin resizes every
 * grid-dependent buffer/plan, re-derives the threshold ladder, and the
 * result actually detects a noise-free burst at that geometry. */
static int
_acq_configure_search_raw_check (void)
{
  int          _fails = 0;
  const size_t spc    = 2;

  acq_state_t *a = acq_create_burst (CODE7, 7, 8, spc, 1.0e6, 45.0, 0.0, 1e-2,
                                     0.9, 0);
  CHECK (a != NULL);
  if (!a)
    return _fails;

  size_t orig_db = a->coherent_bins, orig_nc = a->n_noncoh;

  CHECK (acq_configure_search_raw (a, 0, 1) == -1); /* doppler_bins < 1 */
  CHECK (acq_configure_search_raw (a, 9, 1) == -1); /* > reps (8) */
  CHECK (acq_configure_search_raw (a, 1, 0) == -1); /* n_noncoh < 1 */
  CHECK (acq_configure_search_raw (a, 1, ACQ_N_NONCOH_SAFETY_CEILING + 1)
         == -1); /* > the internal safety-valve ceiling */
  CHECK (a->coherent_bins == orig_db && a->n_noncoh == orig_nc);

  CHECK (acq_configure_search_raw (a, 3, 2) == 0);
  CHECK (a->coherent_bins == 3 && a->n_noncoh == 2);
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

/* Wideband mode (doppler_uncertainty > the native span): coherent_bins is
 * forced to 1 and the uncertainty is tiled with window_bins parallel
 * roll-FFT frequency-window hypotheses (see acq_core.h's file doc comment,
 * and prototypes/async_despreader/bench_freq_bank.py for the benchmark that
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
static int
_acq_wideband_check (void)
{
  int          _fails = 0;
  const double PI     = acos (-1.0);
  const size_t spc    = 2;
  const size_t nx     = 7 * spc; /* code_bins = sf*spc = 14 */
  const double crate  = 1.0e6;
  const double span   = crate / (2.0 * 7.0);

  /* 3.5 * span -> window_bins = ceil(3.5) = 4 (even, so window_bins/2 = 2
   * lands exactly on the convention's positive/negative boundary). */
  acq_state_t *w = acq_create_burst (CODE7, 7, 8, spc, crate, 90.0 /* strong:
                                     forces n_noncoh=1 -- see doc above */,
                                     3.5 * span, 1e-2, 0.9, 0);
  CHECK (w != NULL);
  if (!w)
    return _fails;

  CHECK (w->coherent_bins == 1);
  CHECK (w->window_bins == 4);
  CHECK (w->n == w->window_bins * nx);
  CHECK (w->frame_n == nx); /* one epoch, not window_bins epochs */
  CHECK (w->n_noncoh == 1);
  CHECK (w->searched_bins == w->window_bins);
  CHECK (fabs (w->doppler_res_hz - crate / 7.0) < 1e-6); /* chip_rate/sf */

  /* row r=3 -> signed_r = 3 - 4 = -1 (one window NEGATIVE of DC): the
   * wraparound case. */
  const size_t row      = 3;
  const long   signed_r = (long)row - (long)w->window_bins;
  const double f_norm   = (double)signed_r / (double)nx; /* cycles/sample */
  const size_t d         = 5;                             /* code phase */

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
  CHECK (nh == 1); /* one epoch -> one dump */
  if (nh == 1)
    {
      CHECK (hits[0].doppler_bin == row);
      CHECK (hits[0].code_phase == d);
      CHECK (hits[0].test_stat > w->threshold);
      CHECK (isfinite (hits[0].cn0_dbhz_est) && hits[0].cn0_dbhz_est > 0.0f);
    }

  /* configure_search_raw always exits wideband mode back to the native
   * (doppler_bins, n_noncoh) grid, per its documented contract. */
  CHECK (acq_configure_search_raw (w, 2, 1) == 0);
  CHECK (w->window_bins == 1 && w->coherent_bins == 2);

  acq_destroy (w);
  return _fails;
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
  int                  _fails = 0;
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
  acq_state_t *narrow = acq_create_continuous (CODE31, 31, spc, crate,
                                               sym_rate, cn0_dbhz, 0.0, 1e-3,
                                               0.9, 0);
  CHECK (narrow != NULL);
  if (narrow)
    {
      CHECK (narrow->coherent_bins == 1);
      CHECK (narrow->window_bins == 1);
      acq_destroy (narrow);
    }

  /* 0 < du <= span: still window-tiled (window_bins == 1, same as above) --
   * the point being it's the SAME mechanism/formula as the du > span case
   * below, not a different code path. */
  acq_state_t *within = acq_create_continuous (
      CODE31, 31, spc, crate, sym_rate, cn0_dbhz, 0.5 * span, 1e-3, 0.9, 0);
  CHECK (within != NULL);
  if (within)
    {
      CHECK (within->coherent_bins == 1);
      CHECK (within->window_bins == 1);
      acq_destroy (within);
    }

  /* du > span: window_bins tiles the uncertainty, exactly like
   * acq_create_burst's wideband fallback -- coherent_bins stays pinned at 1
   * either way. */
  acq_state_t *wide = acq_create_continuous (
      CODE31, 31, spc, crate, sym_rate, cn0_dbhz, 3.5 * span, 1e-3, 0.9, 0);
  CHECK (wide != NULL);
  if (wide)
    {
      CHECK (wide->coherent_bins == 1);
      CHECK (wide->window_bins == 4); /* ceil(3.5) */
      CHECK (wide->symbol_rate == sym_rate);
      CHECK (fabs (wide->epochs_per_symbol - (crate / sf) / sym_rate) < 1e-6);
      acq_destroy (wide);
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
  CHECK (acq_create_burst (NULL, 0, 8, spc, crate, 45.0, 0.0, 1e-3, 0.9, 0)
         == NULL);
  CHECK (acq_create_burst (CODE7, 7, 8, spc, 0.0, 45.0, 0.0, 1e-3, 0.9, 0)
         == NULL); /* chip_rate <= 0 */
  CHECK (acq_create_burst (CODE7, 7, 8, spc, crate, 0.0, 0.0, 1e-3, 0.9, 0)
         == NULL); /* cn0_dbhz <= 0 */
  CHECK (acq_create_burst (CODE7, 7, 8, spc, crate, 45.0, 0.0, 0.0, 0.9, 0)
         == NULL); /* pfa out of range */
  /* doppler_uncertainty > span used to be rejected; it now engages wideband
   * mode instead (see _acq_wideband_check below) -- must succeed here. */
  {
    acq_state_t *wide = acq_create_burst (CODE7, 7, 8, spc, crate, 45.0,
                                          span * 2.0, 1e-3, 0.9, 0);
    CHECK (wide != NULL);
    if (wide)
      {
        CHECK (wide->coherent_bins == 1);
        CHECK (wide->window_bins == 2); /* ceil(2.0) */
        acq_destroy (wide);
      }
  }

  /* ── auto-config: a strong C/N0 needs only one coherent rep ──────────── */
  acq_state_t *a
      = acq_create_burst (CODE7, 7, 8, spc, crate, 65.0, 0.0, 1e-2, 0.9, 0);
  CHECK (a != NULL);
  if (!a)
    return 1;
  CHECK (a->sf == 7);
  CHECK (a->code_bins == nx);
  /* Sizing averages Pd over the straddle priors (Jensen-honest): with a
   * 7-chip code the loss tail is heavy enough that one rep's AVERAGE Pd
   * falls short of 0.9 even at 65 dB-Hz, so the engine buys a second. */
  CHECK (a->coherent_bins == 2);
  CHECK (a->n == a->coherent_bins * nx);
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
   * coherent_bins == reps and the slow-time axis has bins to localize on.
   * The injected burst is noise-free, so it clears the (best-effort) gate
   * anyway. Pinned explicitly to n_noncoh=1 (rather than left to the
   * auto-sizer's non-coherent fallback, which would now ascend past 1 with
   * no caller cap to stop it) since the test below pushes exactly one frame
   * expecting exactly one immediate dump. */
  acq_state_t *b
      = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0);
  CHECK (b != NULL);
  if (!b)
    return 1;
  CHECK (b->coherent_bins == 8); /* exhausted to reps */
  CHECK (acq_configure_search_raw (b, 8, 1) == 0);
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
    acq_state_t *ra
        = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9, 0);
    CHECK (ra != NULL);
    if (ra)
      {
        CHECK (acq_configure_search_raw (ra, 8, 1) == 0); /* pin nc=1 */
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
        acq_state_t *r1
            = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9,
                                0);
        acq_state_t *r2
            = acq_create_burst (CODE7, 7, 8, spc, crate, 20.0, 0.0, 1e-2, 0.9,
                                0);
        CHECK (r1 && r2);
        if (r1 && r2)
          {
            CHECK (acq_configure_search_raw (r1, 8, 1) == 0);
            CHECK (acq_configure_search_raw (r2, 8, 1) == 0);
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

  /* acq_run pure-transducer round-trip at two explicitly pinned grids:
   * n_noncoh_pin == 1 (coherent-only) and > 1, the latter covering the
   * nc_surface serialize/restore paths. */
  _fails += _acq_run_roundtrip (s0d, nx, spc, crate, 20.0, 1);
  _fails += _acq_run_roundtrip (s0d, nx, spc, crate, 30.0, 8);

  _fails += _acq_cn0_calibration ();
  _fails += _acq_configure_search_raw_check ();
  _fails += _acq_wideband_check ();
  _fails += _acq_continuous_check ();

  if (_fails)
    {
      fprintf (stderr, "test_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_acq_core PASSED\n");
  return 0;
}
