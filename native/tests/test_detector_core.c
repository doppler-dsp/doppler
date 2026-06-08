#include "detector/detector_core.h"
#include <math.h>
#include <stdio.h>
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

#define N 64

int
main (void)
{
  int _fails = 0;

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    CHECK (det != NULL);
    CHECK (det->n == N);
    CHECK (det->ring_cap >= N);
    CHECK (det->ring != NULL);
    CHECK (det->corr != NULL);
    CHECK (det->_last_corr_valid == 0);

    detector_destroy (det);
    detector_destroy (NULL); /* must not crash */
  }

  /* ── impulse ref: push one full frame, threshold=0 always fires ───── *
   * corr(δ,δ)[τ] = δ[τ] → peak at lag 0, value 1.                      *
   * noise_lo=0 includes the peak in the noise estimate so that           *
   * noise_est = 1/N > 0 and test_stat = N (well above 1).               */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = detector_push (det, ref, N, results, 16);

    CHECK (ndet == 1);
    CHECK (results[0].lag == 0);
    CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    CHECK (results[0].test_stat > 1.0f);
    CHECK (det->_last_corr_valid == 1);

    detector_destroy (det);
  }

  /* ── sub-frame push: two halves should produce one detection ─────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    size_t n1 = detector_push (det, ref, N / 2, results, 16);
    CHECK (n1 == 0); /* only half a frame — no dump */

    size_t n2 = detector_push (det, ref + N / 2, N / 2, results, 16);
    CHECK (n2 == 1);
    CHECK (results[0].lag == 0);

    detector_destroy (det);
  }

  /* ── threshold gate: test_stat must exceed threshold ─────────────── *
   * Push δ vs δ → stat >> 1.  With threshold=1000 nothing fires.       */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 1000.0f, 1);
    det_result_t results[16];
    size_t       ndet = detector_push (det, ref, N, results, 16);
    CHECK (ndet == 0);

    /* Lower threshold — now fires. */
    detector_set_threshold (det, 0.0f);
    ndet = detector_push (det, ref, N, results, 16);
    CHECK (ndet == 1);

    detector_destroy (det);
  }

  /* ── dwell=2: needs two frames before a detection ────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    /* Push 1 frame: corr accumulates, no dump. */
    size_t n1 = detector_push (det, ref, N, results, 16);
    CHECK (n1 == 0);

    /* Push 2nd frame: dumps, test_stat = 2.0/noise (two δ summed). */
    size_t n2 = detector_push (det, ref, N, results, 16);
    CHECK (n2 == 1);
    CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    detector_destroy (det);
  }

  /* ── shifted input: peak should move to lag 1 ───────────────────── *
   * ref = δ[0], in = δ[1].  corr(δ[n-1], δ[n])[τ] → peak at τ=1.    */
  {
    float complex ref[N] = { 0 };
    float complex in[N]  = { 0 };
    ref[0]               = 1.0f;
    in[1]                = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = detector_push (det, in, N, results, 16);
    CHECK (ndet == 1);
    CHECK (results[0].lag == 1);

    detector_destroy (det);
  }

  /* ── detector_reset clears state ────────────────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    detector_state_t *det
        = detector_create (ref, N, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    /* Push 1 frame (partial dwell). */
    detector_push (det, ref, N, results, 16);
    CHECK (det->corr->count == 1);

    /* Reset clears the dwell counter and ring. */
    detector_reset (det);
    CHECK (det->corr->count == 0);
    CHECK (det->_last_corr_valid == 0);

    detector_destroy (det);
  }

  /* ── noise_mode = MEDIAN ─────────────────────────────────────────── *
   * noise_lo=0 so that median over [0..N-1] includes the impulse peak   *
   * and is nonzero (= 0 for N-1 bins, peak for 1 bin → median = 0 for  *
   * N>2; use MIN so any nonzero entry makes noise_est > 0).             */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;

    /* DET_NOISE_MIN: noise_est = min(mag) over full spectrum.
     * For impulse self-corr, mag = [1, 0, 0, ...], min = 0.
     * Use noise_lo=0, noise_hi=0 (only the peak bin) so noise_est=1. */
    detector_state_t *det
        = detector_create (ref, N, 1, 0, 0, DET_NOISE_MEDIAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = detector_push (det, ref, N, results, 16);
    CHECK (ndet == 1);
    CHECK (results[0].test_stat > 0.0f);

    detector_destroy (det);
  }

  /* ── multi-frame push: push 3 frames at once ─────────────────────── */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;
    float complex big[3 * N];
    for (size_t i = 0; i < 3 * N; i++)
      big[i] = ref[i % N];

    detector_state_t *det
        = detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = detector_push (det, big, 3 * N, results, 16);
    CHECK (ndet == 3);

    detector_destroy (det);
  }

  if (_fails)
    {
      fprintf (stderr, "test_detector_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_detector_core PASSED\n");
  return 0;
}
