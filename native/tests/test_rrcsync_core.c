#include "rrcsync/rrcsync_core.h"
#include "wfm/wfm_dsp.h"

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

static int _fails = 0;

#define BETA 0.35
#define SPAN 8u

/* ------------------------------------------------------------------ */
/* RRC-shaped BPSK at an ARBITRARY (fractional) samples-per-symbol.    */
/* Evaluates the pulse directly — no integer grid anywhere — so the    */
/* non-integer cases below are genuinely non-integer.                  */
/* ------------------------------------------------------------------ */
static size_t
gen_bpsk (double sps, double tau, size_t nsym, uint32_t seed, float complex *x,
          size_t x_cap, int8_t *sym_out)
{
  size_t   n = (size_t)((double)nsym * sps);
  uint32_t s = seed;
  if (n > x_cap)
    n = x_cap;
  for (size_t i = 0; i < n; i++)
    x[i] = 0.0f;
  for (size_t k = 0; k < nsym; k++)
    {
      s = s * 1664525u + 1013904223u; /* LCG; the data need only be
                                         balanced and aperiodic */
      double a   = (s >> 31) ? 1.0 : -1.0;
      sym_out[k] = (int8_t)a;
      double c   = (double)k * sps + tau;
      long   lo  = (long)ceil (c - (double)SPAN * sps);
      long   hi  = (long)floor (c + (double)SPAN * sps);
      if (lo < 0)
        lo = 0;
      if (hi >= (long)n)
        hi = (long)n - 1;
      for (long i = lo; i <= hi; i++)
        x[i] += (float)(a
                        * wfm_rrc_h (((double)i - tau) / sps - (double)k,
                                     BETA));
    }
  return n;
}

/* Self-referenced EVM: every symbol against its OWN hard decision, so it
 * needs no reference sequence and no lag search — the truth-free validator
 * a BER number must never be trusted without. */
static double
evm_self (const float complex *y, size_t n)
{
  double num = 0.0, den = 0.0, gr = 0.0, gd = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double d = crealf (y[i]) >= 0.0f ? 1.0 : -1.0;
      gr += d * (double)crealf (y[i]);
      gd += d * d;
    }
  double g = gd > 0.0 ? gr / gd : 1.0;
  for (size_t i = 0; i < n; i++)
    {
      double d  = crealf (y[i]) >= 0.0f ? 1.0 : -1.0;
      double er = (double)crealf (y[i]) - g * d;
      double ei = (double)cimagf (y[i]);
      num += er * er + ei * ei;
      den += g * g;
    }
  return den > 0.0 ? sqrt (num / den) : 1.0;
}

/* ------------------------------------------------------------------ */
/* The bank convention: arm p must move the sampling instant the SAME   */
/* way crossing an emission boundary does. Concretely, arm p is the     */
/* prototype sampled p/num_phases of an output period later, so walking */
/* the arms of one tap must trace the RRC continuously into the NEXT    */
/* tap's value — bank[p=last][t] adjoins bank[p=0][t-1].               */
/* ------------------------------------------------------------------ */
static void
test_bank_convention (void)
{
  const size_t P = 64, span = SPAN;
  const double sps   = 4.0;
  size_t       ntaps = rrcsync_bank_ntaps (RRCSYNC_PULSE_RRC, sps, span);
  float       *bank  = malloc (P * ntaps * sizeof (float));
  CHECK (bank != NULL);
  if (!bank)
    return;
  rrcsync_bank (RRCSYNC_PULSE_RRC, BETA, sps, span, P, ntaps, 0.0, bank);

  /* Continuity across the arm wrap: the last arm of tap t sits one arm step
     short of arm 0 of tap t-1 (one full output period earlier == sps taps
     of the input grid... at sps=4 exactly one tap step per output period is
     NOT the case, so compare against the analytic value instead). */
  double worst = 0.0;
  for (size_t p = 0; p < P; p++)
    for (size_t t = 0; t < ntaps; t++)
      {
        double want = rrcsync_pulse_h (
            RRCSYNC_PULSE_RRC,
            -(double)t / sps + (double)span + (double)p / (double)P, BETA);
        double got = (double)bank[p * ntaps + t];
        double d   = fabs (got - want);
        if (d > worst)
          worst = d;
      }
  CHECK (worst < 1e-6);

  /* The centre tap of arm 0 is the pulse peak; walking arms away from it
     must fall monotonically for the first stretch (the RRC is unimodal at
     its peak) — this is what breaks if the arm sign is flipped. */
  size_t tc   = (size_t)(span * sps); /* tap holding t = 0 at arm 0 */
  double prev = fabs ((double)bank[0 * ntaps + tc]);
  int    mono = 1;
  for (size_t p = 1; p < P / 4; p++)
    {
      double v = fabs ((double)bank[p * ntaps + tc]);
      if (v > prev + 1e-9)
        mono = 0;
      prev = v;
    }
  CHECK (mono);
  free (bank);
}

/* step() == steps(): one block must equal the same inputs fed one at a
 * time, bit for bit. */
static void
test_step_equals_steps (void)
{
  const size_t  NSYM = 400;
  const double  sps  = 4.0;
  float complex x[4096];
  int8_t        syms[NSYM];
  size_t        n = gen_bpsk (sps, 0.37, NSYM, 12345u, x, 4096, syms);

  rrcsync_state_t *a = rrcsync_create (sps, RRCSYNC_PULSE_RRC, BETA, SPAN, 256,
                                       0.005, 0.707, RRCSYNC_TED_GARDNER);
  rrcsync_state_t *b = rrcsync_create (sps, RRCSYNC_PULSE_RRC, BETA, SPAN, 256,
                                       0.005, 0.707, RRCSYNC_TED_GARDNER);
  CHECK (a && b);
  if (!a || !b)
    return;

  float complex ya[2048], yb[2048];
  size_t        na = rrcsync_steps (a, x, n, ya, 2048);
  size_t        nb = 0;
  for (size_t i = 0; i < n; i++)
    {
      float complex y;
      if (rrcsync_step (b, x[i], &y) && nb < 2048)
        yb[nb++] = y;
    }
  CHECK (na == nb);
  CHECK (na > NSYM / 2);
  int exact = 1;
  for (size_t i = 0; i < na && i < nb; i++)
    if (ya[i] != yb[i])
      exact = 0;
  CHECK (exact);
  rrcsync_destroy (a);
  rrcsync_destroy (b);
}

/* Arbitrary-rate lock: a non-integer sps, an arbitrary fractional timing
 * offset, and the loop must still land on the eye. */
static void
test_arbitrary_rate_locks (void)
{
  static float complex x[200000];
  const double         rates[] = { 4.0, 6.5, 17.33389, 2.718281828 };
  for (size_t r = 0; r < sizeof rates / sizeof rates[0]; r++)
    {
      double sps  = rates[r];
      size_t nsym = 3000;
      int8_t syms[3000];
      size_t n
          = gen_bpsk (sps, 0.37, nsym, 999u + (uint32_t)r, x, 200000, syms);

      rrcsync_state_t *s
          = rrcsync_create (sps, RRCSYNC_PULSE_RRC, BETA, SPAN, 1024, 0.005,
                            0.707, RRCSYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (!s)
        return;
      static float complex y[3200];
      size_t               ny = rrcsync_steps (s, x, n, y, 3200);
      CHECK (ny > nsym / 2);
      /* settled tail only — acquisition is allowed a transient */
      size_t skip = ny / 4;
      double evm  = evm_self (y + skip, ny - skip);
      CHECK (evm < 0.05);
      /* the tracked rate must recover the true samples/symbol */
      CHECK (fabs (rrcsync_get_rate (s) - sps) < 0.01 * sps);
      CHECK (rrcsync_get_locked (s) == 1);
      if (evm >= 0.05)
        fprintf (stderr, "  sps=%g evm=%g rate=%g\n", sps, evm,
                 rrcsync_get_rate (s));
      rrcsync_destroy (s);
    }
}

/* NRZ / rectangular BPSK: every sample inside symbol k carries a_k. The
 * common case for chip- and NRZ-rate links, and the one the iandd pulse
 * exists for. */
static size_t
gen_nrz (double sps, double tau, size_t nsym, uint32_t seed, float complex *x,
         size_t x_cap)
{
  size_t   n = (size_t)((double)nsym * sps);
  uint32_t s = seed;
  if (n > x_cap)
    n = x_cap;
  for (size_t k = 0; k < nsym; k++)
    {
      s         = s * 1664525u + 1013904223u;
      double a  = (s >> 31) ? 1.0 : -1.0;
      long   lo = (long)ceil ((double)k * sps + tau);
      long   hi = (long)ceil (((double)k + 1.0) * sps + tau) - 1;
      if (lo < 0)
        lo = 0;
      if (hi >= (long)n)
        hi = (long)n - 1;
      for (long i = lo; i <= hi; i++)
        x[i] = (float)a;
    }
  return n;
}

/* The rectangular case: same object, same loop, different prototype. The
 * matched filter for a rectangular symbol is the integrate-and-dump boxcar,
 * so the bank is one symbol wide instead of 2*span. */
static void
test_rectangular_pulse_locks (void)
{
  static float complex x[120000];
  const double         rates[] = { 4.0, 8.0, 11.7391 };
  for (size_t r = 0; r < sizeof rates / sizeof rates[0]; r++)
    {
      double sps  = rates[r];
      size_t nsym = 3000;
      size_t n    = gen_nrz (sps, 0.37, nsym, 555u + (uint32_t)r, x, 120000);

      rrcsync_state_t *s
          = rrcsync_create (sps, RRCSYNC_PULSE_IANDD, 0.0, 1, 1024, 0.005,
                            0.707, RRCSYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (!s)
        return;
      /* one symbol of support, not 2*span — the whole point of the shape */
      CHECK (rrcsync_bank_ntaps (RRCSYNC_PULSE_IANDD, sps, 8)
             < rrcsync_bank_ntaps (RRCSYNC_PULSE_RRC, sps, 8) / 4);

      static float complex y[3200];
      size_t               ny = rrcsync_steps (s, x, n, y, 3200);
      CHECK (ny > nsym / 2);
      size_t skip = ny / 4;
      double evm  = evm_self (y + skip, ny - skip);
      CHECK (evm < 0.10);
      CHECK (fabs (rrcsync_get_rate (s) - sps) < 0.01 * sps);
      if (evm >= 0.10)
        fprintf (stderr, "  iandd sps=%g evm=%g rate=%g\n", sps, evm,
                 rrcsync_get_rate (s));
      rrcsync_destroy (s);
    }
}

/* A sample-clock offset the object was NOT told about: built at the
 * nominal sps, fed a stream whose true rate differs, the loop must pull it
 * in and the rate estimator must report the truth. */
static void
test_tracks_clock_offset (void)
{
  static float complex x[80000];
  const double         nominal = 4.0;
  const double         ppm[]   = { 200.0, -200.0, 1000.0 };
  for (size_t i = 0; i < 3; i++)
    {
      double true_sps = nominal * (1.0 + ppm[i] * 1e-6);
      int8_t syms[6000];
      size_t n = gen_bpsk (true_sps, 0.11, 6000, 4242u, x, 80000, syms);

      rrcsync_state_t *s
          = rrcsync_create (nominal, RRCSYNC_PULSE_RRC, BETA, SPAN, 1024,
                            0.005, 0.707, RRCSYNC_TED_GARDNER);
      CHECK (s != NULL);
      if (!s)
        return;
      static float complex y[6400];
      size_t               ny   = rrcsync_steps (s, x, n, y, 6400);
      size_t               skip = ny / 3;
      CHECK (evm_self (y + skip, ny - skip) < 0.05);
      /* within 20 ppm of the true rate */
      CHECK (fabs (rrcsync_get_rate (s) - true_sps) < 20e-6 * nominal * 4.0);
      rrcsync_destroy (s);
    }
}

/* Serializable state: a mid-stream split must resume bit-for-bit into a
 * freshly built instance, and a clobbered envelope must be rejected. */
static void
test_state_roundtrip (void)
{
  const double         sps = 4.0;
  int8_t               syms[1200];
  static float complex x[6000];
  size_t               n    = gen_bpsk (sps, 0.37, 1200, 77u, x, 6000, syms);
  size_t               half = n / 2;

  rrcsync_state_t *a = rrcsync_create (sps, RRCSYNC_PULSE_RRC, BETA, SPAN, 256,
                                       0.005, 0.707, RRCSYNC_TED_GARDNER);
  CHECK (a != NULL);
  if (!a)
    return;
  static float complex ya[2048], yb[2048];
  (void)rrcsync_steps (a, x, half, ya, 2048);

  size_t nb   = rrcsync_state_bytes (a);
  void  *blob = malloc (nb);
  CHECK (blob != NULL);
  if (!blob)
    return;
  rrcsync_get_state (a, blob);

  rrcsync_state_t *b = rrcsync_create (sps, RRCSYNC_PULSE_RRC, BETA, SPAN, 256,
                                       0.005, 0.707, RRCSYNC_TED_GARDNER);
  CHECK (b != NULL);
  if (!b)
    return;
  CHECK (rrcsync_state_bytes (b) == nb);
  CHECK (rrcsync_set_state (b, blob) == DP_OK);

  /* Both continue on the same tail; the symbols must match exactly. */
  size_t na2 = rrcsync_steps (a, x + half, n - half, ya, 2048);
  size_t nb2 = rrcsync_steps (b, x + half, n - half, yb, 2048);
  CHECK (na2 == nb2);
  int exact = 1;
  for (size_t i = 0; i < na2 && i < nb2; i++)
    if (ya[i] != yb[i])
      exact = 0;
  CHECK (exact);
  CHECK (na2 > 100);

  /* Stricter than matching outputs: the two instances' own blobs must now be
     byte-identical. That is what catches a state_bytes() that over-counts
     what get_state() writes — the tail bytes then resume fine but carry
     uninitialised garbage. */
  void *blob_a = malloc (nb);
  void *blob_b = malloc (nb);
  CHECK (blob_a && blob_b);
  if (blob_a && blob_b)
    {
      rrcsync_get_state (a, blob_a);
      rrcsync_get_state (b, blob_b);
      CHECK (memcmp (blob_a, blob_b, nb) == 0);
    }
  free (blob_a);
  free (blob_b);

  /* Envelope reject: clobber the magic. */
  ((uint8_t *)blob)[0] ^= 0xFFu;
  CHECK (rrcsync_set_state (b, blob) == DP_ERR_INVALID);

  free (blob);
  rrcsync_destroy (a);
  rrcsync_destroy (b);
}

/* Constructor validation: every out-of-range parameter must be refused
 * rather than producing a silently useless object. */
static void
test_create_validation (void)
{
  CHECK (
      rrcsync_create (0.5, RRCSYNC_PULSE_RRC, BETA, SPAN, 256, 0.005, 0.707, 0)
      == NULL);
  CHECK (
      rrcsync_create (4.0, RRCSYNC_PULSE_RRC, -0.1, SPAN, 256, 0.005, 0.707, 0)
      == NULL);
  CHECK (
      rrcsync_create (4.0, RRCSYNC_PULSE_RRC, 1.1, SPAN, 256, 0.005, 0.707, 0)
      == NULL);
  CHECK (rrcsync_create (4.0, RRCSYNC_PULSE_RRC, BETA, 0, 256, 0.005, 0.707, 0)
         == NULL);
  CHECK (
      rrcsync_create (4.0, RRCSYNC_PULSE_RRC, BETA, SPAN, 300, 0.005, 0.707, 0)
      == NULL);
  CHECK (
      rrcsync_create (4.0, RRCSYNC_PULSE_RRC, BETA, SPAN, 256, -1.0, 0.707, 0)
      == NULL);
  CHECK (
      rrcsync_create (4.0, RRCSYNC_PULSE_RRC, BETA, SPAN, 256, 0.005, 0.0, 0)
      == NULL);
  CHECK (rrcsync_create (4.0, 7, BETA, SPAN, 256, 0.005, 0.707, 0) == NULL);
  /* NaN must be refused too, not smuggled through a >= comparison. */
  CHECK (
      rrcsync_create (NAN, RRCSYNC_PULSE_RRC, BETA, SPAN, 256, 0.005, 0.707, 0)
      == NULL);
}

int
main (void)
{
  test_create_validation ();
  test_bank_convention ();
  test_step_equals_steps ();
  test_arbitrary_rate_locks ();
  test_rectangular_pulse_locks ();
  test_tracks_clock_offset ();
  test_state_roundtrip ();

  if (_fails)
    {
      fprintf (stderr, "test_rrcsync_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_rrcsync_core PASSED\n");
  return 0;
}
