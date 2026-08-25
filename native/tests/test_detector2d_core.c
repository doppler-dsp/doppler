#include "detector2d/detector2d_core.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NY 8
#define NX 8
#define N (NY * NX) /* 64 */

/* An independent aggregate over the noise window, computed from the
 * correlation surface the detector reports rather than from the detector's
 * own scratch. `det_noise_estimate` is the thing under test, so the
 * comparison must not route through it. */
static float
_agg (const float *mag, size_t lo, size_t hi, det_noise_mode_t mode)
{
  size_t cnt = hi - lo + 1;
  if (mode == DET_NOISE_MEAN)
    {
      double acc = 0.0;
      for (size_t k = lo; k <= hi; k++)
        acc += mag[k];
      return (float)(acc / (double)cnt);
    }
  if (mode == DET_NOISE_MIN)
    {
      float m = mag[lo];
      for (size_t k = lo; k <= hi; k++)
        if (mag[k] < m)
          m = mag[k];
      return m;
    }
  if (mode == DET_NOISE_MAX)
    {
      float m = mag[lo];
      for (size_t k = lo; k <= hi; k++)
        if (mag[k] > m)
          m = mag[k];
      return m;
    }
  /* median: insertion sort of a copy, the definition rather than the
     implementation's selection algorithm. */
  float tmp[N];
  for (size_t k = 0; k < cnt; k++)
    tmp[k] = mag[lo + k];
  for (size_t i = 1; i < cnt; i++)
    {
      float  v = tmp[i];
      size_t j = i;
      while (j > 0 && tmp[j - 1] > v)
        {
          tmp[j] = tmp[j - 1];
          j--;
        }
      tmp[j] = v;
    }
  return tmp[cnt / 2];
}

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

  /* ── dwell = 0 is refused, inherited from corr2d ──────────────────────
   *
   * detector2d's own header says dwell "must be >= 1" and it validates
   * nothing itself -- it forwards straight to corr2d_create, which is
   * where the rule now lives (see test_corr2d_core.c). Pinned here too
   * because this is the object a caller constructs, and because it is the
   * assertion that would notice if the forwarding were ever replaced by a
   * local copy that forgot the rule. */
  {
    float complex ref[N] = { 0 };
    ref[0]               = 1.0f;
    DP_CHECK (
        detector2d_create (ref, NY, NX, 0, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1)
        == NULL);
    /* Not vacuous: the neighbouring value builds. */
    detector2d_state_t *ok = detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (ok != NULL);
    detector2d_destroy (ok);
  }

  /* ── all FOUR noise modes, not just the mean ──────────────────────────
   *
   * noise_est is the DENOMINATOR of every detection decision this object
   * makes, and the mode is a documented 4-way enum. Three of the four --
   * MEDIAN, MIN and MAX -- had zero mentions in this file and zero in
   * test_detector2d.py, so a broken selection would have moved every
   * test_stat the library computes with nothing to notice.
   *
   * Measured against `_agg` above, which re-computes the aggregate from
   * the same surface by definition (a full sort for the median, not a
   * selection algorithm), so the two paths share no code. */
  {
    float complex ref[N] = { 0 }, in[N];
    ref[0]               = 1.0f;
    /* A surface with a clear peak and a spread of noise values, so the
       four modes give four DIFFERENT answers -- a flat window would let
       any of them pass as any other. */
    /* dp_xs32, not a hand-rolled LCG: one RNG per repo, so a test's noise
       is reproducible against the same helper every other test uses
       (make tests-ssot). */
    uint32_t st = 4242u;
    for (size_t k = 0; k < N; k++)
      {
        float mag = 0.1f + (float)((dp_xs32 (&st) >> 16) & 0xFFu) / 255.0f;
        in[k]     = mag;
      }
    in[0] = 8.0f; /* the peak */

    const size_t lo = 1, hi = N - 1;
    float        got[4];
    for (int m = 0; m < 4; m++)
      {
        detector2d_state_t *det = detector2d_create (
            ref, NY, NX, 1, lo, hi, (det_noise_mode_t)m, 0.0f, 1);
        DP_CHECK (det != NULL);
        det_result2d_t res[4];
        size_t         nd = detector2d_push (det, in, N, res, 4);
        DP_CHECK (nd == 1);
        if (nd == 1)
          {
            /* Rebuild the surface magnitudes the detector saw. With an
               impulse reference the correlation IS the input, so the
               magnitudes are known without re-running the correlator. */
            float mag[N];
            for (size_t k = 0; k < N; k++)
              mag[k] = cabsf (in[k]);
            float want = _agg (mag, lo, hi, (det_noise_mode_t)m);
            DP_CHECK (fabsf (res[0].noise_est - want)
                      < 1e-4f * (want > 1.0f ? want : 1.0f));
            got[m] = res[0].noise_est;
            /* test_stat is peak/noise, so it moves with the mode. */
            DP_CHECK (
                fabsf (res[0].test_stat - res[0].peak_mag / res[0].noise_est)
                < 1e-4f);
          }
        detector2d_destroy (det);
      }
    /* Not vacuous: the four modes genuinely disagree on this window, so
       each assertion above is discriminating rather than four spellings
       of the same number. MIN < MEDIAN < MEAN is not guaranteed for an
       arbitrary distribution, but MIN < MAX and MIN <= MEDIAN <= MAX are. */
    DP_CHECK (got[DET_NOISE_MIN] < got[DET_NOISE_MAX]);
    DP_CHECK (got[DET_NOISE_MIN] <= got[DET_NOISE_MEDIAN]);
    DP_CHECK (got[DET_NOISE_MEDIAN] <= got[DET_NOISE_MAX]);
    DP_CHECK (got[DET_NOISE_MIN] < got[DET_NOISE_MEAN]);
  }

  /* ── set_ref: both branches of its documented contract ────────────────
   *
   * Zero mentions in either test file. The header promises three things:
   * the new reference takes effect, the object ALWAYS resets even when the
   * reference is accepted, and it returns -1 when corr2d_set_ref refuses.
   *
   * That third branch is not hypothetical, and writing this test found it
   * the hard way: an impulse reference is single-row, so the object is on
   * corr2d's fast path, and the fast path can only accept another
   * single-row reference. Swapping in a multi-row one is REFUSED -- which
   * is the contract working, not a defect, and is now asserted rather than
   * tripped over. */
  {
    float complex ref_a[N] = { 0 }, ref_b[N] = { 0 }, ref_2d[N] = { 0 };
    float complex in[N] = { 0 };
    ref_a[0]            = 1.0f; /* single-row: impulse at (0,0)  */
    ref_b[2]            = 1.0f; /* single-row: impulse at (0,2)  */
    for (size_t k = 0; k < N; k++)
      ref_2d[k] = (float)(k % 5) + 1.0f; /* genuinely multi-row */
    in[0] = 1.0f;

    detector2d_state_t *det = detector2d_create (ref_a, NY, NX, 2, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    det_result2d_t res[4];

    /* Dirty EVERY piece of state the reset is supposed to clear, or the
       assertions below pass on things that were already clean. Measured
       while writing this: asserting only `corr->count == 0` is satisfied
       by corr2d_set_ref's own reset, so it tests corr2d rather than this
       function -- deleting detector2d_reset from set_ref left it green.
       The ring residue and the last-dump flag are what set_ref adds. */
    DP_CHECK (detector2d_push (det, in, N, res, 4) == 0); /* dwell 1 of 2 */
    DP_CHECK (detector2d_push (det, in, N, res, 4) == 1); /* dumps       */
    DP_CHECK (det->_last_corr_valid == 1);
    DP_CHECK (detector2d_push (det, in, N, res, 4) == 0); /* accumulator */
    DP_CHECK (det->corr->count == 1);
    DP_CHECK (detector2d_push (det, in, N / 2, res, 4) == 0); /* ring     */
    size_t resid = (size_t)(DP_LOAD_ACQ (&det->ring->head)
                            - DP_LOAD_RLX (&det->ring->tail));
    DP_CHECK (resid > 0);

    /* Accepted, and it reset all three even though the reference was fine. */
    DP_CHECK (detector2d_set_ref (det, ref_b) == 0);
    DP_CHECK (det->corr->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);
    DP_CHECK ((size_t)(DP_LOAD_ACQ (&det->ring->head)
                       - DP_LOAD_RLX (&det->ring->tail))
              == 0);

    /* The new reference is genuinely in force: the impulse moved two
       columns, so the correlation peak moves with it. */
    DP_CHECK (detector2d_push (det, in, N, res, 4) == 0);
    size_t nd = detector2d_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2);

    /* A multi-row reference is REFUSED on a fast-path object, and the
       refusal is not destructive -- the object still works afterwards. */
    DP_CHECK (detector2d_set_ref (det, ref_2d) != 0);
    DP_CHECK (detector2d_push (det, in, N, res, 4) == 0);
    nd = detector2d_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2); /* still ref_b */

    detector2d_destroy (det);
  }

  /* ── set_threshold gates without rebuilding, and the last-dump values
   *    update REGARDLESS of the gate ──────────────────────────────────────
   *
   * The struct comment says the last-dump fields are "updated on every dump
   * regardless of threshold", which is what lets a caller raise the gate and
   * still read what the surface did. Nothing asserted it.
   *
   * Assert the VALUES move, not the validity flag. Checking
   * `_last_corr_valid == 1` after a gated push passes on state left over
   * from the previous ungated one -- measured: gating the flag's assignment
   * leaves such a test green. So the second push carries a peak at a
   * DIFFERENT position, and the last-dump position must follow it even
   * though nothing was emitted. */
  {
    float complex ref[N] = { 0 }, in_a[N] = { 0 }, in_b[N] = { 0 };
    ref[0]           = 1.0f;
    in_a[0]          = 1.0f; /* peak at (0,0) */
    in_b[2 * NX + 5] = 1.0f; /* peak at (2,5) */

    detector2d_state_t *det = detector2d_create (ref, NY, NX, 1, 0, N - 1,
                                                 DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    det_result2d_t res[4];

    DP_CHECK (detector2d_push (det, in_a, N, res, 4) == 1);
    float stat_open = det->test_stat;
    DP_CHECK (stat_open > 1.0f);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    /* Raise the gate above what the surface produces: nothing emitted ... */
    detector2d_set_threshold (det, stat_open * 10.0f);
    DP_CHECK (det->threshold == stat_open * 10.0f);
    DP_CHECK (detector2d_push (det, in_b, N, res, 4) == 0);

    /* ... and yet the last-dump POSITION followed the new input, which is
       the claim. A stale copy would still read (0,0) from in_a. */
    DP_CHECK (det->peak_row == 2 && det->peak_col == 5);
    DP_CHECK (det->_last_corr_valid == 1);

    /* Dropping the gate re-opens it with no rebuild. */
    detector2d_set_threshold (det, 0.0f);
    DP_CHECK (detector2d_push (det, in_a, N, res, 4) == 1);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    detector2d_destroy (det);
  }

  DP_TEST_END ("test_detector2d_core");
}
