#include "tonemeas/tonemeas_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

#define NCAP 4096u

/* Add a real cosine of `amp` at `cycles` cycles over the capture (may be
 * fractional, for sub-bin placement). */
static void
add_cos (float *x, size_t n, double cycles, double amp)
{
  for (size_t i = 0; i < n; i++)
    x[i] += (float)(amp * cos (2.0 * M_PI * cycles * (double)i / (double)n));
}

/* Deterministic uniform white noise in [-a, a] (variance a^2/3). */
static uint32_t _rng = 12345u;
static double
urand (void)
{
  _rng = _rng * 1664525u + 1013904223u;
  return ((double)_rng / 4294967296.0) * 2.0 - 1.0;
}

int
main (void)
{
  int         _fails = 0;
  tone_meas_t r;
  float      *x = (float *)malloc (NCAP * sizeof (float));

  /* ── 1. lobe-integration invariance: a full-scale tone reads ~0 dBFS at
   *       any sub-bin offset (the headline correctness property) ── */
  tonemeas_state_t *m = tonemeas_create (NCAP, 1.0, 1, 12.0f, 2, 8, 1.0, 0, 0);
  CHECK (m != NULL);
  for (int t = 0; t < 3; t++)
    {
      double off = 0.25 * t; /* 0, 0.25, 0.5 bins */
      for (size_t i = 0; i < NCAP; i++)
        x[i] = 0.0f;
      add_cos (x, NCAP, 300.0 + off, 1.0);
      r = tonemeas_analyze (m, x, NCAP);
      CHECK (fabs (r.fund_dbfs) < 0.1); /* full-scale -> ~0 dBFS */
      CHECK (fabs (r.fund_freq - (300.0 + off) / NCAP) < 2e-3);
    }

  /* ── 2. THD: 2nd harmonic at -40 dBc (amplitude 0.01) ── */
  for (size_t i = 0; i < NCAP; i++)
    x[i] = 0.0f;
  add_cos (x, NCAP, 200.0, 1.0);
  add_cos (x, NCAP, 400.0, 0.01); /* 2nd harmonic */
  r = tonemeas_analyze (m, x, NCAP);
  CHECK (fabs (r.thd - (-40.0)) < 0.5);
  CHECK (fabs (r.thd_pct - 1.0) < 0.1); /* 100*sqrt(1e-4) = 1% */

  /* ── 3. SFDR: non-harmonic spur at -60 dBc ── */
  for (size_t i = 0; i < NCAP; i++)
    x[i] = 0.0f;
  add_cos (x, NCAP, 200.0, 1.0);
  add_cos (x, NCAP, 777.0, 0.001); /* -60 dBc, non-harmonic */
  r = tonemeas_analyze (m, x, NCAP);
  CHECK (fabs (r.sfdr_dbc - 60.0) < 1.0);
  CHECK (r.worst_spur_is_harm == 0);
  CHECK (fabs (r.worst_spur_freq - 777.0 / NCAP) < 2e-3);

  /* ── 4. SNR: tone A=0.5 + white noise sigma=1e-3 ──
   *  SNR = 10log10((A^2/2)/sigma^2). sigma = a/sqrt(3), a = sigma*sqrt(3). */
  {
    double A = 0.5, sigma = 1e-3, a = sigma * sqrt (3.0);
    for (size_t i = 0; i < NCAP; i++)
      x[i] = 0.0f;
    add_cos (x, NCAP, 211.0, A);
    for (size_t i = 0; i < NCAP; i++)
      x[i] += (float)(a * urand ());
    r                 = tonemeas_analyze (m, x, NCAP);
    double snr_expect = 10.0 * log10 ((A * A / 2.0) / (sigma * sigma));
    CHECK (fabs (r.snr - snr_expect) < 1.5);
    /* ENOB derives from SINAD (~SNR here, no harmonics) */
    CHECK (fabs (r.enob - (r.sinad - 1.76) / 6.02) < 1e-6);
  }

  /* ── 5. complex capture: tone at a negative frequency ── */
  {
    float complex *xc
        = (float complex *)malloc (NCAP * sizeof (float complex));
    for (size_t i = 0; i < NCAP; i++)
      xc[i] = (float complex) (
          1.0 * cexp (-2.0 * I * M_PI * 137.0 * (double)i / (double)NCAP));
    r = tonemeas_analyze_complex (m, xc, NCAP);
    CHECK (fabs (r.fund_freq - (-137.0 / NCAP)) < 2e-3);
    CHECK (fabs (r.fund_dbfs) < 0.2); /* full-scale complex tone -> ~0 dBFS */
    free (xc);
  }

  /* ── 6. harmonic folding (real): 3rd harmonic of f0=0.3 aliases to 0.1 ──
   *  3*0.3 = 0.9 > 0.5 -> reflects to 1 - 0.9 = 0.1.  Inject -40 dBc there;
   *  the analyser must fold the 3rd harmonic to 0.1 and count it in THD. */
  {
    double n3 = 0.3 * NCAP; /* 1228.8 cycles */
    for (size_t i = 0; i < NCAP; i++)
      x[i] = 0.0f;
    add_cos (x, NCAP, n3, 1.0);
    add_cos (x, NCAP, 0.1 * NCAP, 0.01); /* aliased 3rd harmonic, -40 dBc */
    r = tonemeas_analyze (m, x, NCAP);
    CHECK (r.thd > -45.0 && r.thd < -35.0); /* folded harmonic detected */
  }

  /* ── 7. time stats: a sine has crest factor ~3.01 dB ── */
  {
    for (size_t i = 0; i < NCAP; i++)
      x[i] = 0.0f;
    add_cos (x, NCAP, 50.0, 0.8);
    time_stats_t ts;
    ts = tonemeas_time_stats (m, x, NCAP);
    CHECK (fabs (ts.crest_db - 3.01) < 0.1);
    CHECK (fabs (ts.fs_util_pct - 80.0) < 1.0);
    CHECK (fabs (ts.dc_offset) < 1e-3);
  }

  /* ── 8. multi-frame averaging (the averaged-spectrum path) ──
   *  A capture longer than n is split into floor(len/n) segments whose power
   *  spectra are averaged before the metric kernel runs. */
  {
    const size_t K  = 8;
    float       *xk = (float *)malloc (K * NCAP * sizeof (float));

    /* (a) K identical noiseless frames reduce to the single-frame result. */
    for (size_t i = 0; i < NCAP; i++)
      x[i] = 0.0f;
    add_cos (x, NCAP, 211.0, 0.5);
    tone_meas_t r1 = tonemeas_analyze (m, x, NCAP);
    for (size_t f = 0; f < K; f++)
      for (size_t i = 0; i < NCAP; i++)
        xk[f * NCAP + i] = x[i];
    tone_meas_t rk = tonemeas_analyze (m, xk, K * NCAP);
    CHECK (fabs (rk.fund_dbfs - r1.fund_dbfs) < 1e-4);
    CHECK (fabs (rk.fund_freq - r1.fund_freq) < 1e-9);

    /* (b) K frames of tone + independent white noise: SNR ~ analytic. */
    double A = 0.5, sigma = 1e-3, a = sigma * sqrt (3.0);
    for (size_t f = 0; f < K; f++)
      {
        for (size_t i = 0; i < NCAP; i++)
          xk[f * NCAP + i] = 0.0f;
        add_cos (xk + f * NCAP, NCAP, 211.0, A);
        for (size_t i = 0; i < NCAP; i++)
          xk[f * NCAP + i] += (float)(a * urand ());
      }
    tone_meas_t rn         = tonemeas_analyze (m, xk, K * NCAP);
    double      snr_expect = 10.0 * log10 ((A * A / 2.0) / (sigma * sigma));
    CHECK (fabs (rn.snr - snr_expect) < 1.5);
    CHECK (fabs (rn.fund_freq - 211.0 / NCAP) < 2e-3);
    free (xk);
  }

  /* ── 9. accuracy metadata sanity ── */
  CHECK (r.bin_hz > 0.0 && r.rbw_hz > 0.0);
  CHECK (r.lobe_bins == m->lobe_bins);
  CHECK (r.n_noise_bins > 0);

  tonemeas_destroy (m);
  free (x);

  if (_fails)
    {
      fprintf (stderr, "test_tonemeas_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_tonemeas_core PASSED\n");
  return 0;
}
