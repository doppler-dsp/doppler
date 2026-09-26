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
 *   - dp_cic_destroy(NULL): no crash
 *   - Alias rejection: stopband tone ≥ 20 dB below passband reference
 */
#include "doppler/cic/cic_core.h"
#include "doppler/dp_complex.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Feed n_in copies of `sample` through obj and return the last output.
 * `out` must hold at least ceil(n_in / R) elements.
 * Caller guarantees n_in is large enough that the filter has settled.
 */
static float _Complex dc_last (dp_cic_state_t *obj, float _Complex sample,
                               float _Complex *out, size_t n_in, size_t cap)
{
  float _Complex *in
      = (float _Complex *)malloc (n_in * sizeof (float _Complex));
  for (size_t i = 0; i < n_in; i++)
    in[i] = sample;
  size_t n = dp_cic_decimate (obj, in, n_in, out, cap);
  free (in);
  return out[n - 1];
}

int
main (void)
{

  /* ── Invalid constructor args → NULL ─────────────────────────────────── */
  DP_CHECK (dp_cic_create (0) == NULL);    /* R = 0 */
  DP_CHECK (dp_cic_create (1) == NULL);    /* R = 1: < 2 */
  DP_CHECK (dp_cic_create (3) == NULL);    /* non-power-of-two */
  DP_CHECK (dp_cic_create (8192) == NULL); /* R > 4096 */

  /* NULL destroy is a documented no-op */
  dp_cic_destroy (NULL);

  /* ── shift field: CIC_N * log2(R) ────────────────────────────────────── */
  {
    dp_cic_state_t *obj = dp_cic_create (16);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->R == 16);
    DP_CHECK (obj->shift == 16); /* CIC_N=4, log2(16)=4 → 4*4=16 */
    dp_cic_destroy (obj);
  }
  {
    dp_cic_state_t *obj = dp_cic_create (8);
    DP_CHECK (obj != NULL);
    DP_CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 → 4*3=12 */
    dp_cic_destroy (obj);
  }

  /* ── Output sample count ─────────────────────────────────────────────── */
  /* For a fresh filter, n_in = k*R must produce exactly k outputs. */
  {
    uint32_t        R   = 8;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    float _Complex in[256] = { 0 }, out[256];
    for (int k = 1; k <= 4; k++)
      {
        dp_cic_reset (obj);
        size_t n = dp_cic_decimate (obj, in, (size_t)k * R, out, 256);
        DP_CHECK (n == (size_t)k);
      }
    /* Partial block: R-1 inputs → 0 outputs */
    dp_cic_reset (obj);
    DP_CHECK (dp_cic_decimate (obj, in, R - 1, out, 256) == 0);
    /* Then 1 more input completes the first decimation cycle */
    DP_CHECK (dp_cic_decimate (obj, in, 1, out, 256) == 1);
    dp_cic_destroy (obj);
  }

  /* ── Zero input → zero settled output ───────────────────────────────── */
  /* With offset-binary encoding, zero input maps to u=32768 (not 0), so
   * the integrators ramp during the first CIC_N output periods before the
   * comb delay chain fills.  From output index CIC_N onward the output is
   * exactly 0+0j. */
  {
    dp_cic_state_t *obj = dp_cic_create (4);
    DP_CHECK (obj != NULL);
    float _Complex in[64] = { 0 }, out[64];
    size_t n              = dp_cic_decimate (obj, in, 64, out, 64);
    DP_CHECK (n == 16);
    for (size_t i = CIC_N; i < n; i++)
      DP_CHECK (dp_cnearf (out[i], 0.0f, 0.0f));
    dp_cic_destroy (obj);
  }

  /* ── DC response: +1.0 real ──────────────────────────────────────────── */
  /* Transient ≈ CIC_N*(R-1) input samples; 8*R*CIC_N ensures full settling. */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 1.0f + 0.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 1.0f + 0.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: −1.0 real (tests signed two's-complement path) ─────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, -1.0f + 0.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, -1.0f + 0.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: +j (imaginary path independent of real) ────────────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 8 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 0.0f + 1.0f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 0.0f + 1.0f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── DC response: (0.5 + 0.5j) — typical SDR config R=32 ────────────── */
  /* Transient ≈ CIC_N*(32-1) = 124 inputs; 12*R outputs ensures settling. */
  {
    uint32_t        R   = 32;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 12 * R * CIC_N;
    float _Complex *out  = malloc ((n_in / R + 1) * sizeof (float _Complex));
    float _Complex last
        = dc_last (obj, 0.5f + 0.5f * I, out, n_in, n_in / R + 1);
    DP_CHECK (dp_cnearf (last, 0.5f + 0.5f * I, 4e-5f));
    free (out);
    dp_cic_destroy (obj);
  }

  /* ── Reset: second run produces byte-identical output ────────────────── */
  {
    uint32_t        R   = 4;
    dp_cic_state_t *obj = dp_cic_create (R);
    DP_CHECK (obj != NULL);
    size_t          n_in = 64;
    float _Complex *in   = malloc (n_in * sizeof (float _Complex));
    float _Complex *out1 = malloc (n_in * sizeof (float _Complex));
    float _Complex *out2 = malloc (n_in * sizeof (float _Complex));
    /* non-trivial input: ramp on real, constant on imag */
    for (size_t i = 0; i < n_in; i++)
      in[i] = (float)i * 0.01f + 0.5f * I;

    size_t n1 = dp_cic_decimate (obj, in, n_in, out1, n_in);
    dp_cic_reset (obj);
    size_t n2 = dp_cic_decimate (obj, in, n_in, out2, n_in);
    DP_CHECK (n1 == n2);
    DP_CHECK (memcmp (out1, out2, n1 * sizeof (float _Complex)) == 0);

    free (in);
    free (out1);
    free (out2);
    dp_cic_destroy (obj);
  }

  /* ── Reconfigure: output count and DC response update correctly ──────── */
  {
    dp_cic_state_t *obj = dp_cic_create (4);
    DP_CHECK (obj != NULL);
    float _Complex in[256], out[256];
    for (int i = 0; i < 256; i++)
      in[i] = 1.0f;

    /* warm up with R=4 */
    dp_cic_decimate (obj, in, 32, out, 256);

    /* reconfigure to R=8 */
    dp_cic_reconfigure (obj, 8);
    DP_CHECK (obj->R == 8);
    DP_CHECK (obj->shift == 12); /* CIC_N=4, log2(8)=3 */

    /* output count must reflect new R */
    size_t n = dp_cic_decimate (obj, in, 8 * 8 * CIC_N, out, 256);
    DP_CHECK (n == (size_t)(8 * CIC_N));

    /* settled output must be 1.0 */
    DP_CHECK (dp_cnearf (out[n - 1], 1.0f + 0.0f * I, 4e-5f));
    dp_cic_destroy (obj);
  }

  /* ── Reconfigure: invalid args are silently ignored ─────────────────── */
  {
    dp_cic_state_t *obj = dp_cic_create (8);
    DP_CHECK (obj != NULL);
    dp_cic_reconfigure (obj, 0); /* R=0: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_reconfigure (obj, 3); /* non-power-of-two: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_reconfigure (obj, 8192); /* R > 4096: invalid, ignored */
    DP_CHECK (obj->R == 8);
    dp_cic_destroy (obj);
  }

  /* ── Streaming: split block across two calls ─────────────────────────── */
  /* Two calls of R samples must give same result as one call of 2R samples */
  {
    uint32_t R = 16;
    float _Complex in[64], out_split[4], out_whole[4];
    for (int i = 0; i < 64; i++)
      in[i] = 0.7f - 0.3f * I;

    dp_cic_state_t *a = dp_cic_create (R);
    dp_cic_state_t *b = dp_cic_create (R);
    DP_CHECK (a && b);

    /* whole: 2R in one call */
    dp_cic_decimate (b, in, 2 * R, out_whole, 4);

    /* split: R then R */
    dp_cic_decimate (a, in, R, out_split, 4);
    dp_cic_decimate (a, in + R, R, out_split + 1, 3);

    DP_CHECK (dp_cnearf (out_split[0], out_whole[0], 0.0f));
    DP_CHECK (dp_cnearf (out_split[1], out_whole[1], 0.0f));

    dp_cic_destroy (a);
    dp_cic_destroy (b);
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

    float _Complex *in  = malloc (n_in * sizeof (float _Complex));
    float _Complex *out = malloc (n_out * sizeof (float _Complex));

    /* passband reference: DC input → should be ~1.0 at output */
    for (size_t i = 0; i < n_in; i++)
      in[i] = 1.0f + 0.0f * I;
    dp_cic_state_t *obj = dp_cic_create (R);
    dp_cic_decimate (obj, in, n_in, out, n_out);
    double pwr_pass = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_pass += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_pass /= (double)n_meas;

    /* alias-zone tone */
    dp_cic_reset (obj);
    for (size_t i = 0; i < n_in; i++)
      in[i] = CMPLXF ((float)cos (2 * M_PI * f_alias * i),
                      (float)sin (2 * M_PI * f_alias * i));
    dp_cic_decimate (obj, in, n_in, out, n_out);
    double pwr_alias = 0.0;
    for (size_t i = n_drop; i < n_out; i++)
      pwr_alias += (double)cabsf (out[i]) * cabsf (out[i]);
    pwr_alias /= (double)n_meas;

    double rejection_db = 10.0 * log10 (pwr_pass / (pwr_alias + 1e-300));
    DP_CHECK (rejection_db >= 20.0);

    dp_cic_destroy (obj);
    free (in);
    free (out);
  }

  /* ── Serializable state round-trip — the elastic-resume guarantee ─────────
   * Split a stream at a mid-decimation-cycle cut, hand the state to a fresh
   * CIC, and continue: output equals an uninterrupted run byte-for-byte. */
  {
    const uint32_t  R  = 16;
    const size_t    L  = 320;
    float _Complex *in = malloc (L * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)(i % 7) - 3.0f + I * ((float)(i % 5) - 2.0f);
    float _Complex outA[64], outB[64];

    dp_cic_state_t *ra = dp_cic_create (R);
    size_t          nA = dp_cic_decimate (ra, in, L, outA, 64);
    dp_cic_destroy (ra);

    const size_t    cut = 173; /* not a multiple of R → mid-cycle phase */
    dp_cic_state_t *r1  = dp_cic_create (R);
    size_t          nB  = dp_cic_decimate (r1, in, cut, outB, 64);
    size_t          sb  = dp_cic_state_bytes (r1);
    DP_CHECK (sb
              == sizeof (dp_state_hdr_t) + 4 * CIC_N * sizeof (uint64_t)
                     + sizeof (uint32_t) + sizeof (uint8_t));
    /* This block's own input runs to +-3, i.e. well past the +-1.0 bound —
       so the sticky flag must be up, and must survive the round trip.  A
       resumed stream that forgot it had clipped would answer wrongly. */
    DP_CHECK (r1->clipped == 1);
    void *blob = malloc (sb);
    dp_cic_get_state (r1, blob);
    dp_cic_destroy (r1);

    dp_cic_state_t *r2 = dp_cic_create (R);
    DP_CHECK (r2->clipped == 0);
    DP_CHECK (dp_cic_set_state (r2, blob) == DP_OK);
    DP_CHECK (r2->clipped == 1);
    /* standard envelope: a magic-clobbered blob is rejected, r2 untouched */
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (dp_cic_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_cic_decimate (r2, in + cut, L - cut, outB + nB, 64 - nB);
    dp_cic_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    DP_CHECK (nA > 0
              && memcmp (outA, outB, nA * sizeof (float _Complex)) == 0);
    free (in);
  }

  /* ── sticky clip flag: the only signal that the +-1.0 input bound was
   *    exceeded, since the sample stream stays finite and plausible ────── */
  {
    dp_cic_state_t *obj = dp_cic_create (16);
    float _Complex in[64];
    DP_CHECK (obj->clipped == 0);
    /* The bound is CIC_PAPR_HEADROOM, not 1.0: the encoder reserves that
       headroom precisely so a unit-amplitude signal's peaks fit (an RRC
       stream peaks at 1.582x its symbol amplitude). 1.5 is now IN range —
       it used to clip, and a caller who backed off to avoid that was giving
       away 4 dB nothing restored. */
    for (size_t i = 0; i < 64; i++)
      in[i] = 0.9f + 0.9f * I;
    float _Complex out[8];
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 0); /* in range — no false positive */

    for (size_t i = 0; i < 64; i++)
      in[i] = 1.5f + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 0); /* inside the PAPR headroom */

    for (size_t i = 0; i < 64; i++)
      in[i] = 1.05f * CIC_PAPR_HEADROOM + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 1);

    for (size_t i = 0; i < 64; i++)
      in[i] = 0.1f + 0.0f * I;
    dp_cic_decimate (obj, in, 64, out, 8);
    DP_CHECK (obj->clipped == 1); /* sticky across later in-range blocks */

    dp_cic_reset (obj);
    DP_CHECK (obj->clipped == 0); /* cleared only by reset() */

    /* every component and sign is caught */
    const float B               = 1.05f * CIC_PAPR_HEADROOM;
    const float _Complex bad[4] = { B, -B, B * I, -B * I };
    for (size_t k = 0; k < 4; k++)
      {
        dp_cic_reset (obj);
        for (size_t i = 0; i < 64; i++)
          in[i] = bad[k];
        dp_cic_decimate (obj, in, 64, out, 8);
        DP_CHECK (obj->clipped == 1);
      }
    dp_cic_destroy (obj);
  }

  /* ── short out: the filter still runs, only emission truncates ──────
   * The integrators and combs must advance over every input sample even
   * when there is no room to write the result -- otherwise the pipeline
   * desynchronises from the stream.  Feed the same input twice, once with
   * room and once without, and check the STATE lands in the same place. */
  {
    const uint32_t R    = 4;
    const size_t   n_in = 64, n_full = n_in / R; /* 16 outputs */
    float _Complex in[64];
    for (size_t i = 0; i < n_in; i++)
      in[i] = CMPLXF ((float)cos (2 * M_PI * 0.05 * (double)i) * 0.5f,
                      (float)sin (2 * M_PI * 0.05 * (double)i) * 0.5f);

    float _Complex full[16], part[16];
    const float _Complex CANARY = -1234.0f - 567.0f * I;
    const size_t K              = 5;

    dp_cic_state_t *a = dp_cic_create (R);
    DP_CHECK (dp_cic_decimate (a, in, n_in, full, n_full) == n_full);

    dp_cic_state_t *b = dp_cic_create (R);
    for (size_t k = 0; k < n_full; k++)
      part[k] = CANARY;
    DP_CHECK (dp_cic_decimate (b, in, n_in, part, K) == K);
    /* what was written is the true prefix ... */
    for (size_t k = 0; k < K; k++)
      DP_CHECK (dp_cnearf (part[k], full[k], 0.0f));
    /* ... and nothing past the capacity was touched. */
    for (size_t k = K; k < n_full; k++)
      DP_CHECK (dp_cnearf (part[k], CANARY, 0.0f));
    /* ... and both filters are in the SAME state: the next block agrees. */
    float _Complex nextA[16], nextB[16];
    size_t nA = dp_cic_decimate (a, in, n_in, nextA, n_full);
    size_t nB = dp_cic_decimate (b, in, n_in, nextB, n_full);
    DP_CHECK (nA == nB);
    for (size_t k = 0; k < nA; k++)
      DP_CHECK (dp_cnearf (nextA[k], nextB[k], 0.0f));

    dp_cic_destroy (a);
    dp_cic_destroy (b);
  }

  DP_TEST_END ("test_cic_core");
}
