/*
 * test_dsss_receiver_core.c — composed DsssReceiver C-level tests.
 *
 * Covers: argument validation, a real Acquisition -> Dll -> RateConverter ->
 * MpskReceiver run over a synthetic continuous DSSS-BPSK signal (searching
 * -> tracking transition, correct decode via a lag search over known data --
 * the physics of *why* this composition works is already covered by
 * Acquisition/Dll/MpskReceiver's own dedicated tests and this repo's
 * async-DSSS-receiver gallery story; this test is about the NEW object
 * wiring those four correctly, not re-proving the underlying DSP), the
 * state-serialization round trip in both the searching and tracking phases,
 * and an envelope-reject check.
 */
#include "dsss_receiver/dsss_receiver_core.h"
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

/* A length-7 maximal-length sequence (one period) -- same fixture
 * test_acq_core.c uses for its own fast, real (not mocked) unit tests. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* Unit-variance complex Gaussian (Box-Muller from xorshift) -- same
 * generator as test_acq_core.c/test_dll_core.c/test_symsync_core.c (no
 * shared test-utils header exists for it yet). */
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
  double   mag = sqrt (-log (u1));
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

static int
_test_arg_validation (void)
{
  int _fails = 0;
  CHECK (dsss_receiver_create (NULL, 0, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0,
                               16, 8, 0.0 /* doppler_resolution */, 4, 8, 0)
         == NULL);
  CHECK (dsss_receiver_create (CODE7, 7, 0.0, 1e3, 2, 2, 55.0, 1e-3, 0.9,
                               100.0, 16, 8, 0.0 /* doppler_resolution */, 4,
                               8, 0)
         == NULL); /* chip_rate <= 0 */
  CHECK (dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 3, 55.0, 1e-3, 0.9,
                               100.0, 16, 8, 0.0 /* doppler_resolution */, 4,
                               8, 0)
         == NULL); /* m not in {2,4,8} */
  CHECK (dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9,
                               100.0, 16, 8, 0.0 /* doppler_resolution */, 0,
                               8, 0)
         == NULL); /* segments < 1 */

  dsss_receiver_state_t *rx = dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 45.0, 1e-2, 0.9, 500.0, 8, 4,
      0.0 /* doppler_resolution */, 4, 8, 0);
  CHECK (rx != NULL);
  if (rx)
    {
      CHECK (dsss_receiver_get_tracking (rx) == 0);
      CHECK (dsss_receiver_get_segments (rx) == 4);
      CHECK (dsss_receiver_get_sps (rx) == 8);
      CHECK (dsss_receiver_get_n (rx) == 4); /* sps=8 -> largest divisor 4 */
      CHECK (dsss_receiver_get_chip_phase (rx) == 0.0);
      CHECK (dsss_receiver_get_code_rate (rx) == 1.0);
      dsss_receiver_destroy (rx);
    }
  return _fails;
}

/* Build a continuous, code-spread BPSK capture: silence, then
 * data[si] * code[cph] * exp(j*2*pi*doppler_hz/fs*idx) + AWGN. Mirrors this
 * repo's own Python story's make_signal(), simplified (no asynchronous
 * symbol/code-epoch clock stress -- that physics is already validated at
 * the Acquisition/Dll level; this test is about the composed object's
 * wiring) and sized for a fast C test. */
static void
_make_signal (const uint8_t *code, size_t sf, size_t spc, double fs,
              double tsym, double doppler_hz, double cn0_dbhz, size_t n_sym,
              size_t pre_silence, uint32_t seed, float complex **x_out,
              size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed ? seed : 1;
  for (size_t i = 0; i < n_sym + 4; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      data[i] = (st & 1u) ? 1.0 : -1.0;
    }

  size_t         n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t         tot = pre_silence + n;
  float complex *x   = calloc (tot, sizeof *x);

  double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  double sigma   = 1.0 / amp_snr;
  for (size_t i = 0; i < tot; i++)
    x[i] = (float complex) (sigma / sqrt (2.0)) * cgauss (&st);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t cph = (idx / spc) % sf;
      double ph = 2.0 * 3.14159265358979323846 * doppler_hz / fs * (double)idx;
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/* Stream `x` through `rx` in fixed-size chunks, collecting every emitted
 * symbol; return the symbol count and fill `*syms_out` (caller frees). */
static size_t
_stream (dsss_receiver_state_t *rx, const float complex *x, size_t n,
         size_t chunk, float complex **syms_out)
{
  float complex *syms   = malloc (n * sizeof *syms); /* generous upper bound */
  size_t         n_syms = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : (n - pos);
      n_syms += dsss_receiver_steps (rx, x + pos, take, syms + n_syms,
                                     n - n_syms);
    }
  *syms_out = syms;
  return n_syms;
}

/* Best-lag BER over the back half of the recovered symbols against the
 * known data (absorbing the composed chain's settling transient and
 * pipeline delay -- the existence of a clean near-zero-error lag is the
 * proof, the same pattern this repo's gallery examples use in Python). */
static double
_best_ber (const float complex *syms, size_t n_syms, const double *data,
           size_t n_sym)
{
  if (n_syms < 20)
    return 1.0;
  size_t lo = n_syms / 2, hi = n_syms;
  double best = 1.0;
  for (int lag = -20; lag <= 20; lag++)
    {
      size_t errs = 0, cnt = 0;
      for (size_t i = lo; i < hi; i++)
        {
          long di = (long)i + lag;
          if (di < 0 || (size_t)di >= n_sym)
            continue;
          double bit = crealf (syms[i]) > 0 ? 1.0 : -1.0;
          if (bit != data[(size_t)di])
            errs++;
          cnt++;
        }
      if (cnt < (hi - lo) / 2)
        continue; /* not enough overlap at this lag to be meaningful */
      double ber = (double)errs / (double)cnt;
      if (ber < best)
        best = ber;
      if (1.0 - ber < best) /* phase/sign ambiguity: inverted bits */
        best = 1.0 - ber;
    }
  return best;
}

static int
_test_acquire_and_decode (void)
{
  int _fails = 0;

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 90.0;

  float complex *x;
  size_t         n;
  double        *data;
  _make_signal (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7, &x,
                &n, &data);

  dsss_receiver_state_t *rx = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2, 0.9, 500.0, 8, 4,
      0.0 /* doppler_resolution */, 4, 8, 0);
  CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return _fails + 1;
    }

  float complex *syms;
  size_t         n_syms = _stream (rx, x, n, te, &syms);

  CHECK (dsss_receiver_get_tracking (rx) == 1);
  CHECK (n_syms > 20);
  CHECK (dsss_receiver_get_cn0_dbhz_est (rx) > 0.0);

  double ber = _best_ber (syms, n_syms, data, n_sym + 4);
  CHECK (ber < 0.05);

  /* ── state-serialization round trip, while tracking ─────────────────── */
  size_t cb   = dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  dsss_receiver_get_state (rx, blob);

  dsss_receiver_state_t *rx2 = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2, 0.9, 500.0, 8, 4,
      0.0 /* doppler_resolution */, 4, 8, 0);
  CHECK (rx2 != NULL);
  if (rx2)
    {
      CHECK (dsss_receiver_set_state (rx2, blob) == DP_OK);
      CHECK (dsss_receiver_get_tracking (rx2) == 1);
      CHECK (fabs (dsss_receiver_get_chip_phase (rx2)
                   - dsss_receiver_get_chip_phase (rx))
             < 1e-9);

      /* a corrupted envelope must be rejected, not reinterpreted. */
      ((char *)blob)[0] ^= (char)0xFF;
      CHECK (dsss_receiver_set_state (rx2, blob) == DP_ERR_INVALID);
      dsss_receiver_destroy (rx2);
    }
  free (blob);

  /* ── state-serialization round trip, while searching ─────────────────── */
  dsss_receiver_state_t *rx3 = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2, 0.9, 500.0, 8, 4,
      0.0 /* doppler_resolution */, 4, 8, 0);
  CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      dsss_receiver_get_state (rx3, blob3);

      dsss_receiver_state_t *rx4 = dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2, 0.9, 500.0, 8, 4,
          0.0 /* doppler_resolution */, 4, 8, 0);
      CHECK (rx4 != NULL);
      if (rx4)
        {
          CHECK (dsss_receiver_set_state (rx4, blob3) == DP_OK);
          CHECK (dsss_receiver_get_tracking (rx4) == 0);
          dsss_receiver_destroy (rx4);
        }
      free (blob3);
      dsss_receiver_destroy (rx3);
    }

  /* ── reset() returns to searching ─────────────────────────────────────── */
  dsss_receiver_reset (rx);
  CHECK (dsss_receiver_get_tracking (rx) == 0);
  CHECK (dsss_receiver_get_chip_phase (rx) == 0.0);

  free (syms);
  free (x);
  free (data);
  dsss_receiver_destroy (rx);
  return _fails;
}

int
main (void)
{
  int _fails = 0;
  _fails += _test_arg_validation ();
  _fails += _test_acquire_and_decode ();
  if (_fails)
    {
      fprintf (stderr, "test_dsss_receiver_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_dsss_receiver_core PASSED\n");
  return 0;
}
