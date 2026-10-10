#include "doppler/detector/detector_core.h"
#include "dp_chunk_inv.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define N 64

/* ── chunk invariance (dp_chunk_inv.h) ──────────────────────────────────── */

/** One detector configuration, handed to the harness as its `arg`. */
typedef struct
{
  const float _Complex *ref;
  size_t                dwell, noise_lo, noise_hi;
  det_noise_mode_t      noise_mode;
  float                 threshold;
} ci_det_cfg_t;

static void *
ci_det_create (void *arg)
{
  const ci_det_cfg_t *c = (const ci_det_cfg_t *)arg;
  return dp_detector_create (c->ref, N, c->dwell, c->noise_lo, c->noise_hi,
                             c->noise_mode, c->threshold, 1);
}

static void
ci_det_destroy (void *obj)
{
  dp_detector_destroy ((dp_detector_state_t *)obj);
}

/* out_cap is the result cap: sized so no call can fill it (one result per
   frame at most), so every frame of every partition is drained. */
static size_t
ci_det_push (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  return dp_detector_push ((dp_detector_state_t *)obj,
                           (const float _Complex *)in, n, (det_result_t *)out,
                           out_cap);
}

/* Exact, field by field: det_result_t has padding after its size_t and
   three floats, and the harness's default memcmp would compare that too.
   Each float is compared by its bytes, so -0.0 and +0.0 still differ. */
static int
ci_det_equal (const void *a, const void *b, size_t n)
{
  const det_result_t *x = (const det_result_t *)a;
  const det_result_t *y = (const det_result_t *)b;
  for (size_t i = 0; i < n; i++)
    if (x[i].lag != y[i].lag
        || memcmp (&x[i].peak_mag, &y[i].peak_mag, sizeof (float)) != 0
        || memcmp (&x[i].noise_est, &y[i].noise_est, sizeof (float)) != 0
        || memcmp (&x[i].test_stat, &y[i].test_stat, sizeof (float)) != 0)
      return 0;
  return 1;
}

int
main (void)
{

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->n == N);
    DP_CHECK (det->ring_cap >= N);
    DP_CHECK (det->ring != NULL);
    DP_CHECK (det->corr != NULL);
    DP_CHECK (det->_last_corr_valid == 0);

    dp_detector_destroy (det);
    dp_detector_destroy (NULL); /* must not crash */
  }

  /* ── noise_hi sentinel clamp ────────────────────────────────────── *
   * The binding passes (size_t)-1 for the documented "n-1" default; it *
   * must clamp to N-1, not overflow scratch sizing / read OOB.         */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det = dp_detector_create (ref, N, 1, 0, (size_t)-1,
                                                   DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->noise_lo == 0);
    DP_CHECK (det->noise_hi == N - 1);

    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].lag == 0);
    DP_CHECK (isfinite (results[0].noise_est) && results[0].noise_est > 0.0f);
    DP_CHECK (isfinite (results[0].test_stat) && results[0].test_stat > 1.0f);

    dp_detector_destroy (det);
  }

  /* ── impulse ref: push one full frame, threshold=0 always fires ───── *
   * corr(δ,δ)[τ] = δ[τ] → peak at lag 0, value 1.                      *
   * noise_lo=0 includes the peak in the noise estimate so that           *
   * noise_est = 1/N > 0 and test_stat = N (well above 1).               */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, ref, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].lag == 0);
    DP_CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    DP_CHECK (results[0].test_stat > 1.0f);
    DP_CHECK (det->_last_corr_valid == 1);

    dp_detector_destroy (det);
  }

  /* ── sub-frame push: two halves should produce one detection ─────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    size_t n1 = dp_detector_push (det, ref, N / 2, results, 16);
    DP_CHECK (n1 == 0); /* only half a frame — no dump */

    size_t n2 = dp_detector_push (det, ref + N / 2, N / 2, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].lag == 0);

    dp_detector_destroy (det);
  }

  /* ── threshold gate: test_stat must exceed threshold ─────────────── *
   * Push δ vs δ → stat >> 1.  With threshold=1000 nothing fires.       */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 1000.0f, 1);
    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 0);

    /* Lower threshold — now fires. */
    dp_detector_set_threshold (det, 0.0f);
    ndet = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);

    dp_detector_destroy (det);
  }

  /* ── dwell=2: needs two frames before a detection ────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    /* Push 1 frame: corr accumulates, no dump. */
    size_t n1 = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (n1 == 0);

    /* Push 2nd frame: dumps, test_stat = 2.0/noise (two δ summed). */
    size_t n2 = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    dp_detector_destroy (det);
  }

  /* ── shifted input: peak should move to lag 1 ───────────────────── *
   * ref = δ[0], in = δ[1].  corr(δ[n-1], δ[n])[τ] → peak at τ=1.    */
  {
    float _Complex ref[N] = { 0 };
    float _Complex in[N]  = { 0 };
    ref[0]                = 1.0f;
    in[1]                 = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, in, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].lag == 1);

    dp_detector_destroy (det);
  }

  /* ── dp_detector_reset clears state ────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];

    /* Push 1 frame (partial dwell). */
    dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (det->corr->count == 1);

    /* Reset clears the dwell counter and ring. */
    dp_detector_reset (det);
    DP_CHECK (det->corr->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);

    dp_detector_destroy (det);
  }

  /* ── noise_mode = MEDIAN ─────────────────────────────────────────── *
   * noise_lo=0 so that median over [0..N-1] includes the impulse peak   *
   * and is nonzero (= 0 for N-1 bins, peak for 1 bin → median = 0 for  *
   * N>2; use MIN so any nonzero entry makes noise_est > 0).             */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    /* DET_NOISE_MIN: noise_est = min(mag) over full spectrum.
     * For impulse self-corr, mag = [1, 0, 0, ...], min = 0.
     * Use noise_lo=0, noise_hi=0 (only the peak bin) so noise_est=1. */
    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 0, 0, DET_NOISE_MEDIAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].test_stat > 0.0f);

    dp_detector_destroy (det);
  }

  /* ── multi-frame push: push 3 frames at once ─────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;
    float _Complex big[3 * N];
    for (size_t i = 0; i < 3 * N; i++)
      big[i] = ref[i % N];

    dp_detector_state_t *det
        = dp_detector_create (ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result_t results[16];
    size_t       ndet = dp_detector_push (det, big, 3 * N, results, 16);
    DP_CHECK (ndet == 3);

    dp_detector_destroy (det);
  }

  /* serializable state — corr child + ring residual + result fields. */
  {
    float _Complex ref[16], in[24];
    det_result_t res[16];
    for (int i = 0; i < 16; i++)
      ref[i] = (float)(i % 4) + 0.5f * I;
    for (int i = 0; i < 24; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    dp_detector_state_t *a
        = dp_detector_create (ref, 16, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    dp_detector_state_t *b
        = dp_detector_create (ref, 16, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_detector_push (a, in, 24, res, 16);
    DP_STATE_ROUNDTRIP_TEST (dp_detector, a, b);
    DP_CHECK (b->corr->count == a->corr->count); /* corr child resumed */
    DP_CHECK ((DP_LOAD_ACQ (&b->ring->head) - DP_LOAD_RLX (&b->ring->tail))
              == (DP_LOAD_ACQ (&a->ring->head)
                  - DP_LOAD_RLX (&a->ring->tail))); /* ring residual */
    DP_CHECK (b->_last_corr_valid == a->_last_corr_valid);
    dp_detector_destroy (a);
    dp_detector_destroy (b);
  }

  /* chunk invariance: the detections are a function of the input stream,
   * not of how push() calls cut it. A partial frame is carried between
   * calls, and so is a dwell's coherent sum, so both are exercised: every
   * frame firing (dwell 1, threshold 0), and a gated dwell of 4 under the
   * median estimator. The reference sits at irregular offsets in noise, so
   * the lags and statistics differ from frame to frame. */
  {
    enum
    {
      LEN = 37 * N + 21
    };
    uint32_t st = 0x1895u;
    float _Complex ref[N];
    static float _Complex x[LEN];
    for (size_t i = 0; i < N; i++)
      {
        const float re = (float)dp_bit (&st);
        const float im = (float)dp_bit (&st);
        ref[i]         = re + I * im;
      }
    for (size_t i = 0; i < LEN; i++)
      x[i] = 0.4f * dp_cgauss (&st);
    for (size_t at = 17; at + N <= LEN; at += 3 * N + 29)
      for (size_t i = 0; i < N; i++)
        x[at + i] += ref[i];

    const ci_det_cfg_t cfgs[] = {
      { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f },
      { ref, 4, 2, N - 4, DET_NOISE_MEDIAN, 3.0f },
    };
    for (size_t c = 0; c < sizeof cfgs / sizeof *cfgs; c++)
      {
        dp_ci_spec_t spec = {
          .name     = c == 0 ? "detector push, dwell 1"
                             : "detector push, dwell 4, median",
          .create   = ci_det_create,
          .destroy  = ci_det_destroy,
          .process  = ci_det_push,
          .arg      = (void *)&cfgs[c],
          .in_size  = sizeof (float _Complex),
          .out_size = sizeof (det_result_t),
          .out_cap  = LEN / N + 1,
          .equal    = ci_det_equal,
          .frame_n  = N,
        };
        DP_CHECK (dp_chunk_invariance (&spec, x, LEN) == 0);
      }
  }

  DP_TEST_END ("test_detector_core");
}
