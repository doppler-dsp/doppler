/**
 * @file test_dll_core.c
 * @brief Unit tests for the DLL (early/prompt/late code-tracking loop).
 *
 * Tests:
 *   1. Lifecycle / NULL-code guard / init==create parity
 *   2. On-time alignment — discriminator ~0, code_rate ~1
 *   3. Code Doppler — code_rate converges to the incoming chip rate
 *   4. Static phase offset is pulled in (discriminator decays)
 *   5. Reset reproducibility
 */
#include "dll/dll_core.h"
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

/* xorshift ±1 BPSK data bit (one per code period). */
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

/* Build a deterministic 0/1 spreading code (xorshift bits). */
static void
make_code (uint8_t *code, size_t sf, uint32_t seed)
{
  uint32_t st = seed;
  for (size_t i = 0; i < sf; i++)
    code[i] = prbs (&st) > 0 ? 0u : 1u;
}

/* Carrier-free spread signal at code rate (1+delta), random BPSK data per
 * period. `sps` samples per nominal chip. Returns the sample count. */
static size_t
make_signal (float complex *rx, const uint8_t *code, size_t sf, size_t sps,
             double delta, size_t nper, uint32_t seed)
{
  uint32_t dst    = seed;
  size_t   tsamps = sf * sps;
  double   inv    = 1.0 / (double)sps;
  int      data   = prbs (&dst);
  size_t   k      = 0;
  double   cph    = 0.0; /* incoming code phase, chips */
  for (size_t p = 0; p < nper; p++)
    {
      data = prbs (&dst);
      for (size_t i = 0; i < tsamps; i++, k++)
        {
          size_t idx  = (size_t)fmod (cph, (double)sf);
          float  csgn = (code[idx] & 1u) ? -1.0f : 1.0f;
          rx[k]       = (float)data * csgn;
          cph += inv * (1.0 + delta); /* incoming chip rate */
        }
    }
  return k;
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, NULL-code guard, init==create parity               *
   * ---------------------------------------------------------------- */
  {
    CHECK (dll_create (NULL, 0, 2, 0.0, 0.01, 0.707, 0.5) == NULL);

    uint8_t code[31];
    make_code (code, 31, 1u);
    dll_state_t *c = dll_create (code, 31, 2, 0.0, 0.02, 0.707, 0.5);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    CHECK (c->sf == 31 && c->sps == 2);
    CHECK (dll_get_code_rate (c) == 1.0);

    dll_state_t v;
    dll_init (&v, code, 31, 2, 0.0, 0.02, 0.707, 0.5);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.owns_code == 0);  /* init borrows */
    CHECK (c->owns_code == 1); /* create copies */
    dll_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 2. On-time alignment — E ~ L, discriminator ~0, code_rate ~1     *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 400;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 7u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 0.0, nper, 3u);

    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5);
    float complex *sym = malloc (nper * sizeof (*sym));
    size_t         k   = dll_steps (d, rx, n, sym, nper);
    CHECK (k >= nper - 2 && k <= nper);           /* ~one prompt per period */
    CHECK (fabs (dll_get_last_error (d)) < 0.05); /* E ~ L on-time */
    CHECK (fabs (dll_get_code_rate (d) - 1.0) < 1e-3);
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 3. Code Doppler — code_rate converges to the incoming chip rate  *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 1500;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 11u);
    float complex *rx    = malloc (sf * sps * nper * sizeof (*rx));
    double         delta = 5e-4; /* incoming code runs 0.05% fast */
    size_t         n     = make_signal (rx, code, sf, sps, delta, nper, 9u);

    /* half-chip E/L discriminator is steep — keep the loop BW low. */
    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.005, 0.707, 0.5);
    float complex *sym = malloc (nper * sizeof (*sym));
    dll_steps (d, rx, n, sym, nper);
    /* the loop must speed its replica up to match the incoming rate */
    CHECK (fabs (dll_get_code_rate (d) - (1.0 + delta)) < 1e-4);
    CHECK (fabs (dll_get_last_error (d)) < 0.1); /* stays locked */
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 4. Static phase offset is pulled in (discriminator decays)       *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 63, sps = 4, nper = 800;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 13u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 0.0, nper, 17u);

    /* seed the replica 0.4 chips off — the loop must realign it */
    dll_state_t   *d   = dll_create (code, sf, sps, 0.4, 0.005, 0.707, 0.5);
    float complex *sym = malloc (nper * sizeof (*sym));
    /* early discriminator (first few periods) should be non-trivial */
    dll_steps (d, rx, sf * sps * 3, sym, 3);
    double early_err = fabs (dll_get_last_error (d));
    dll_steps (d, rx + sf * sps * 3, n - sf * sps * 3, sym, nper);
    double late_err = fabs (dll_get_last_error (d));
    CHECK (early_err > 0.05); /* started misaligned */
    CHECK (late_err < 0.05);  /* pulled in */
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  /* ---------------------------------------------------------------- *
   * 5. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    const size_t sf = 31, sps = 2, nper = 300;
    uint8_t     *code = malloc (sf);
    make_code (code, sf, 21u);
    float complex *rx = malloc (sf * sps * nper * sizeof (*rx));
    size_t         n  = make_signal (rx, code, sf, sps, 3e-4, nper, 5u);

    dll_state_t   *d   = dll_create (code, sf, sps, 0.0, 0.02, 0.707, 0.5);
    float complex *sym = malloc (nper * sizeof (*sym));
    dll_steps (d, rx, n, sym, nper);
    double r1 = dll_get_code_rate (d), e1 = dll_get_last_error (d);
    dll_reset (d);
    dll_steps (d, rx, n, sym, nper);
    double r2 = dll_get_code_rate (d), e2 = dll_get_last_error (d);
    CHECK (r1 == r2 && e1 == e2);
    dll_destroy (d);
    free (rx);
    free (sym);
    free (code);
  }

  if (_fails)
    {
      fprintf (stderr, "test_dll_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_dll_core PASSED\n");
  return 0;
}
