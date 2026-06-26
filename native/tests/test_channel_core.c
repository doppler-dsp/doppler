/**
 * @file test_channel_core.c
 * @brief Unit tests for the GPS-style tracking channel (Costas + DLL).
 *
 * Tests:
 *   1. Lifecycle / NULL-code guard / init==create parity
 *   2. Full receiver — small residual: carrier + code lock, zero BER (steps)
 *   3. FLL assist — a residual the bare PLL misses is locked
 *   4. Hard bits (nav_period = 1) match the data, up to a global flip
 *   5. Bit-sync (nav_period > 1) recovers nav bits + detects the boundary
 *   6. Reset reproducibility
 */
#include "channel/channel_core.h"
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

static void
make_code (uint8_t *code, size_t sf, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < sf; i++)
    code[i] = prbs (&st) > 0 ? 0u : 1u;
}

/* Build a continuous DSSS-BPSK signal: PN code x BPSK data (one data bit every
 * nav_period code periods) x carrier exp(j 2pi f0 n), optional AWGN sigma.
 * Fills `data` with the +-1 bit per nav symbol.  Returns sample count. */
static size_t
make_signal (float complex *rx, int *data, const uint8_t *code, size_t sf,
             size_t sps, size_t nper, size_t nav_period, double f0,
             float sigma, uint32_t seed)
{
  uint32_t dst = seed ^ 0x5bd1e995u, nst = seed;
  size_t   k     = 0;
  double   cph   = 0.0;
  double   phase = 0.0, w = f0 * 2.0 * M_PI;
  int      bit = prbs (&dst);
  for (size_t p = 0; p < nper; p++)
    {
      if (p % nav_period == 0) /* new data bit at the nav boundary */
        bit = prbs (&dst);
      data[p / nav_period] = bit;
      for (size_t i = 0; i < sf * sps; i++, k++)
        {
          size_t        idx  = (size_t)fmod (cph, (double)sf);
          float         csgn = (code[idx] & 1u) ? -1.0f : 1.0f;
          float complex s    = (float)bit * csgn * cexpf ((float)phase * I);
          if (sigma > 0.0f)
            {
              float gr = 0, gi = 0;
              for (int j = 0; j < 4; j++)
                gr += (float)prbs (&nst);
              for (int j = 0; j < 4; j++)
                gi += (float)prbs (&nst);
              s += CMPLXF (sigma * gr * 0.5f, sigma * gi * 0.5f);
            }
          rx[k] = s;
          cph += 1.0 / (double)sps;
          phase += w;
        }
    }
  return k;
}

/* ambiguity-tolerant bit-error count of decisions vs truth over [lo,hi) */
static int
amb_errors (const int *dec, const int *truth, size_t lo, size_t hi)
{
  int err = 0, n = (int)(hi - lo);
  for (size_t i = lo; i < hi; i++)
    if (dec[i] != truth[i])
      err++;
  return err < n - err ? err : n - err;
}

int
main (void)
{
  int          _fails = 0;
  const size_t sf = 127, sps = 8, tsamps = sf * sps;

  /* 1. Lifecycle / guard / parity */
  {
    CHECK (channel_create (NULL, 0, sps, 0.0, 0.0, 0.05, 0.005, 0.0, 0.707,
                           0.5, 1)
           == NULL);
    uint8_t code[127];
    make_code (code, sf, 1u);
    channel_state_t *c = channel_create (code, sf, sps, 0.001, 0.0, 0.05,
                                         0.005, 0.0, 0.707, 0.5, 1);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (fabs (channel_get_norm_freq (c) - 0.001) < 1e-9); /* seeded */
    CHECK (channel_get_code_rate (c) == 1.0);
    channel_state_t v;
    channel_init (&v, code, sf, sps, 0.001, 0.0, 0.05, 0.005, 0.0, 0.707, 0.5,
                  1);
    CHECK (v.car.lf.kp == c->car.lf.kp);
    CHECK (v.code.sf == sf && v.code.owns_code == 0);
    free (v.flip_hist);
    channel_destroy (c);
  }

  /* 2. Full receiver — small residual locks, zero BER on the tail */
  {
    const size_t nper = 500;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 7u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 5e-5, 0.0f, 3u);

    channel_state_t *c = channel_create (code, sf, sps, 0.0, 0.0, 0.05, 0.005,
                                         0.0, 0.707, 0.5, 1);
    float complex   *sym = malloc (nper * sizeof (*sym));
    size_t           k   = channel_steps (c, rx, n, sym, nper);
    CHECK (fabs (channel_get_norm_freq (c) - 5e-5) < 1e-5);
    CHECK (channel_get_lock_metric (c) > 0.9);
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    channel_destroy (c);
    free (rx);
    free (data);
    free (sym);
    free (dec);
    free (code);
  }

  /* 3. FLL assist — a 0.2 cyc/epoch residual the bare PLL misses */
  {
    const size_t nper = 700;
    double       f0   = 0.2 / (double)tsamps; /* 0.2 cycles per code period */
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 9u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, f0, 0.0f, 11u);

    channel_state_t *pll = channel_create (code, sf, sps, 0.0, 0.0, 0.05,
                                           0.005, 0.0, 0.707, 0.5, 1);
    float complex   *sym = malloc (nper * sizeof (*sym));
    channel_steps (pll, rx, n, sym, nper);
    CHECK (channel_get_lock_metric (pll) < 0.8); /* bare PLL misses it */
    channel_destroy (pll);

    channel_state_t *fll = channel_create (code, sf, sps, 0.0, 0.0, 0.05,
                                           0.005, 0.03, 0.707, 0.5, 1);
    size_t           k   = channel_steps (fll, rx, n, sym, nper);
    CHECK (fabs (channel_get_norm_freq (fll) - f0) < 2e-5);
    CHECK (channel_get_lock_metric (fll) > 0.9);
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    channel_destroy (fll);
    free (rx);
    free (data);
    free (sym);
    free (dec);
    free (code);
  }

  /* 4. Hard bits (nav_period = 1) match the data */
  {
    const size_t nper = 400;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 13u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 4e-5, 0.0f, 17u);

    channel_state_t *c = channel_create (code, sf, sps, 0.0, 0.0, 0.05, 0.005,
                                         0.0, 0.707, 0.5, 1);
    uint8_t         *bits = malloc (nper);
    size_t           k    = channel_bits (c, rx, n, bits, nper);
    int             *dec  = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = bits[i] ? 1 : -1;
    CHECK (amb_errors (dec, data, k / 2, k) == 0);
    channel_destroy (c);
    free (rx);
    free (data);
    free (bits);
    free (dec);
    free (code);
  }

  /* 5. Bit-sync (nav_period = 20) recovers nav bits + detects the boundary */
  {
    const size_t N = 20, nbits = 120, nper = N * nbits;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 19u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nbits * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, N, 3e-5, 0.0f, 23u);

    channel_state_t *c = channel_create (code, sf, sps, 0.0, 0.0, 0.05, 0.005,
                                         0.0, 0.707, 0.5, N);
    uint8_t         *bits = malloc (nbits);
    size_t           k    = channel_bits (c, rx, n, bits, nbits);
    CHECK (k >= nbits - 3); /* ~one bit per nav_period periods */
    CHECK (channel_get_bit_phase (c) == 0); /* boundary at epoch 0 */
    int *dec = malloc (k * sizeof (int));
    for (size_t i = 0; i < k; i++)
      dec[i] = bits[i] ? 1 : -1;
    /* tail: after bit-sync settles, recovered bits match the data */
    CHECK (amb_errors (dec, data, k / 3, k) == 0);
    channel_destroy (c);
    free (rx);
    free (data);
    free (bits);
    free (dec);
    free (code);
  }

  /* 6. Reset reproducibility */
  {
    const size_t nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 21u);
    float complex *rx   = malloc (tsamps * nper * sizeof (*rx));
    int           *data = malloc (nper * sizeof (*data));
    size_t n = make_signal (rx, data, code, sf, sps, nper, 1, 5e-5, 0.0f, 5u);

    channel_state_t *c = channel_create (code, sf, sps, 0.0, 0.0, 0.05, 0.005,
                                         0.0, 0.707, 0.5, 1);
    float complex   *sym = malloc (nper * sizeof (*sym));
    channel_steps (c, rx, n, sym, nper);
    double f1 = channel_get_norm_freq (c), l1 = channel_get_lock_metric (c);
    channel_reset (c);
    channel_steps (c, rx, n, sym, nper);
    CHECK (f1 == channel_get_norm_freq (c));
    CHECK (l1 == channel_get_lock_metric (c));
    channel_destroy (c);
    free (rx);
    free (data);
    free (sym);
    free (code);
  }

  if (_fails)
    {
      fprintf (stderr, "test_channel_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_channel_core PASSED\n");
  return 0;
}
