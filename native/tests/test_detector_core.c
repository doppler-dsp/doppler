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

/* The stream both stream tests read: a random QPSK reference at irregular
   offsets in noise, so the lags and statistics differ from frame to frame.
   CI_LEN is 37 frames and a partial one. */
#define CI_LEN (37 * N + 21)
static void
ci_det_stream (float _Complex ref[N], float _Complex x[CI_LEN])
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
   dp_detector_consumed() until it is used up, and return the detections in
   got[]. Counts the calls that stopped short, the stops anywhere but one
   sample short of a frame (frames tile the stream from sample 0, and a full
   push still takes the carry), and any call that wrote past its room --
   literally: the slot just past the room holds a byte pattern during the
   call, and a call that changed it, or reports more than its room, counts.
   got[] has room for got_len results. */
static size_t
ci_det_resume (dp_detector_state_t *d, const float _Complex *x, size_t len,
               size_t cap, size_t max_chunk, det_result_t *got, size_t got_len,
               size_t *stops, size_t *wrong_stop, size_t *overfull)
{
  size_t       n_got = 0, off = 0;
  uint32_t     r = 0x1895u;
  det_result_t sentinel;
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
              = dp_detector_push (d, x + off, offered, got + n_got, cap);
          const size_t took = dp_detector_consumed (d);
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
              if (off % N != N - 1)
                (*wrong_stop)++;
            }
          if (took == 0)  /* cannot happen with room for one: stop the loop */
            return n_got; /* rather than spin, and let the caller fail     */
        }
    }
  return n_got;
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
    /* the carry: 24 samples at n = 16 is one frame and 8 left over */
    DP_CHECK (dp_f32_framer_pending (&a->framer) == 8);
    DP_CHECK (dp_f32_framer_pending (&b->framer)
              == dp_f32_framer_pending (&a->framer));
    DP_CHECK (dp_detector_consumed (a) == 24); /* the push took all of in */
    DP_CHECK (dp_detector_consumed (b) == 0);  /* set_state: no last push */
    DP_CHECK (b->_last_corr_valid == a->_last_corr_valid);
    dp_detector_destroy (a);
    dp_detector_destroy (b);
  }

  /* chunk invariance: the detections are a function of the input stream,
   * not of how push() calls cut it. A partial frame is carried between
   * calls, and so is a dwell's coherent sum, so both are exercised: every
   * frame firing (dwell 1, threshold 0), and a gated dwell of 4 under the
   * median estimator. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_det_stream (ref, x);

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
          .out_cap  = CI_LEN / N + 1,
          .equal    = ci_det_equal,
          .frame_n  = N,
        };
        DP_CHECK (dp_chunk_invariance (&spec, x, CI_LEN) == 0);
      }
  }

  /* stop and resume: a full result[] stops a push and never loses input.
   * Every frame fires (dwell 1, threshold 0), so room for ONE detection
   * fills on every frame. Offering the stream in seeded random chunks, each
   * re-offered from dp_detector_consumed() until it is used up, must give
   * exactly the detections of one push with room for all of them. And each
   * stopped call must have taken everything before the sample that would
   * complete a frame it had no room for: the frames tile the stream from
   * sample 0, so it stops with the stream position one short of a frame. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_det_stream (ref, x);
    const ci_det_cfg_t cfg = { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f };
    det_result_t       want[CI_LEN / N + 1], got[CI_LEN / N + 4];

    dp_detector_state_t *one = ci_det_create ((void *)&cfg);
    dp_detector_state_t *d   = ci_det_create ((void *)&cfg);
    DP_CHECK (one != NULL && d != NULL);
    DP_CHECK (d != NULL && dp_detector_consumed (d) == 0); /* after create */
    const size_t n_want
        = dp_detector_push (one, x, CI_LEN, want, CI_LEN / N + 1);
    DP_CHECK (n_want == CI_LEN / N);                 /* every frame fired */
    DP_CHECK (dp_detector_consumed (one) == CI_LEN); /* room: took all */

    size_t       stops, wrong_stop, overfull;
    const size_t n_got
        = ci_det_resume (d, x, CI_LEN, 1, 3 * N, got, CI_LEN / N + 4, &stops,
                         &wrong_stop, &overfull);
    DP_CHECK (stops > 0); /* the cap actually stopped some calls */
    DP_CHECK (wrong_stop == 0);
    DP_CHECK (overfull == 0);
    DP_CHECK (n_got == n_want);
    DP_CHECK (n_got == n_want && ci_det_equal (got, want, n_want));
    dp_detector_reset (d);
    DP_CHECK (dp_detector_consumed (d) == 0); /* after reset */

    /* Room for none: input that completes no frame is still taken whole --
     * it is the carry -- and the sample that would complete frame 0 is not.
     * Resumed with room for one, that frame's hit is the one-shot's first. */
    DP_CHECK (dp_detector_push (d, x, 3 * N, got, 0) == 0);
    DP_CHECK (dp_detector_consumed (d) == N - 1);
    DP_CHECK (dp_detector_push (d, x + N - 1, 2 * N, got, 1) == 1);
    DP_CHECK (dp_detector_consumed (d) == N); /* frame 0, then the carry */
    DP_CHECK (ci_det_equal (got, want, 1));
    dp_detector_destroy (one);
    dp_detector_destroy (d);
  }

  /* stop and resume with room for 2 and 3: room for one cannot tell the
   * batched feed (as many frames as there are free slots) from one frame
   * at a time, because there one slot is one frame. With more room, a
   * batch that over-feeds writes past result, and one that under-drains
   * strands frames; so both configurations run, the gated dwell of 4 too,
   * in chunks of up to the whole stream, so several dumps land in one call
   * (a dump every 4 frames needs over 12 to fill room for 3). Each must
   * give exactly the one-shot's detections, never
   * write past its room, and stop one sample short of a frame. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_det_stream (ref, x);
    const ci_det_cfg_t cfgs[] = {
      { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f },
      { ref, 4, 2, N - 4, DET_NOISE_MEDIAN, 3.0f },
    };
    for (size_t c = 0; c < sizeof cfgs / sizeof *cfgs; c++)
      for (size_t cap = 2; cap <= 3; cap++)
        {
          det_result_t         want[CI_LEN / N + 1], got[CI_LEN / N + 4];
          dp_detector_state_t *one = ci_det_create ((void *)&cfgs[c]);
          dp_detector_state_t *d   = ci_det_create ((void *)&cfgs[c]);
          DP_CHECK (one != NULL && d != NULL);
          if (!one || !d)
            continue;
          const size_t n_want
              = dp_detector_push (one, x, CI_LEN, want, CI_LEN / N + 1);
          size_t       stops, wrong_stop, overfull;
          const size_t n_got
              = ci_det_resume (d, x, CI_LEN, cap, CI_LEN, got, CI_LEN / N + 4,
                               &stops, &wrong_stop, &overfull);
          DP_CHECK (stops > 0); /* the room actually stopped calls */
          DP_CHECK (wrong_stop == 0);
          DP_CHECK (overfull == 0);
          DP_CHECK (n_got == n_want && ci_det_equal (got, want, n_want));
          dp_detector_destroy (one);
          dp_detector_destroy (d);
        }
  }

  DP_TEST_END ("test_detector_core");
}
