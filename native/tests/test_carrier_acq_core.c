#include "carrier_acq/carrier_acq_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                          \
  do                                                                         \
    {                                                                       \
      if (!(cond))                                                          \
        {                                                                   \
          fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                         \
        }                                                                    \
    }                                                                        \
  while (0)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int _fails = 0;

static uint32_t _rng_state;
static uint32_t
_xorshift32(void)
{
  uint32_t x = _rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return _rng_state = x;
}

/* BPSK NRZ data (+-1, held sps samples/symbol) * a complex tone at
 * tone_hz, sampled at sample_rate_hz -- deterministic given seed. */
static float complex *
_make_signal(size_t n_symbols, size_t sps, double sample_rate_hz,
             double tone_hz, uint32_t seed, size_t *out_len)
{
  size_t n = n_symbols * sps;
  float complex *x = malloc(n * sizeof(float complex));
  _rng_state = seed ? seed : 1u;
  for (size_t sym = 0; sym < n_symbols; sym++)
    {
      float bit = (_xorshift32() & 1u) ? 1.0f : -1.0f;
      for (size_t j = 0; j < sps; j++)
        {
          size_t i = sym * sps + j;
          double phase = 2.0 * M_PI * tone_hz * (double)i / sample_rate_hz;
          x[i] = bit * (cosf((float)phase) + I * sinf((float)phase));
        }
    }
  *out_len = n;
  return x;
}

static float complex *
_make_noise(size_t n, uint32_t seed)
{
  float complex *noise = malloc(n * sizeof(float complex));
  _rng_state = seed ? seed : 1u;
  for (size_t i = 0; i < n; i++)
    {
      float a = ((float)(_xorshift32() % 2001u) - 1000.0f) / 100000.0f;
      float b = ((float)(_xorshift32() % 2001u) - 1000.0f) / 100000.0f;
      noise[i] = a + I * b;
    }
  return noise;
}

static const double SAMPLE_RATE_HZ = 8000.0;
static const double SYMBOL_RATE_HZ = 1000.0;
static const double TONE_HZ = 123.0;
#define SPS 8
#define N_SYMBOLS 4000
#define MAX_N_BLOCKS 100000

int
main(void)
{
  /* ── sequential=true: fires early, accurate residual_hz ── */
  {
    size_t n;
    float complex *x
        = _make_signal(N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    carrier_acq_state_t *ca = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, /*sequential=*/true, MAX_N_BLOCKS);
    CHECK(ca != NULL);
    if (ca)
      {
        carrier_acq_steps(ca, x, n);
        CHECK(ca->ready);
        if (ca->ready)
          CHECK(fabs(ca->residual_hz - TONE_HZ) < 5.0);
        CHECK(ca->n_blocks <= ca->max_n_blocks);
        carrier_acq_destroy(ca);
      }
    free(x);
  }

  /* ── sequential=false: waits the full fixed dwell, same accuracy ── */
  {
    size_t n;
    float complex *x
        = _make_signal(N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    carrier_acq_state_t *ca = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, /*sequential=*/false, MAX_N_BLOCKS);
    CHECK(ca != NULL);
    if (ca)
      {
        carrier_acq_steps(ca, x, n);
        CHECK(ca->ready);
        if (ca->ready)
          {
            CHECK(fabs(ca->residual_hz - TONE_HZ) < 5.0);
            CHECK(ca->n_blocks == ca->dwell_target);
          }
        carrier_acq_destroy(ca);
      }
    free(x);
  }

  /* ── carry-buffer split-call: tiny, non-block-aligned chunks give the
     same result as one big call ── */
  {
    size_t n;
    float complex *x
        = _make_signal(N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ, 12345u, &n);
    carrier_acq_state_t *ca = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, true, MAX_N_BLOCKS);
    CHECK(ca != NULL);
    if (ca)
      {
        const size_t chunk = 3; /* not a multiple of n_fft or sps */
        for (size_t off = 0; off < n && !ca->ready; off += chunk)
          {
            size_t take = (n - off < chunk) ? n - off : chunk;
            carrier_acq_steps(ca, x + off, take);
          }
        CHECK(ca->ready);
        if (ca->ready)
          CHECK(fabs(ca->residual_hz - TONE_HZ) < 5.0);
        carrier_acq_destroy(ca);
      }
    free(x);
  }

  /* ── pure noise, non-sequential mode: never confidently detects, gives
     up at dwell_target (the fixed, design_snr-driven wait point) ── */
  {
    carrier_acq_state_t *ca = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, /*sequential=*/false, MAX_N_BLOCKS);
    CHECK(ca != NULL);
    if (ca)
      {
        size_t nfft_frame
            = (size_t)llround(SAMPLE_RATE_HZ / (SYMBOL_RATE_HZ / 10.0));
        size_t n = (ca->dwell_target + 2) * nfft_frame;
        float complex *noise = _make_noise(n, 999u);
        carrier_acq_steps(ca, noise, n);
        CHECK(!ca->ready);
        CHECK(ca->n_blocks == ca->dwell_target);
        free(noise);
        carrier_acq_destroy(ca);
      }
  }

  /* ── pure noise, sequential mode: tests EVERY block (not bounded by
     dwell_target, an optimistic design_snr point estimate that can be
     as small as 1) -- gives up only at its OWN, separate max_n_blocks
     cap. A small explicit max_n_blocks keeps this fast. ── */
  {
    const size_t small_cap = 5;
    carrier_acq_state_t *ca = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, /*sequential=*/true, small_cap);
    CHECK(ca != NULL);
    if (ca)
      {
        CHECK(ca->max_n_blocks == small_cap);
        size_t nfft_frame
            = (size_t)llround(SAMPLE_RATE_HZ / (SYMBOL_RATE_HZ / 10.0));
        size_t n = (small_cap + 2) * nfft_frame;
        float complex *noise = _make_noise(n, 999u);
        carrier_acq_steps(ca, noise, n);
        CHECK(!ca->ready);
        CHECK(ca->n_blocks == small_cap);
        free(noise);
        carrier_acq_destroy(ca);
      }
  }

  /* ── template override: a caller-supplied shape is used verbatim,
     runs to completion without crashing ── */
  {
    carrier_acq_state_t *probe = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, true, MAX_N_BLOCKS);
    CHECK(probe != NULL);
    if (probe)
      {
        size_t nfft = probe->nfft;
        carrier_acq_destroy(probe);

        float *tmpl = malloc(nfft * sizeof(float));
        /* A single sharp spike at DC (bin nfft/2, DC-centred) instead of
           the default sinc^2 -- a deliberately DIFFERENT known shape. */
        for (size_t k = 0; k < nfft; k++)
          tmpl[k] = (k == nfft / 2) ? 1.0f : 0.0f;

        size_t n;
        float complex *x = _make_signal(N_SYMBOLS, SPS, SAMPLE_RATE_HZ,
                                         TONE_HZ, 12345u, &n);
        carrier_acq_state_t *ca = carrier_acq_create(
            SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, tmpl, nfft, 1e-3,
            0.9, 2.0, true, MAX_N_BLOCKS);
        CHECK(ca != NULL);
        if (ca)
          {
            CHECK(ca->nfft == nfft);
            carrier_acq_steps(ca, x, n);
            CHECK(ca->n_blocks > 0);
            carrier_acq_destroy(ca);
          }
        free(x);
        free(tmpl);
      }
  }

  /* ── invalid args rejected ── */
  CHECK(carrier_acq_create(0.0, 0.0, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9, 2.0,
                            true, MAX_N_BLOCKS)
        == NULL);

  /* ── state roundtrip + envelope reject; resumed instance continues
     bit-for-bit identically to the un-interrupted one ── */
  {
    size_t n;
    float complex *x = _make_signal(N_SYMBOLS, SPS, SAMPLE_RATE_HZ, TONE_HZ,
                                     777u, &n);
    carrier_acq_state_t *a = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, true, MAX_N_BLOCKS);
    carrier_acq_state_t *b = carrier_acq_create(
        SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, 4, 0, 0.0f, NULL, 0, 1e-3, 0.9,
        2.0, true, MAX_N_BLOCKS);
    CHECK(a != NULL && b != NULL);
    if (a && b)
      {
        size_t half = n / 2 + 3; /* mid-stream, not block-aligned */
        if (half > n)
          half = n;
        carrier_acq_steps(a, x, half);
        DP_STATE_ROUNDTRIP_TEST(carrier_acq, a, b);
        CHECK(b->n_blocks == a->n_blocks);
        CHECK(b->ready == a->ready);

        carrier_acq_steps(a, x + half, n - half);
        carrier_acq_steps(b, x + half, n - half);
        CHECK(a->ready == b->ready);
        CHECK(a->n_blocks == b->n_blocks);
        if (a->ready && b->ready)
          CHECK(fabs(a->residual_hz - b->residual_hz) < 1e-9);
      }
    if (a)
      carrier_acq_destroy(a);
    if (b)
      carrier_acq_destroy(b);
    free(x);
  }

  if (_fails)
    {
      fprintf(stderr, "test_carrier_acq_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf("test_carrier_acq_core PASSED\n");
  return 0;
}
