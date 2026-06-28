/**
 * @file test_symsync_core.c
 * @brief Unit tests for the Gardner symbol-timing synchronizer.
 *
 * Tests:
 *   1. Lifecycle / order / init parity / reset reproducibility
 *   2. Lock across a range of static timing offsets -> zero BER
 *   3. Clock-rate (asynchronous) tracking -> zero BER + recovered rate
 *   4. All three interpolator orders lock
 */
#include "symsync/symsync_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define NSYM 2000
#define SPS 4

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

/* Nyquist raised-cosine pulse (matched-filtered), unit symbol period T. */
static double
rc (double t, double beta, double T)
{
  double x = t / T;
  double s = (fabs (x) < 1e-9) ? 1.0 : sin (M_PI * x) / (M_PI * x);
  double d = 1.0 - (2.0 * beta * x) * (2.0 * beta * x);
  double c = (fabs (d) < 1e-9) ? M_PI / 4.0 : cos (M_PI * beta * x) / d;
  return s * c;
}

/* Build an RC-shaped BPSK signal at SPS samples/symbol with a fractional
 * timing `offset` (samples) and a clock-rate scale `rate`.  Fills bits[]. */
static size_t
make_signal (float complex *rx, int *bits, size_t nsym, double offset,
             double rate, uint32_t seed)
{
  size_t   n    = nsym * SPS;
  uint32_t st   = seed;
  double   beta = 0.35, T = SPS, span = 8 * SPS;
  for (size_t i = 0; i < n; i++)
    rx[i] = 0.0f;
  for (size_t k = 0; k < nsym; k++)
    {
      int b    = prbs (&st);
      bits[k]  = b; /* fill every bit so the BER tail is always valid */
      double c = (double)k * SPS * rate + offset;
      if (c + span >= (double)n)
        continue; /* signal ran out, but keep populating bits[] */
      long lo = (long)(c - span), hi = (long)(c + span);
      if (lo < 0)
        lo = 0;
      for (long i = lo; i <= hi && i < (long)n; i++)
        rx[i] += (float)(b * rc ((double)i - c, beta, T));
    }
  return n;
}

/* Ambiguity- and lag-tolerant BER over a clean, fully-locked middle window
 * (avoids the acquisition transient and the low-signal tail). */
static double
tail_ber (const float complex *sym, size_t nsym, const int *bits, size_t nbits)
{
  size_t lo_i = nsym / 4, hi_i = nsym - nsym / 4; /* middle half */
  double best = 1.0;
  for (int lag = -80; lag <= 80; lag++)
    {
      int err = 0, cnt = 0;
      for (size_t i = lo_i; i < hi_i; i++)
        {
          long j = (long)i + lag;
          if (j < 0 || j >= (long)nbits)
            continue;
          int dec = (crealf (sym[i]) >= 0.0f) ? 1 : -1;
          err += (dec != bits[j]);
          cnt++;
        }
      if (cnt < 100)
        continue;
      double b = (double)err / cnt;
      if (b > 0.5)
        b = 1.0 - b; /* global inversion is don't-care */
      if (b < best)
        best = b;
    }
  return best;
}

int
main (void)
{
  int            _fails = 0;
  float complex *rx     = malloc (NSYM * SPS * sizeof (*rx));
  int           *bits   = malloc (NSYM * sizeof (int));
  float complex *sym    = malloc (NSYM * sizeof (*sym));

  /* 1. Lifecycle / order / reset reproducibility */
  {
    symsync_state_t *s = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC);
    CHECK (s != NULL);
    if (!s)
      return 1;
    CHECK (s->farrow.order == FARROW_CUBIC);
    CHECK (fabs (symsync_get_bn (s) - 0.01) < 1e-12);
    make_signal (rx, bits, NSYM, 1.3, 1.0, 3u);
    size_t k1 = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
    double r1 = symsync_get_rate (s);
    symsync_reset (s);
    size_t k2 = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
    CHECK (k1 == k2);
    CHECK (symsync_get_rate (s) == r1);
    symsync_destroy (s);
  }

  /* 2. Lock across static timing offsets */
  {
    for (int oi = 0; oi < 8; oi++)
      {
        double           off = oi * (double)SPS / 8.0;
        symsync_state_t *s   = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC);
        make_signal (rx, bits, NSYM, off, 1.0, 7u);
        size_t k   = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        double ber = tail_ber (sym, k, bits, NSYM);
        CHECK (ber == 0.0);
        symsync_destroy (s);
      }
  }

  /* 3. Clock-rate (asynchronous) tracking */
  {
    double rates[3] = { 1.0, 1.005, 0.995 };
    for (int ri = 0; ri < 3; ri++)
      {
        symsync_state_t *s = symsync_create (SPS, 0.005, 0.707, FARROW_CUBIC);
        make_signal (rx, bits, NSYM, 1.3, rates[ri], 11u);
        size_t k   = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        double ber = tail_ber (sym, k, bits, NSYM);
        CHECK (ber == 0.0);
        /* recovered samples/symbol tracks the true clock rate to ~1% */
        CHECK (fabs (symsync_get_rate (s) - SPS * rates[ri]) < 0.05);
        symsync_destroy (s);
      }
  }

  /* 4. All three interpolator orders lock */
  {
    for (int order = 0; order <= 2; order++)
      {
        symsync_state_t *s = symsync_create (SPS, 0.01, 0.707, order);
        make_signal (rx, bits, NSYM, 1.7, 1.0, 13u);
        size_t k = symsync_steps (s, rx, NSYM * SPS, sym, NSYM);
        CHECK (tail_ber (sym, k, bits, NSYM) == 0.0);
        symsync_destroy (s);
      }
  }

  /* symsync_init() (by-value, in place) produces a state byte-for-byte
   * identical to symsync_create()'s calloc + init — including a stack-embedded
   * target with arbitrary prior contents (symsync_init memsets first). The
   * whole-struct memcmp IS the init==create contract: identical state implies
   * identical behaviour. (A per-sample stream compare would be fragile here —
   * the compiler inlines symsync_step separately for the heap and stack
   * instances and may contract FMAs differently between the two, ~1 ULP, which
   * is a codegen artifact, not a state difference.) */
  {
    symsync_state_t *c = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC);
    /* poison the target so memset-or-not is actually exercised */
    symsync_state_t v;
    memset (&v, 0xFF, sizeof v);
    symsync_init (&v, SPS, 0.01, 0.707, FARROW_CUBIC);
    CHECK (memcmp (c, &v, sizeof *c) == 0); /* init == create, byte-for-byte */
    symsync_destroy (c);
  }

  free (rx);
  free (bits);
  free (sym);
  if (_fails)
    {
      fprintf (stderr, "test_symsync_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_symsync_core PASSED\n");
  return 0;
}
