/* test_cic_core.c — CIC decimation filter unit tests.
 *
 * CIC is fixed at N=4 stages, M=1, power-of-two R.
 *
 * Covers:
 *   - Invalid constructor arguments → NULL
 *   - Output sample count: n_in/R for any block size multiple of R
 *   - DC response: settled output = 1.0 for ±1.0 DC input, real and complex
 *   - Zero input: output is exactly 0+0j throughout
 *   - Reset: second run with same input produces byte-identical output
 *   - Reconfigure: output count and DC response correct after R change
 *   - cic_destroy(NULL): no crash
 *   - Alias rejection: stopband tone ≥ 20 dB below passband reference
 */
#include "cic/cic_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

static inline int
_feq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_ceq (float complex a, float complex b, float tol)
{
  return _feq (crealf (a), crealf (b), tol)
         && _feq (cimagf (a), cimagf (b), tol);
}
#define FEQC(a, b, tol) _ceq ((float complex) (a), (float complex) (b), (tol))

/*
 * Feed n_in copies of `sample` through obj and return the last output.
 * `out` must hold at least ceil(n_in / R) elements.
 * Caller guarantees n_in is large enough that the filter has settled.
 */
static float complex
dc_last (cic_state_t *obj, float complex sample, float complex *out,
         size_t n_in)
{
  float complex *in = (float complex *)malloc (n_in * sizeof (float complex));
  for (size_t i = 0; i < n_in; i++)
    in[i] = sample;
  size_t n = cic_decimate (obj, in, n_in, out);
  free (in);
  return out[n - 1];
}

int
main (void)
{
  int _fails = 0;

  /* ── Invalid constructor args → NULL ─────────────────────────────────── */
  CHECK (cic_create (0) == NULL);    /* R = 0 */
  CHECK (cic_create (1) == NULL);    /* R = 1: < 2 */
  CHECK (cic_create (3) == NULL);    /* non-power-of-two */
  CHECK (cic_create (8192) == NULL); /* R > 4096 */

  /* NULL destroy is a documented no-op */
  cic_destroy (NULL);

  /* ── shift field: CIC_N * log2(R) ────────────────────────────────────── */
  {
    cic_state_t *obj = cic_create (16);
    CHECK (obj != NULL);
    CHECK (obj->R == 16);
    CHECK (obj->shift == 16); /* CIC_N=4, log2(16)=4 → 4*4=16 */
    cic_destroy (obj);
  }
  {
    cic_state_t *obj = cic_create (8);
    CHECK (obj != NULL);
    CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 → 4*3=12 */
    cic_destroy (obj);
  }

  /* ── Output sample count ─────────────────────────────────────────────── */
  /* For a fresh filter, n_in = k*R must produce exactly k outputs. */
  {
    uint32_t     R   = 8;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    float complex in[256] = { 0 }, out[256];
    for (int k = 1; k <= 4; k++)
      {
        cic_reset (obj);
        size_t n = cic_decimate (obj, in, (size_t)k * R, out);
        CHECK (n == (size_t)k);
      }
    /* Partial block: R-1 inputs → 0 outputs */
    cic_reset (obj);
    CHECK (cic_decimate (obj, in, R - 1, out) == 0);
    /* Then 1 more input completes the first decimation cycle */
    CHECK (cic_decimate (obj, in, 1, out) == 1);
    cic_destroy (obj);
  }

  /* ── Zero input → zero settled output ───────────────────────────────── */
  /* With offset-binary encoding, zero input maps to u=32768 (not 0), so
   * the integrators ramp during the first CIC_N output periods before the
   * comb delay chain fills.  From output index CIC_N onward the output is
   * exactly 0+0j. */
  {
    cic_state_t *obj = cic_create (4);
    CHECK (obj != NULL);
    float complex in[64] = { 0 }, out[64];
    size_t        n      = cic_decimate (obj, in, 64, out);
    CHECK (n == 16);
    for (size_t i = CIC_N; i < n; i++)
      CHECK (FEQC (out[i], 0.0f, 0.0f));
    cic_destroy (obj);
  }

  /* ── DC response: +1.0 real ──────────────────────────────────────────── */
  /* Transient ≈ CIC_N*(R-1) input samples; 8*R*CIC_N ensures full settling. */
  {
    uint32_t     R   = 4;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    size_t         n_in = 8 * R * CIC_N;
    float complex *out  = malloc ((n_in / R + 1) * sizeof (float complex));
    float complex  last = dc_last (obj, 1.0f + 0.0f * I, out, n_in);
    CHECK (FEQC (last, 1.0f + 0.0f * I, 4e-5f));
    free (out);
    cic_destroy (obj);
  }

  /* ── DC response: −1.0 real (tests signed two's-complement path) ─────── */
  {
    uint32_t     R   = 4;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    size_t         n_in = 8 * R * CIC_N;
    float complex *out  = malloc ((n_in / R + 1) * sizeof (float complex));
    float complex  last = dc_last (obj, -1.0f + 0.0f * I, out, n_in);
    CHECK (FEQC (last, -1.0f + 0.0f * I, 4e-5f));
    free (out);
    cic_destroy (obj);
  }

  /* ── DC response: +j (imaginary path independent of real) ────────────── */
  {
    uint32_t     R   = 4;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    size_t         n_in = 8 * R * CIC_N;
    float complex *out  = malloc ((n_in / R + 1) * sizeof (float complex));
    float complex  last = dc_last (obj, 0.0f + 1.0f * I, out, n_in);
    CHECK (FEQC (last, 0.0f + 1.0f * I, 4e-5f));
    free (out);
    cic_destroy (obj);
  }

  /* ── DC response: (0.5 + 0.5j) — typical SDR config R=32 ────────────── */
  /* Transient ≈ CIC_N*(32-1) = 124 inputs; 12*R outputs ensures settling. */
  {
    uint32_t     R   = 32;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    size_t         n_in = 12 * R * CIC_N;
    float complex *out  = malloc ((n_in / R + 1) * sizeof (float complex));
    float complex  last = dc_last (obj, 0.5f + 0.5f * I, out, n_in);
    CHECK (FEQC (last, 0.5f + 0.5f * I, 4e-5f));
    free (out);
    cic_destroy (obj);
  }

  /* ── Reset: second run produces byte-identical output ────────────────── */
  {
    uint32_t     R   = 4;
    cic_state_t *obj = cic_create (R);
    CHECK (obj != NULL);
    size_t         n_in = 64;
    float complex *in   = malloc (n_in * sizeof (float complex));
    float complex *out1 = malloc (n_in * sizeof (float complex));
    float complex *out2 = malloc (n_in * sizeof (float complex));
    /* non-trivial input: ramp on real, constant on imag */
    for (size_t i = 0; i < n_in; i++)
      in[i] = (float)i * 0.01f + 0.5f * I;

    size_t n1 = cic_decimate (obj, in, n_in, out1);
    cic_reset (obj);
    size_t n2 = cic_decimate (obj, in, n_in, out2);
    CHECK (n1 == n2);
    CHECK (memcmp (out1, out2, n1 * sizeof (float complex)) == 0);

    free (in);
    free (out1);
    free (out2);
    cic_destroy (obj);
  }

  /* ── Reconfigure: output count and DC response update correctly ──────── */
  {
    cic_state_t *obj = cic_create (4);
    CHECK (obj != NULL);
    float complex in[256], out[256];
    for (int i = 0; i < 256; i++)
      in[i] = 1.0f;

    /* warm up with R=4 */
    cic_decimate (obj, in, 32, out);

    /* reconfigure to R=8 */
    cic_reconfigure (obj, 8);
    CHECK (obj->R == 8);
    CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 */

    /* output count must reflect new R */
    size_t n = cic_decimate (obj, in, 8 * 8 * CIC_N, out);
    CHECK (n == (size_t)(8 * CIC_N));

    /* settled output must be 1.0 */
    CHECK (FEQC (out[n - 1], 1.0f + 0.0f * I, 4e-5f));
    cic_destroy (obj);
  }

  /* ── Reconfigure: invalid args are silently ignored ─────────────────── */
  {
    cic_state_t *obj = cic_create (8);
    CHECK (obj != NULL);
    cic_reconfigure (obj, 0); /* R=0: invalid, ignored */
    CHECK (obj->R == 8);
    cic_reconfigure (obj, 3); /* non-power-of-two: invalid, ignored */
    CHECK (obj->R == 8);
    cic_reconfigure (obj, 8192); /* R > 4096: invalid, ignored */
    CHECK (obj->R == 8);
    cic_destroy (obj);
  }

  /* ── Streaming: split block across two calls ─────────────────────────── */
  /* Two calls of R samples must give same result as one call of 2R samples */
  {
    uint32_t      R = 16;
    float complex in[64], out_split[4], out_whole[4];
    for (int i = 0; i < 64; i++)
      in[i] = 0.7f - 0.3f * I;

    cic_state_t *a = cic_create (R);
    cic_state_t *b = cic_create (R);
    CHECK (a && b);

    /* whole: 2R in one call */
    cic_decimate (b, in, 2 * R, out_whole);

    /* split: R then R */
    cic_decimate (a, in, R, out_split);
    cic_decimate (a, in + R, R, out_split + 1);

    CHECK (FEQC (out_split[0], out_whole[0], 0.0f));
    CHECK (FEQC (out_split[1], out_whole[1], 0.0f));

    cic_destroy (a);
    cic_destroy (b);
  }

  /* ── Alias rejection: stopband tone must be heavily attenuated ───────── */
  /* Feed a tone at 0.95*fs/R (near first CIC null) and compare output
   * power to a DC (passband) reference.  Requires ≥ 20 dB rejection. */
  {
    uint32_t R       = 8;
    double   f_alias = 0.95 / R; /* just inside first null at fs/R */
    size_t   n_in    = 32 * R * CIC_N;
    size_t   n_out   = n_in / R;
    size_t   n_drop  = CIC_N * (R - 1) / R + 2; /* skip transient */
    size_t   n_meas  = n_out - n_drop;

    float complex *in  = malloc (n_in * sizeof (float complex));
    float complex *out = malloc (n_out * sizeof (float complex));

    /* passband reference: DC input → should be ~1.0 at output */
    for (size_t i = 0; i < n_in; i++)
      in[i] = 1.0f + 0.0f * I;
    cic_state_t *obj = cic_create (R);
    cic_decimate (obj, in, n_in, out);
    double pwr_pass = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_pass += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_pass /= (double)n_meas;

    /* alias-zone tone */
    cic_reset (obj);
    for (size_t i = 0; i < n_in; i++)
      in[i] = CMPLXF ((float)cos (2 * M_PI * f_alias * i),
                      (float)sin (2 * M_PI * f_alias * i));
    cic_decimate (obj, in, n_in, out);
    double pwr_alias = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_alias += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_alias /= (double)n_meas;

    double rejection_db = 10.0 * log10 (pwr_pass / (pwr_alias + 1e-300));
    CHECK (rejection_db >= 20.0);

    cic_destroy (obj);
    free (in);
    free (out);
  }

  /* ── Serializable state round-trip — the elastic-resume guarantee ─────────
   * Split a stream at a mid-decimation-cycle cut, hand the state to a fresh
   * CIC, and continue: output equals an uninterrupted run byte-for-byte. */
  {
    const uint32_t R  = 16;
    const size_t   L  = 320;
    float complex *in = malloc (L * sizeof (float complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)(i % 7) - 3.0f + I * ((float)(i % 5) - 2.0f);
    float complex outA[64], outB[64];

    cic_state_t *ra = cic_create (R);
    size_t       nA = cic_decimate (ra, in, L, outA);
    cic_destroy (ra);

    const size_t cut = 173; /* not a multiple of R → mid-cycle phase */
    cic_state_t *r1  = cic_create (R);
    size_t       nB  = cic_decimate (r1, in, cut, outB);
    size_t       sb  = cic_state_bytes (r1);
    CHECK (sb == 4 * CIC_N * sizeof (uint64_t) + sizeof (uint32_t));
    void *blob = malloc (sb);
    cic_get_state (r1, blob);
    cic_destroy (r1);

    cic_state_t *r2 = cic_create (R);
    CHECK (cic_set_state (r2, blob) == 0);
    nB += cic_decimate (r2, in + cut, L - cut, outB + nB);
    cic_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    CHECK (nA > 0 && memcmp (outA, outB, nA * sizeof (float complex)) == 0);
    free (in);
  }

  if (_fails)
    {
      fprintf (stderr, "test_cic_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_cic_core PASSED\n");
  return 0;
}
