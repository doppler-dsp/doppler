#include "detector2d/detector2d_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
      _fails++;                                                                \
    }                                                                          \
  } while (0)

#define NY 8
#define NX 8
#define N (NY * NX) /* 64 */

int
main (void)
{
  int _fails = 0;

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    CHECK (det != NULL);
    CHECK (det->ny == NY);
    CHECK (det->nx == NX);
    CHECK (det->n == N);
    CHECK (det->ring_cap >= N);
    CHECK (det->ring != NULL);
    CHECK (det->corr != NULL);
    CHECK (det->_last_corr_valid == 0);

    detector2d_destroy (det);
    detector2d_destroy (NULL);
  }

  /* ── impulse ref: peak at (row=0, col=0) ────────────────────────── *
   * noise_lo=0 includes the peak so noise_est = 1/N > 0 and            *
   * test_stat = N >> 1.                                                 */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];
    size_t ndet = detector2d_push (det, ref, N, results, 16);

    CHECK (ndet == 1);
    CHECK (results[0].row == 0);
    CHECK (results[0].col == 0);
    CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    CHECK (results[0].test_stat > 1.0f);
    CHECK (det->_last_corr_valid == 1);

    detector2d_destroy (det);
  }

  /* ── sub-frame push ──────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    size_t n1 = detector2d_push (det, ref, N / 2, results, 16);
    CHECK (n1 == 0);

    size_t n2 = detector2d_push (det, ref + N / 2, N / 2, results, 16);
    CHECK (n2 == 1);
    CHECK (results[0].row == 0 && results[0].col == 0);

    detector2d_destroy (det);
  }

  /* ── dwell=2 ─────────────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 2, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    size_t n1 = detector2d_push (det, ref, N, results, 16);
    CHECK (n1 == 0);

    size_t n2 = detector2d_push (det, ref, N, results, 16);
    CHECK (n2 == 1);
    CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    detector2d_destroy (det);
  }

  /* ── 2-D shift: ref=δ[0,0] in=δ[1,0] → peak at (row=1, col=0) ──── */
  {
    float complex ref[N] = { 0 };
    float complex in[N] = { 0 };
    ref[0] = 1.0f;
    in[NX] = 1.0f; /* row 1, col 0 */

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];
    size_t ndet = detector2d_push (det, in, N, results, 16);

    CHECK (ndet == 1);
    CHECK (results[0].row == 1);
    CHECK (results[0].col == 0);

    detector2d_destroy (det);
  }

  /* ── threshold gate ──────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                 DET_NOISE_MEAN, 1000.0f, 1);
    det_result2d_t results[16];
    size_t ndet = detector2d_push (det, ref, N, results, 16);
    CHECK (ndet == 0);

    detector2d_set_threshold (det, 0.0f);
    ndet = detector2d_push (det, ref, N, results, 16);
    CHECK (ndet == 1);

    detector2d_destroy (det);
  }

  /* ── reset clears state ──────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 2, 1, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    detector2d_push (det, ref, N, results, 16);
    CHECK (det->corr->count == 1);

    detector2d_reset (det);
    CHECK (det->corr->count == 0);
    CHECK (det->_last_corr_valid == 0);

    detector2d_destroy (det);
  }

  if (_fails)
    {
      fprintf (stderr, "test_detector2d_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_detector2d_core PASSED\n");
  return 0;
}
