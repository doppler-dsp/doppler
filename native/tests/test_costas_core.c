/**
 * @file test_costas_core.c
 * @brief Unit tests for the Costas carrier-tracking loop.
 *
 * Tests:
 *   1. Lifecycle / gain math / init==create parity
 *   2. Pull-in — locks a grid of carrier residuals (freq + lock + bits)
 *   3. 180° BPSK ambiguity is tolerated (output correct up to a global flip)
 *   4. Noise robustness — locks under AWGN
 *   5. Dynamic stress — tracks a frequency ramp (Doppler rate) with bounded
 * err
 *   6. Reset reproducibility
 */
#include "costas/costas_core.h"
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

/* Deterministic ±1 BPSK bit stream (xorshift). */
static int
prbs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return (x & 1u) ? -1 : 1;
}

/* Box-Muller unit-variance Gaussian (per component). */
static float
gauss (uint32_t *st)
{
  double u1 = (prbs (st) + 2) / 4.0; /* crude but seeded; reseed below */
  (void)u1;
  /* use two PRBS dwords mapped to (0,1) */
  uint32_t a  = (*st ^= *st << 7, *st);
  uint32_t b  = (*st ^= *st >> 9, *st);
  double   r1 = (a + 1.0) / 4294967297.0;
  double   r2 = (b + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (2.0 * M_PI * r2));
}

/* Build a continuous BPSK-at-symbol-rate signal with carrier residual f0
 * (cycles/sample), optional per-sample frequency ramp (Doppler rate), and
 * optional AWGN sigma per component.  Returns the ±1 bits used in `bits`. */
static void
make_signal (float complex *rx, int *bits, size_t nsym, size_t tsamps,
             double f0, double ramp, float sigma, uint32_t seed)
{
  uint32_t bst = seed, nst = seed ^ 0x9e3779b9u;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  size_t   k = 0;
  for (size_t s = 0; s < nsym; s++)
    {
      int b   = prbs (&bst);
      bits[s] = b;
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          float complex c = cexpf ((float)phase * I);
          rx[k]           = (float)b * c;
          if (sigma > 0.0f)
            rx[k] += CMPLXF (sigma * gauss (&nst), sigma * gauss (&nst));
          phase += w;
          w += ramp * 2.0 * M_PI; /* frequency ramps each sample */
        }
    }
}

/* Run the loop over a built signal; report tracked freq, lock, and the
 * ambiguity-tolerant bit-error count over the converged tail. */
static void
run (costas_state_t *c, const float complex *rx, const int *bits, size_t nsym,
     size_t tsamps, double *out_freq, double *out_lock, int *out_biterr)
{
  float complex *sym = malloc (nsym * sizeof (*sym));
  size_t         k   = costas_steps (c, rx, nsym * tsamps, sym, nsym);
  *out_freq          = costas_get_norm_freq (c);
  *out_lock          = costas_get_lock_metric (c);
  /* bit errors over the converged tail (last half), ambiguity-tolerant */
  size_t tail0 = k / 2;
  int    err   = 0;
  for (size_t s = tail0; s < k; s++)
    {
      int dec = (crealf (sym[s]) >= 0.0f) ? 1 : -1;
      if (dec != bits[s])
        err++;
    }
  int n       = (int)(k - tail0);
  *out_biterr = err < n - err ? err : n - err; /* min(err, n-err) */
  free (sym);
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, gain math, init==create parity                     *
   * ---------------------------------------------------------------- */
  {
    costas_state_t *c = costas_create (0.05, 0.707, 0.01, 16);
    CHECK (c != NULL);
    if (!c)
      return 1;
    /* gains derive from the embedded loop_filter (bn,zeta,t=1) */
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    /* seeded NCO frequency == requested residual */
    CHECK (fabs (costas_get_norm_freq (c) - 0.01) < 1e-12);

    costas_state_t v;
    costas_init (&v, 0.05, 0.707, 0.01, 16);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.nco.phase_inc == c->nco.phase_inc);
    costas_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 2. Pull-in — a grid of carrier residuals locks                   *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 4000;
    float complex *rx    = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits  = malloc (nsym * sizeof (*bits));
    double         f0s[] = { 0.0, 0.001, 0.003, -0.004 };
    for (int t = 0; t < 4; t++)
      {
        make_signal (rx, bits, nsym, tsamps, f0s[t], 0.0, 0.0f, 12345u);
        costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps);
        double          f, lk;
        int             be;
        run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
        CHECK (fabs (f - f0s[t]) < 2e-4); /* tracked the residual    */
        CHECK (lk > 0.9);                 /* phase-locked            */
        CHECK (be == 0);                  /* zero bit errors on tail */
        costas_destroy (c);
      }
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 3. 180° BPSK ambiguity — output is correct up to a global flip   *
   * (run() already scores ambiguity-tolerant; assert a flipped seed) *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 3000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 777u);
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (be == 0); /* min(err, n-err)==0 even if globally inverted  */
    CHECK (lk > 0.9);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 4. Noise robustness — locks under AWGN (per-symbol SNR is high   *
   * after tsamps-fold coherent integration)                          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 5000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    /* sigma=1.0 per component → ~ -3 dB per-sample SNR; +12 dB from the
     * 16-fold I&D → comfortably locked. */
    make_signal (rx, bits, nsym, tsamps, 0.0015, 0.0, 1.0f, 2024u);
    costas_state_t *c = costas_create (0.03, 0.707, 0.0, tsamps);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (fabs (f - 0.0015) < 5e-4);
    CHECK (lk > 0.7);
    CHECK (be == 0);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 5. Dynamic stress — track a frequency ramp (Doppler rate)        *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 6000;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    /* start at 0, ramp the carrier to a final offset; 2nd-order loop
     * tracks a constant rate with bounded (small) steady error. */
    double ramp = 5e-9; /* cycles/sample per sample */
    make_signal (rx, bits, nsym, tsamps, 0.0, ramp, 0.0f, 99u);
    double          final_f0 = ramp * (double)(nsym * tsamps);
    costas_state_t *c        = costas_create (0.06, 0.707, 0.0, tsamps);
    double          f, lk;
    int             be;
    run (c, rx, bits, nsym, tsamps, &f, &lk, &be);
    CHECK (fabs (f - final_f0) < 1e-3); /* follows the moving carrier */
    CHECK (lk > 0.85);
    CHECK (be == 0);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility — run #2 == run #1 after reset          *
   * ---------------------------------------------------------------- */
  {
    const size_t   tsamps = 16, nsym = 1500;
    float complex *rx   = malloc (nsym * tsamps * sizeof (*rx));
    int           *bits = malloc (nsym * sizeof (*bits));
    make_signal (rx, bits, nsym, tsamps, 0.002, 0.0, 0.0f, 55u);
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, tsamps);
    double          f1, lk1;
    int             be1;
    run (c, rx, bits, nsym, tsamps, &f1, &lk1, &be1);
    costas_reset (c);
    double f2, lk2;
    int    be2;
    run (c, rx, bits, nsym, tsamps, &f2, &lk2, &be2);
    CHECK (f1 == f2 && lk1 == lk2 && be1 == be2);
    costas_destroy (c);
    free (rx);
    free (bits);
  }

  if (_fails)
    {
      fprintf (stderr, "test_costas_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_costas_core PASSED\n");
  return 0;
}
