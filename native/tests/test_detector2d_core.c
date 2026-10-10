#include "doppler/detector2d/detector2d_core.h"

/* det_private.h needs det_noise_mode_t from the header above, and the include
   sorter would put it first; its own block keeps the order. */
#include "doppler/detector/det_private.h"

#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NY 8
#define NX 8
#define N (NY * NX) /* 64 */

/* ── chunk invariance (dp_chunk_inv.h) ──────────────────────────────────── */

/** One detector2d configuration, handed to the harness as its `arg`. */
typedef struct
{
  const float _Complex *ref;
  size_t                dwell, noise_lo, noise_hi;
  det_noise_mode_t      noise_mode;
  float                 threshold;
} ci_d2_cfg_t;

static void *
ci_d2_create (void *arg)
{
  const ci_d2_cfg_t *c = (const ci_d2_cfg_t *)arg;
  return dp_detector2d_create (c->ref, NY, NX, c->dwell, c->noise_lo,
                               c->noise_hi, c->noise_mode, c->threshold, 1);
}

static void
ci_d2_destroy (void *obj)
{
  dp_detector2d_destroy ((dp_detector2d_state_t *)obj);
}

/* out_cap is the result cap: sized so no call can fill it (one result per
   frame at most), so every frame of every partition is drained. */
static size_t
ci_d2_push (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  return dp_detector2d_push ((dp_detector2d_state_t *)obj,
                             (const float _Complex *)in, n,
                             (det_result2d_t *)out, out_cap);
}

/* Exact, field by field: det_result2d_t has padding after its two size_t
   and three floats, and the harness's default memcmp would compare that
   too. Each float is compared by its bytes, so -0.0 and +0.0 still differ. */
static int
ci_d2_equal (const void *a, const void *b, size_t n)
{
  const det_result2d_t *x = (const det_result2d_t *)a;
  const det_result2d_t *y = (const det_result2d_t *)b;
  for (size_t i = 0; i < n; i++)
    if (x[i].row != y[i].row || x[i].col != y[i].col
        || memcmp (&x[i].peak_mag, &y[i].peak_mag, sizeof (float)) != 0
        || memcmp (&x[i].noise_est, &y[i].noise_est, sizeof (float)) != 0
        || memcmp (&x[i].test_stat, &y[i].test_stat, sizeof (float)) != 0)
      return 0;
  return 1;
}

/* The stream the stream tests read: a random QPSK reference at irregular
   offsets in noise, so the peak cells and statistics differ from frame to
   frame. CI_LEN is 37 frames and a partial one. */
#define CI_LEN (37 * N + 21)
static void
ci_d2_stream (float _Complex ref[N], float _Complex x[CI_LEN])
{
  uint32_t st = 0x1895u;
  for (size_t i = 0; i < N; i++)
    {
      const float re = (float)dp_bit (&st);
      const float im = (float)dp_bit (&st);
      ref[i]         = re + I * im;
    }
  for (size_t i = 0; i < CI_LEN; i++)
    x[i] = 0.4f * dp_cgauss (&st);
  for (size_t at = 17; at + N <= CI_LEN; at += 3 * N + 29)
    for (size_t i = 0; i < N; i++)
      x[at + i] += ref[i];
}

/* Push x through d in seeded random chunks of 1..max_chunk samples with room
   for `cap` detections a call, each chunk re-offered from
   dp_detector2d_consumed() until it is used up, and return the detections
   in got[]. Counts the calls that stopped short, the stops anywhere but on a
   frame boundary (frames tile the stream from sample 0, and a full push
   takes nothing past its last frame), and any call that wrote past its room --
   literally: the slot just past the room holds a byte pattern during the
   call, and a call that changed it, or reports more than its room, counts.
   got[] has room for got_len results. */
static size_t
ci_d2_resume (dp_detector2d_state_t *d, const float _Complex *x, size_t len,
              size_t cap, size_t max_chunk, det_result2d_t *got,
              size_t got_len, size_t *stops, size_t *wrong_stop,
              size_t *overfull)
{
  size_t         n_got = 0, off = 0;
  uint32_t       r = 0x1895u;
  det_result2d_t sentinel;
  memset (&sentinel, 0xA5, sizeof sentinel);
  *stops = *wrong_stop = *overfull = 0;
  while (off < len)
    {
      size_t m = 1 + dp_xs32 (&r) % max_chunk;
      if (m > len - off)
        m = len - off;
      for (size_t end = off + m; off < end;)
        {
          const size_t offered = end - off;
          const int    guard   = n_got + cap < got_len;
          if (guard)
            got[n_got + cap] = sentinel;
          const size_t k
              = dp_detector2d_push (d, x + off, offered, got + n_got, cap);
          const size_t took = dp_detector2d_consumed (d);
          if (k > cap
              || (guard
                  && memcmp (&got[n_got + cap], &sentinel, sizeof sentinel)
                         != 0))
            (*overfull)++;
          n_got += k;
          off += took;
          if (took < offered)
            {
              (*stops)++;
              if (off % N != 0)
                (*wrong_stop)++;
            }
          if (took == 0)  /* cannot happen with room for one: stop the loop */
            return n_got; /* rather than spin, and let the caller fail     */
        }
    }
  return n_got;
}

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
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->ny == NY);
    DP_CHECK (det->nx == NX);
    DP_CHECK (det->n == N);
    DP_CHECK (det->ring_cap >= N);
    DP_CHECK (det->ring != NULL);
    DP_CHECK (det->corr != NULL);
    DP_CHECK (det->_last_corr_valid == 0);

    dp_detector2d_destroy (det);
    dp_detector2d_destroy (NULL);
  }

  /* ── noise_hi sentinel clamp ────────────────────────────────────── *
   * The binding passes (size_t)-1 for the documented "ny*nx-1" default. *
   * It must clamp to N-1, not overflow the scratch sizing / OOB-read.   */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 0, (size_t)-1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->noise_lo == 0);
    DP_CHECK (det->noise_hi == N - 1);

    det_result2d_t results[16];
    size_t         ndet = dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);
    DP_CHECK (isfinite (results[0].noise_est) && results[0].noise_est > 0.0f);
    DP_CHECK (isfinite (results[0].test_stat) && results[0].test_stat > 1.0f);

    dp_detector2d_destroy (det);
  }

  /* ── impulse ref: peak at (row=0, col=0) ────────────────────────── *
   * noise_lo=0 includes the peak so noise_est = 1/N > 0 and            *
   * test_stat = N >> 1.                                                 */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];
    size_t         ndet = dp_detector2d_push (det, ref, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0);
    DP_CHECK (results[0].col == 0);
    DP_CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    DP_CHECK (results[0].test_stat > 1.0f);
    DP_CHECK (det->_last_corr_valid == 1);

    dp_detector2d_destroy (det);
  }

  /* ── sub-frame push ──────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    size_t n1 = dp_detector2d_push (det, ref, N / 2, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = dp_detector2d_push (det, ref + N / 2, N / 2, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);

    dp_detector2d_destroy (det);
  }

  /* ── dwell=2 ─────────────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    size_t n1 = dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    dp_detector2d_destroy (det);
  }

  /* ── 2-D shift: ref=δ[0,0] in=δ[1,0] → peak at (row=1, col=0) ──── */
  {
    float _Complex ref[N] = { 0 };
    float _Complex in[N]  = { 0 };
    ref[0]                = 1.0f;
    in[NX]                = 1.0f; /* row 1, col 0 */

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];
    size_t         ndet = dp_detector2d_push (det, in, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 1);
    DP_CHECK (results[0].col == 0);

    dp_detector2d_destroy (det);
  }

  /* ── threshold gate ──────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 1000.0f, 1);
    det_result2d_t results[16];
    size_t         ndet = dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 0);

    dp_detector2d_set_threshold (det, 0.0f);
    ndet = dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);

    dp_detector2d_destroy (det);
  }

  /* ── reset clears state ──────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    det_result2d_t results[16];

    dp_detector2d_push (det, ref, N, results, 16);
    DP_CHECK (det->corr->count == 1);

    dp_detector2d_reset (det);
    DP_CHECK (det->corr->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);

    dp_detector2d_destroy (det);
  }

  /* serializable state — corr2d child + ring residual + result fields. */
  {
    float _Complex ref[16], in[72];
    det_result2d_t res[16];
    for (int i = 0; i < 16; i++)
      ref[i] = (float)(i % 4) + 0.5f * I;
    for (int i = 0; i < 72; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    dp_detector2d_state_t *a
        = dp_detector2d_create (ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    dp_detector2d_state_t *b
        = dp_detector2d_create (ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_detector2d_push (a, in, 72, res, 16);
    DP_STATE_ROUNDTRIP_TEST (dp_detector2d, a, b);
    DP_CHECK (b->corr->count == a->corr->count); /* corr2d child resumed */
    /* 72 samples at 4 x 4: four frames, so a dump at the third and
       one frame toward the next, and a carry of 8 */
    DP_CHECK (a->corr->count == 1);
    DP_CHECK (dp_f32_framer_pending (&a->framer) == 8);
    DP_CHECK (dp_f32_framer_pending (&b->framer)
              == dp_f32_framer_pending (&a->framer));
    DP_CHECK (dp_detector2d_consumed (a) == 72); /* the push took all of in */
    DP_CHECK (dp_detector2d_consumed (b) == 0);  /* set_state: no last push */
    /* the last surface is not state: a's push dumped, b has no view */
    DP_CHECK (a->_last_corr_valid == 1);
    DP_CHECK (b->_last_corr_valid == 0);
    dp_detector2d_destroy (a);
    dp_detector2d_destroy (b);
  }

  /* ── dwell = 0 is refused, inherited from corr2d ──────────────────────
   *
   * detector2d's own header says dwell "must be >= 1" and it validates
   * nothing itself -- it forwards straight to dp_corr2d_create, which is
   * where the rule now lives (see test_corr2d_core.c). Pinned here too
   * because this is the object a caller constructs, and because it is the
   * assertion that would notice if the forwarding were ever replaced by a
   * local copy that forgot the rule. */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;
    DP_CHECK (dp_detector2d_create (ref, NY, NX, 0, 1, N - 1, DET_NOISE_MEAN,
                                    0.0f, 1)
              == NULL);
    /* Not vacuous: the neighbouring value builds. */
    dp_detector2d_state_t *ok = dp_detector2d_create (ref, NY, NX, 1, 1, N - 1,
                                                      DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (ok != NULL);
    dp_detector2d_destroy (ok);
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
    float _Complex ref[N] = { 0 }, in[N];
    ref[0]                = 1.0f;
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
        dp_detector2d_state_t *det = dp_detector2d_create (
            ref, NY, NX, 1, lo, hi, (det_noise_mode_t)m, 0.0f, 1);
        DP_CHECK (det != NULL);
        det_result2d_t res[4];
        size_t         nd = dp_detector2d_push (det, in, N, res, 4);
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
        dp_detector2d_destroy (det);
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
   * reference is accepted, and it returns -1 when dp_corr2d_set_ref refuses.
   *
   * That third branch is not hypothetical, and writing this test found it
   * the hard way: an impulse reference is single-row, so the object is on
   * corr2d's fast path, and the fast path can only accept another
   * single-row reference. Swapping in a multi-row one is REFUSED -- which
   * is the contract working, not a defect, and is now asserted rather than
   * tripped over. */
  {
    float _Complex ref_a[N] = { 0 }, ref_b[N] = { 0 }, ref_2d[N] = { 0 };
    float _Complex in[N] = { 0 };
    ref_a[0]             = 1.0f; /* single-row: impulse at (0,0)  */
    ref_b[2]             = 1.0f; /* single-row: impulse at (0,2)  */
    for (size_t k = 0; k < N; k++)
      ref_2d[k] = (float)(k % 5) + 1.0f; /* genuinely multi-row */
    in[0] = 1.0f;

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref_a, NY, NX, 2, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    det_result2d_t res[4];

    /* Dirty EVERY piece of state the reset is supposed to clear, or the
       assertions below pass on things that were already clean. Measured
       while writing this: asserting only `corr->count == 0` is satisfied
       by dp_corr2d_set_ref's own reset, so it tests corr2d rather than this
       function -- deleting dp_detector2d_reset from set_ref left it green.
       The ring residue and the last-dump flag are what set_ref adds. */
    DP_CHECK (dp_detector2d_push (det, in, N, res, 4) == 0); /* dwell 1 of 2 */
    DP_CHECK (dp_detector2d_push (det, in, N, res, 4) == 1); /* dumps       */
    DP_CHECK (det->_last_corr_valid == 1);
    DP_CHECK (dp_detector2d_push (det, in, N, res, 4) == 0); /* accumulator */
    DP_CHECK (det->corr->count == 1);
    DP_CHECK (dp_detector2d_push (det, in, N / 2, res, 4) == 0); /* carry    */
    DP_CHECK (dp_f32_framer_pending (&det->framer) == N / 2);
    DP_CHECK (dp_detector2d_consumed (det) == N / 2);

    /* Accepted, and it reset all three even though the reference was fine. */
    DP_CHECK (dp_detector2d_set_ref (det, ref_b) == 0);
    DP_CHECK (det->corr->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);
    DP_CHECK (dp_f32_framer_pending (&det->framer) == 0);
    DP_CHECK (dp_detector2d_consumed (det) == 0);

    /* The new reference is genuinely in force: the impulse moved two
       columns, so the correlation peak moves with it. */
    DP_CHECK (dp_detector2d_push (det, in, N, res, 4) == 0);
    size_t nd = dp_detector2d_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2);

    /* A multi-row reference is REFUSED on a fast-path object, and the
       refusal is not destructive -- the object still works afterwards. */
    DP_CHECK (dp_detector2d_set_ref (det, ref_2d) != 0);
    DP_CHECK (dp_detector2d_push (det, in, N, res, 4) == 0);
    nd = dp_detector2d_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2); /* still ref_b */

    dp_detector2d_destroy (det);
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
    float _Complex ref[N] = { 0 }, in_a[N] = { 0 }, in_b[N] = { 0 };
    ref[0]           = 1.0f;
    in_a[0]          = 1.0f; /* peak at (0,0) */
    in_b[2 * NX + 5] = 1.0f; /* peak at (2,5) */

    dp_detector2d_state_t *det = dp_detector2d_create (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    det_result2d_t res[4];

    DP_CHECK (dp_detector2d_push (det, in_a, N, res, 4) == 1);
    float stat_open = det->test_stat;
    DP_CHECK (stat_open > 1.0f);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    /* Raise the gate above what the surface produces: nothing emitted ... */
    dp_detector2d_set_threshold (det, stat_open * 10.0f);
    DP_CHECK (det->threshold == stat_open * 10.0f);
    DP_CHECK (dp_detector2d_push (det, in_b, N, res, 4) == 0);

    /* ... and yet the last-dump POSITION followed the new input, which is
       the claim. A stale copy would still read (0,0) from in_a. */
    DP_CHECK (det->peak_row == 2 && det->peak_col == 5);
    DP_CHECK (det->_last_corr_valid == 1);

    /* Dropping the gate re-opens it with no rebuild. */
    dp_detector2d_set_threshold (det, 0.0f);
    DP_CHECK (dp_detector2d_push (det, in_a, N, res, 4) == 1);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    dp_detector2d_destroy (det);
  }

  /* ── the noise aggregates, directly ─────────────────────────────────────
   * det_noise_chunk is the scratch-free MEAN/MIN/MAX form a parallel tile
   * calls (acq); det_noise_estimate adds the MEDIAN, which needs a scratch.
   * Both against the independent _agg oracle, plus the contract the callers
   * lean on: an empty range is 0, and a chunk cannot be a median (0, not a
   * crash) -- it takes no scratch at all, which is what acq's tile relies
   * on. */
  {
    const float v[10]
        = { 4.0f, -1.0f, 9.0f, 2.5f, 7.0f, 0.0f, 3.0f, 8.0f, 5.5f, 1.0f };
    float                  scratch[10];
    const det_noise_mode_t modes[4]
        = { DET_NOISE_MEAN, DET_NOISE_MEDIAN, DET_NOISE_MIN, DET_NOISE_MAX };
    for (int m = 0; m < 4; m++)
      {
        const float want = _agg (v, 2, 8, modes[m]);
        DP_CHECK_NEAR (det_noise_estimate (v, 2, 8, scratch, modes[m]), want,
                       1e-6f);
        if (modes[m] != DET_NOISE_MEDIAN)
          DP_CHECK_NEAR (det_noise_chunk (v, 2, 8, modes[m]), want, 1e-6f);
        /* An empty range (lo > hi) aggregates to 0, scratch or not. */
        DP_CHECK (det_noise_estimate (v, 5, 4, scratch, modes[m]) == 0.0f);
        DP_CHECK (det_noise_chunk (v, 5, 4, modes[m]) == 0.0f);
      }
    /* A chunk has no median form: documented as 0. */
    DP_CHECK (det_noise_chunk (v, 2, 8, DET_NOISE_MEDIAN) == 0.0f);
    /* One bin is its own aggregate in every mode. */
    for (int m = 0; m < 4; m++)
      DP_CHECK (det_noise_estimate (v, 3, 3, scratch, modes[m]) == v[3]);
  }

  /* chunk invariance: the detections are a function of the input stream,
   * not of how push() calls cut it. A partial NY x NX frame is carried
   * between calls, and so is a dwell's coherent sum, so both are exercised:
   * every frame firing (dwell 1, threshold 0), and a gated dwell of 2 under
   * the median estimator. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_d2_stream (ref, x);

    const ci_d2_cfg_t cfgs[] = {
      { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f },
      { ref, 2, 1, N - 2, DET_NOISE_MEDIAN, 2.0f },
    };
    for (size_t c = 0; c < sizeof cfgs / sizeof *cfgs; c++)
      {
        dp_ci_spec_t spec = {
          .name     = c == 0 ? "detector2d push, dwell 1"
                             : "detector2d push, dwell 2, median",
          .create   = ci_d2_create,
          .destroy  = ci_d2_destroy,
          .process  = ci_d2_push,
          .arg      = (void *)&cfgs[c],
          .in_size  = sizeof (float _Complex),
          .out_size = sizeof (det_result2d_t),
          .out_cap  = CI_LEN / N + 1,
          .equal    = ci_d2_equal,
          .frame_n  = N,
        };
        DP_CHECK (dp_chunk_invariance (&spec, x, CI_LEN) == 0);
      }
  }

  /* stop and resume: a full result[] stops a push and never loses input.
   * Every frame fires (dwell 1, threshold 0), so room for ONE detection
   * fills on every frame. Offering the stream in seeded random chunks, each
   * re-offered from dp_detector2d_consumed() until it is used up, must give
   * exactly the detections of one push with room for all of them, never
   * write past the room, and stop on a frame boundary. Then room for none
   * takes nothing, and room for one takes frame 0 and not a sample more. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_d2_stream (ref, x);
    const ci_d2_cfg_t cfg = { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f };
    det_result2d_t    want[CI_LEN / N + 1], got[CI_LEN / N + 4];

    dp_detector2d_state_t *one = ci_d2_create ((void *)&cfg);
    dp_detector2d_state_t *d   = ci_d2_create ((void *)&cfg);
    DP_CHECK (one != NULL && d != NULL);
    DP_CHECK (d != NULL && dp_detector2d_consumed (d) == 0); /* create */
    const size_t n_want
        = dp_detector2d_push (one, x, CI_LEN, want, CI_LEN / N + 1);
    DP_CHECK (n_want == CI_LEN / N);                   /* every frame */
    DP_CHECK (dp_detector2d_consumed (one) == CI_LEN); /* room: all   */

    size_t       stops, wrong_stop, overfull;
    const size_t n_got
        = ci_d2_resume (d, x, CI_LEN, 1, 3 * N, got, CI_LEN / N + 4, &stops,
                        &wrong_stop, &overfull);
    DP_CHECK (stops > 0);
    DP_CHECK (wrong_stop == 0);
    DP_CHECK (overfull == 0);
    DP_CHECK (n_got == n_want && ci_d2_equal (got, want, n_want));
    dp_detector2d_reset (d);
    DP_CHECK (dp_detector2d_consumed (d) == 0); /* after reset */

    DP_CHECK (dp_detector2d_push (d, x, 3 * N, got, 0) == 0);
    DP_CHECK (dp_detector2d_consumed (d) == 0);
    DP_CHECK (dp_detector2d_push (d, x, 3 * N, got, 1) == 1);
    /* frame 0, and not a sample more */
    DP_CHECK (dp_detector2d_consumed (d) == N);
    DP_CHECK (ci_d2_equal (got, want, 1));
    dp_detector2d_destroy (one);
    dp_detector2d_destroy (d);
  }

  /* stop and resume with room for 2 and 3: room for one cannot tell the
   * batched feed (as many frames as there are free slots) from one frame at
   * a time. Both configurations, the gated dwell of 2 too, in chunks of up
   * to the whole stream so several dumps land in one call. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_d2_stream (ref, x);
    const ci_d2_cfg_t cfgs[] = {
      { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f },
      { ref, 2, 1, N - 2, DET_NOISE_MEDIAN, 2.0f },
    };
    for (size_t c = 0; c < sizeof cfgs / sizeof *cfgs; c++)
      for (size_t cap = 2; cap <= 3; cap++)
        {
          det_result2d_t         want[CI_LEN / N + 1], got[CI_LEN / N + 4];
          dp_detector2d_state_t *one = ci_d2_create ((void *)&cfgs[c]);
          dp_detector2d_state_t *d   = ci_d2_create ((void *)&cfgs[c]);
          DP_CHECK (one != NULL && d != NULL);
          if (!one || !d)
            continue;
          const size_t n_want
              = dp_detector2d_push (one, x, CI_LEN, want, CI_LEN / N + 1);
          size_t       stops, wrong_stop, overfull;
          const size_t n_got
              = ci_d2_resume (d, x, CI_LEN, cap, CI_LEN, got, CI_LEN / N + 4,
                              &stops, &wrong_stop, &overfull);
          DP_CHECK (stops > 0);
          DP_CHECK (wrong_stop == 0);
          DP_CHECK (overfull == 0);
          DP_CHECK (n_got == n_want && ci_d2_equal (got, want, n_want));
          dp_detector2d_destroy (one);
          dp_detector2d_destroy (d);
        }
  }

  DP_TEST_END ("test_detector2d_core");
}
