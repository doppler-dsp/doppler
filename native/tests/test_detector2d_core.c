#include "detector2d/detector2d_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NY 8
#define NX 8
#define N (NY * NX) /* 64 */

int
main (void)
{

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->ny == NY);
    DP_CHECK (det->nx == NX);
    DP_CHECK (det->n == N);
    DP_CHECK (det->ring_cap >= N);
    DP_CHECK (det->ring != NULL);
    DP_CHECK (det->corr != NULL);
    DP_CHECK (det->_last_corr_valid == 0);

    detector2d_destroy (det);
    detector2d_destroy (NULL);
  }

  /* ── noise_hi sentinel clamp ────────────────────────────────────── *
   * The binding passes (size_t)-1 for the documented "ny*nx-1" default. *
   * It must clamp to N-1, not overflow the scratch sizing / OOB-read.   */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, (size_t)-1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->noise_lo == 0);
    DP_CHECK (det->noise_hi == N - 1);

    det_result2d_t results[16];
    size_t         ndet = detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);
    DP_CHECK (isfinite (results[0].noise_est) && results[0].noise_est > 0.0f);
    DP_CHECK (isfinite (results[0].test_stat) && results[0].test_stat > 1.0f);

    detector2d_destroy (det);
  }

  /* ── impulse ref: peak at (row=0, col=0) ────────────────────────── *
   * noise_lo=0 includes the peak so noise_est = 1/N > 0 and            *
   * test_stat = N >> 1.                                                 */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t      results[16];
    size_t              ndet = detector2d_push (det, ref, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0);
    DP_CHECK (results[0].col == 0);
    DP_CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    DP_CHECK (results[0].test_stat > 1.0f);
    DP_CHECK (det->_last_corr_valid == 1);

    detector2d_destroy (det);
  }

  /* ── sub-frame push ──────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t      results[16];

    size_t n1 = detector2d_push (det, ref, N / 2, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = detector2d_push (det, ref + N / 2, N / 2, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);

    detector2d_destroy (det);
  }

  /* ── dwell=2 ─────────────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 2, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t      results[16];

    size_t n1 = detector2d_push (det, ref, N, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = detector2d_push (det, ref, N, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    detector2d_destroy (det);
  }

  /* ── 2-D shift: ref=δ[0,0] in=δ[1,0] → peak at (row=1, col=0) ──── */
  {
    float complex ref[N] = { 0 };
    float complex in[N]  = { 0 };
    ref[0]               = 1.0f;
    in[NX]               = 1.0f; /* row 1, col 0 */

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t      results[16];
    size_t              ndet = detector2d_push (det, in, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 1);
    DP_CHECK (results[0].col == 0);

    detector2d_destroy (det);
  }

  /* ── threshold gate ──────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 1000.0f, 1);
    det_result2d_t      results[16];
    size_t              ndet = detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 0);

    detector2d_set_threshold (det, 0.0f);
    ndet = detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);

    detector2d_destroy (det);
  }

  /* ── reset clears state ──────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 2, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t      results[16];

    detector2d_push (det, ref, N, results, 16);
    DP_CHECK (det->corr->count == 1);

    detector2d_reset (det);
    DP_CHECK (det->corr->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);

    detector2d_destroy (det);
  }

  /* serializable state — corr2d child + ring residual + result fields. */
  {
    float complex  ref[16], in[24];
    det_result2d_t res[16];
    for (int i = 0; i < 16; i++)
      ref[i] = (float)(i % 4) + 0.5f * I;
    for (int i = 0; i < 24; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    detector2d_state_t *a
        = detector2d_create (ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    detector2d_state_t *b
        = detector2d_create (ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (a != NULL && b != NULL);
    (void)detector2d_push (a, in, 24, res, 16);
    DP_STATE_ROUNDTRIP_TEST (detector2d, a, b);
    DP_CHECK (b->corr->count == a->corr->count); /* corr2d child resumed */
    DP_CHECK ((DP_LOAD_ACQ (&b->ring->head) - DP_LOAD_RLX (&b->ring->tail))
              == (DP_LOAD_ACQ (&a->ring->head)
                  - DP_LOAD_RLX (&a->ring->tail))); /* ring residual */
    DP_CHECK (b->_last_corr_valid == a->_last_corr_valid);
    detector2d_destroy (a);
    detector2d_destroy (b);
  }

  DP_TEST_END ("test_detector2d_core");
}
