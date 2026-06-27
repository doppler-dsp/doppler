/**
 * @file test_carrier_mpsk_core.c
 * @brief Unit tests for the M-PSK carrier-tracking loop.
 *
 * Tests:
 *   1. Lifecycle / gain math / init==create parity / m validation
 *   2. Pull-in per M — BPSK/QPSK lock a grid of residuals (pure PLL),
 *      8PSK locks with the FLL assist
 *   3. M-fold phase ambiguity is tolerated (output correct up to a rotation)
 *   4. Noise robustness — locks under AWGN
 *   5. FLL assist widens pull-in beyond the bare PLL
 *   6. Reset reproducibility
 *
 * The m=2 ≡ BPSK Costas identity (the headline anchor) is asserted in the
 * Python test (test_carrier_mpsk.py), where both types live in track.so.
 */
#include "carrier_mpsk/carrier_mpsk_core.h"
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

/* xorshift32 PRNG — deterministic across runs. */
static uint32_t
xs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return x;
}

/* Box-Muller unit-variance Gaussian (per component), seeded. */
static float
gauss (uint32_t *st)
{
  double r1 = (xs (st) + 1.0) / 4294967297.0;
  double r2 = (xs (st) + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (2.0 * M_PI * r2));
}

/* Continuous M-PSK-at-symbol-rate signal with carrier residual f0
 * (cycles/sample), optional per-sample frequency ramp, and optional AWGN
 * sigma per component.  Records the Gray labels used in `labels`. */
static void
make_signal (float complex *rx, int *labels, size_t nsym, size_t tsamps, int m,
             double f0, double ramp, float sigma, uint32_t seed)
{
  uint32_t bst = seed, nst = seed ^ 0x9e3779b9u;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  size_t   k = 0;
  for (size_t s = 0; s < nsym; s++)
    {
      int g            = (int)(xs (&bst) % (uint32_t)m);
      labels[s]        = g;
      float complex pt = mpsk_constellation (g, m);
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          float complex c = cexpf ((float)phase * I);
          rx[k]           = pt * c;
          if (sigma > 0.0f)
            rx[k] += CMPLXF (sigma * gauss (&nst), sigma * gauss (&nst));
          phase += w;
          w += ramp * 2.0 * M_PI;
        }
    }
}

/* Nearest constellation Gray label to a (possibly rotated/scaled) symbol. */
static int
nearest_label (float complex y, int m)
{
  float         best = -1e30f;
  int           bi   = 0;
  float complex u    = y / (cabsf (y) + 1e-12f);
  for (int g = 0; g < m; g++)
    {
      float complex p = mpsk_constellation (g, m);
      float d = crealf (u * conjf (p)); /* cos(angle) — max = nearest */
      if (d > best)
        {
          best = d;
          bi   = g;
        }
    }
  return bi;
}

/* Run the loop; report tracked freq, lock, and the ambiguity-tolerant
 * symbol-error count over the converged tail (min over the m global
 * phase rotations the loop may have locked onto). */
static void
run (carrier_mpsk_state_t *c, const float complex *rx, const int *labels,
     size_t nsym, size_t tsamps, int m, double *out_freq, double *out_lock,
     int *out_symerr)
{
  float complex *sym = malloc (nsym * sizeof (*sym));
  size_t         k   = carrier_mpsk_steps (c, rx, nsym * tsamps, sym, nsym);
  *out_freq          = carrier_mpsk_get_norm_freq (c);
  *out_lock          = carrier_mpsk_get_lock_metric (c);
  size_t tail0       = k / 2;
  int    best        = (int)(k - tail0) + 1;
  for (int r = 0; r < m; r++)
    {
      float complex rot
          = cexpf (-2.0f * (float)M_PI * (float)r / (float)m * I);
      int err = 0;
      for (size_t s = tail0; s < k; s++)
        if (nearest_label (sym[s] * rot, m) != labels[s])
          err++;
      if (err < best)
        best = err;
    }
  *out_symerr = best;
  free (sym);
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, gain math, init==create parity, m validation       *
   * ---------------------------------------------------------------- */
  {
    carrier_mpsk_state_t *c
        = carrier_mpsk_create (0.05, 0.707, 0.01, 16, 0.0, 4);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    CHECK (fabs (carrier_mpsk_get_norm_freq (c) - 0.01) < 1e-12);
    CHECK (carrier_mpsk_get_m (c) == 4);

    carrier_mpsk_state_t v;
    carrier_mpsk_init (&v, 0.05, 0.707, 0.01, 16, 0.0, 4);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.nco.phase_inc == c->nco.phase_inc);
    carrier_mpsk_destroy (c);

    /* only M in {2,4,8} is a valid constellation order */
    CHECK (carrier_mpsk_create (0.05, 0.707, 0.0, 16, 0.0, 2) != NULL);
    CHECK (carrier_mpsk_create (0.05, 0.707, 0.0, 16, 0.0, 8) != NULL);
    CHECK (carrier_mpsk_create (0.05, 0.707, 0.0, 16, 0.0, 3) == NULL);
    CHECK (carrier_mpsk_create (0.05, 0.707, 0.0, 16, 0.0, 16) == NULL);
  }

  /* ---------------------------------------------------------------- *
   * 2a. Pull-in — BPSK & QPSK lock a grid of residuals (pure PLL)   *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 4000;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    int            ms[]   = { 2, 4 };
    double         f0s[]  = { 0.0, 0.001, -0.0012 };
    for (int mi = 0; mi < 2; mi++)
      for (int t = 0; t < 3; t++)
        {
          int m = ms[mi];
          make_signal (rx, labels, nsym, tsamps, m, f0s[t], 0.0, 0.0f, 13u);
          carrier_mpsk_state_t *c
              = carrier_mpsk_create (0.05, 0.707, 0.0, tsamps, 0.0, m);
          double f, lk;
          int    se;
          run (c, rx, labels, nsym, tsamps, m, &f, &lk, &se);
          CHECK (fabs (f - f0s[t]) < 3e-4);
          CHECK (lk > 0.9);
          CHECK (se == 0);
          carrier_mpsk_destroy (c);
        }
    free (rx);
    free (labels);
  }

  /* ---------------------------------------------------------------- *
   * 2b. Pull-in — 8PSK locks with the FLL assist (narrow phase       *
   * discriminator needs the wide frequency discriminator to acquire) *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 6000;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    make_signal (rx, labels, nsym, tsamps, 8, 0.0015, 0.0, 0.0f, 71u);
    carrier_mpsk_state_t *c
        = carrier_mpsk_create (0.05, 0.707, 0.0, tsamps, 0.01, 8);
    double f, lk;
    int    se;
    run (c, rx, labels, nsym, tsamps, 8, &f, &lk, &se);
    CHECK (fabs (f - 0.0015) < 5e-4);
    CHECK (lk > 0.9);
    CHECK (se == 0);
    carrier_mpsk_destroy (c);
    free (rx);
    free (labels);
  }

  /* ---------------------------------------------------------------- *
   * 3. M-fold ambiguity — QPSK output correct up to a rotation       *
   * (run() scores ambiguity-tolerant; assert zero on a fresh seed)   *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 3000;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    make_signal (rx, labels, nsym, tsamps, 4, 0.002, 0.0, 0.0f, 909u);
    carrier_mpsk_state_t *c
        = carrier_mpsk_create (0.05, 0.707, 0.0, tsamps, 0.0, 4);
    double f, lk;
    int    se;
    run (c, rx, labels, nsym, tsamps, 4, &f, &lk, &se);
    CHECK (se == 0); /* min over the 4 rotations is exact */
    CHECK (lk > 0.9);
    carrier_mpsk_destroy (c);
    free (rx);
    free (labels);
  }

  /* ---------------------------------------------------------------- *
   * 4. Noise robustness — QPSK locks under AWGN (high per-symbol SNR *
   * after the tsamps-fold coherent integration)                      *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 5000;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    make_signal (rx, labels, nsym, tsamps, 4, 0.0015, 0.0, 0.6f, 2024u);
    carrier_mpsk_state_t *c
        = carrier_mpsk_create (0.03, 0.707, 0.0, tsamps, 0.0, 4);
    double f, lk;
    int    se;
    run (c, rx, labels, nsym, tsamps, 4, &f, &lk, &se);
    CHECK (fabs (f - 0.0015) < 5e-4);
    CHECK (lk > 0.7);
    CHECK (se == 0);
    carrier_mpsk_destroy (c);
    free (rx);
    free (labels);
  }

  /* ---------------------------------------------------------------- *
   * 5. FLL assist widens pull-in beyond the bare PLL (QPSK)          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 6000;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    double         f0     = 0.006;
    make_signal (rx, labels, nsym, tsamps, 4, f0, 0.0, 0.0f, 31u);
    double f, lk;
    int    se;

    carrier_mpsk_state_t *pll
        = carrier_mpsk_create (0.01, 0.707, 0.0, tsamps, 0.0, 4);
    run (pll, rx, labels, nsym, tsamps, 4, &f, &lk, &se);
    int pll_locked = (fabs (f - f0) < 5e-4) && (lk > 0.9);
    CHECK (!pll_locked); /* the bare narrow PLL does NOT acquire it */
    carrier_mpsk_destroy (pll);

    carrier_mpsk_state_t *fll
        = carrier_mpsk_create (0.01, 0.707, 0.0, tsamps, 0.03, 4);
    run (fll, rx, labels, nsym, tsamps, 4, &f, &lk, &se);
    CHECK (fabs (f - f0) < 5e-4);
    CHECK (lk > 0.9);
    CHECK (se == 0);
    carrier_mpsk_destroy (fll);
    free (rx);
    free (labels);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility — run #2 == run #1 after reset          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 1500;
    float complex *rx     = malloc (nsym * tsamps * sizeof (*rx));
    int           *labels = malloc (nsym * sizeof (*labels));
    make_signal (rx, labels, nsym, tsamps, 8, 0.001, 0.0, 0.0f, 55u);
    carrier_mpsk_state_t *c
        = carrier_mpsk_create (0.05, 0.707, 0.0, tsamps, 0.01, 8);
    double f1, lk1;
    int    se1;
    run (c, rx, labels, nsym, tsamps, 8, &f1, &lk1, &se1);
    carrier_mpsk_reset (c);
    double f2, lk2;
    int    se2;
    run (c, rx, labels, nsym, tsamps, 8, &f2, &lk2, &se2);
    CHECK (f1 == f2 && lk1 == lk2 && se1 == se2);
    carrier_mpsk_destroy (c);
    free (rx);
    free (labels);
  }

  if (_fails)
    {
      fprintf (stderr, "test_carrier_mpsk_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_carrier_mpsk_core PASSED\n");
  return 0;
}
