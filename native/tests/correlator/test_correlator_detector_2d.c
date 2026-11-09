#include "doppler/correlator/correlator_core.h"

/* correlator_private.h needs det_noise_mode_t from the header above, and the
   include sorter would put it first; its own block keeps the order. */
#include "doppler/correlator/correlator_private.h"

#include "dp_chunk_inv.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
  return dp_correlator_create_detector_2d (c->ref, NY, NX, c->dwell,
                                           c->noise_lo, c->noise_hi,
                                           c->noise_mode, c->threshold, 1);
}

static void
ci_d2_destroy (void *obj)
{
  dp_correlator_destroy ((dp_correlator_state_t *)obj);
}

/* out_cap is the result cap: sized so no call can fill it (one result per
   frame at most), so every frame of every partition is drained. */
static size_t
ci_d2_push (void *obj, const void *in, size_t n, void *out, size_t out_cap)
{
  return dp_correlator_push ((dp_correlator_state_t *)obj,
                             (const float _Complex *)in, n,
                             (dp_correlator_hit_t *)out, out_cap);
}

/* Exact, field by field: dp_correlator_hit_t has padding after its two size_t
   and three floats, and the harness's default memcmp would compare that
   too. Each float is compared by its bytes, so -0.0 and +0.0 still differ. */
static int
ci_d2_equal (const void *a, const void *b, size_t n)
{
  const dp_correlator_hit_t *x = (const dp_correlator_hit_t *)a;
  const dp_correlator_hit_t *y = (const dp_correlator_hit_t *)b;
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
   dp_correlator_consumed() until it is used up, and return the detections
   in got[]. Counts the calls that stopped short, the stops anywhere but on a
   frame boundary (frames tile the stream from sample 0, and a full push
   that stops short stops on one), and any call that wrote past its room --
   literally: the slot just past the room holds a byte pattern during the
   call, and a call that changed it, or reports more than its room, counts.
   got[] has room for got_len results. */
static size_t
ci_d2_resume (dp_correlator_state_t *d, const float _Complex *x, size_t len,
              size_t cap, size_t max_chunk, dp_correlator_hit_t *got,
              size_t got_len, size_t *stops, size_t *wrong_stop,
              size_t *overfull)
{
  size_t              n_got = 0, off = 0;
  uint32_t            r = 0x1895u;
  dp_correlator_hit_t sentinel;
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
              = dp_correlator_push (d, x + off, offered, got + n_got, cap);
          const size_t took = dp_correlator_consumed (d);
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

/* det_peak_scan()'s definition: the first maximum among the candidates,
   one cell at a time, where a NaN is never a candidate (U2). The masked
   scan skips whole blocks a lane-wise pass says cannot move the pick; this
   is what it must agree with, cell for cell, so the oracle is the plain
   loop and never the implementation. */
static size_t
_scan_oracle (const float *surf, const uint8_t *mask, size_t k0, size_t k1)
{
  size_t best = k1;
  for (size_t k = k0; k < k1; k++)
    if (!mask[k] && !isnan (surf[k]) && (best == k1 || surf[k] > surf[best]))
      best = k;
  return best;
}

/* ── The detector flavors' blob (#2752 step 3.3) ──────────────────────────
 *
 * A detector's CORL blob carries the map's keys, the count and the
 * accumulator, then noise_lo, noise_hi and noise_mode (reject keys), the
 * threshold (a mutator's value, restored), the framer's child blob and the
 * last dump. The pins: each reject key refuses a blob that differs in it
 * alone, at an equal blob size (a REQUIRE, so a size refusal cannot stand
 * in for the key); a detector blob and a map blob of one shape refuse each
 * other; the threshold is restored; and a refused blob changes nothing,
 * the framer child included, which is checked before anything is written. */

/* SRC mid-dwell (one frame toward a dwell of 3, a carry of 3 samples)
   loaded into DST, which must refuse it and stay as it was. */
static int
_det_cross_load_refused (dp_correlator_state_t *src,
                         dp_correlator_state_t *dst, const float _Complex *x,
                         size_t n, int same_size)
{
  dp_correlator_hit_t hits[4];
  DP_REQUIRE (src && dst);
  if (same_size)
    DP_REQUIRE (dp_correlator_state_bytes (src)
                == dp_correlator_state_bytes (dst));
  else
    DP_REQUIRE (dp_correlator_state_bytes (src)
                != dp_correlator_state_bytes (dst));
  if (dp_correlator_push (src, x, n + 3, hits, 4) == SIZE_MAX)
    {
      float _Complex out[4 * 64];
      DP_REQUIRE (dp_correlator_execute (src, x, n, out, 4 * 64, NULL) == 0);
    }
  DP_REQUIRE (src->count == 1);
  const size_t   sb   = dp_correlator_state_bytes (src);
  const size_t   db   = dp_correlator_state_bytes (dst);
  unsigned char *blob = malloc (sb);
  unsigned char *pre  = malloc (db);
  unsigned char *post = malloc (db);
  DP_REQUIRE (blob && pre && post);
  dp_correlator_get_state (src, blob);
  DP_CHECK (dp_correlator_set_state (src, blob) == DP_OK);
  dp_correlator_get_state (dst, pre);
  DP_CHECK (dp_correlator_set_state (dst, pre) == DP_OK);
  DP_CHECK (dp_correlator_set_state (dst, blob) == DP_ERR_INVALID);
  dp_correlator_get_state (dst, post);
  DP_CHECK (memcmp (pre, post, db) == 0);
  DP_CHECK (dst->count == 0);
  free (blob);
  free (pre);
  free (post);
  return 0;
}

static int
_test_detector_blob (void)
{
  enum
  {
    BY = 4,
    BX = 16,
    BN = BY * BX
  };
  float _Complex ref[BN] = { 0 }, x[2 * BN];
  for (size_t i = 0; i < BX; i++)
    ref[i] = (float)(i % 5) - 2.0f + I * (float)(i % 3); /* row 0 only */
  for (size_t i = 0; i < 2 * BN; i++)
    x[i] = (float)(i % 7) - 3.0f + I * (float)(i % 2);

  /* Each pair differs in one key, checked to be that key and no other the
     blob carries, so a refusal is that key's. */
  struct
  {
    const char      *key;
    size_t           lo_a, hi_a, lo_b, hi_b;
    det_noise_mode_t m_a, m_b;
  } pairs[] = {
    { "noise_lo", 1, BN - 1, 2, BN - 1, DET_NOISE_MEAN, DET_NOISE_MEAN },
    { "noise_hi", 1, BN - 1, 1, BN - 2, DET_NOISE_MEAN, DET_NOISE_MEAN },
    { "noise_mode", 1, BN - 1, 1, BN - 1, DET_NOISE_MEAN, DET_NOISE_MAX },
  };
  for (size_t p = 0; p < sizeof pairs / sizeof pairs[0]; p++)
    {
      dp_correlator_state_t *a = dp_correlator_create_detector_2d (
          ref, BY, BX, 3, pairs[p].lo_a, pairs[p].hi_a, pairs[p].m_a, 0.0f, 1);
      dp_correlator_state_t *b = dp_correlator_create_detector_2d (
          ref, BY, BX, 3, pairs[p].lo_b, pairs[p].hi_b, pairs[p].m_b, 0.0f, 1);
      DP_REQUIRE_MSG (a && b, pairs[p].key);
      const int differ[]
          = { a->noise_lo != b->noise_lo, a->noise_hi != b->noise_hi,
              a->noise_mode != b->noise_mode };
      int n_differ = 0;
      for (size_t k = 0; k < 3; k++)
        n_differ += differ[k];
      DP_REQUIRE_MSG (differ[p] && n_differ == 1 && a->flavor == b->flavor,
                      pairs[p].key);
      if (_det_cross_load_refused (a, b, x, BN, 1))
        return 1;
      dp_correlator_destroy (a);
      dp_correlator_destroy (b);
    }

  /* The flavor: the 1-D detector against the 2-D one at one row, the same
     shape, keys and blob size. */
  for (int dir = 0; dir < 2; dir++)
    {
      dp_correlator_state_t *a = dp_correlator_create_detector_1d (
          ref, BX, 3, 1, BX - 1, DET_NOISE_MEAN, 0.0f, 1);
      dp_correlator_state_t *b = dp_correlator_create_detector_2d (
          ref, 1, BX, 3, 1, BX - 1, DET_NOISE_MEAN, 0.0f, 1);
      DP_REQUIRE (a && b && a->flavor != b->flavor);
      if (dir ? _det_cross_load_refused (b, a, x, BX, 1)
              : _det_cross_load_refused (a, b, x, BX, 1))
        return 1;
      dp_correlator_destroy (a);
      dp_correlator_destroy (b);
    }

  /* A detector and a map of one shape refuse each other's blob, both ways
     (the sizes differ, so the envelope refuses; the flavor would too). */
  {
    dp_correlator_state_t *d = dp_correlator_create_detector_2d (
        ref, BY, BX, 3, 1, BN - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_state_t *m
        = dp_correlator_create_2d (ref, BY, BX, 3, 1, 0, 0, -1);
    if (_det_cross_load_refused (d, m, x, BN, 0))
      return 1;
    dp_correlator_destroy (d);
    dp_correlator_destroy (m);
    d = dp_correlator_create_detector_2d (ref, BY, BX, 3, 1, BN - 1,
                                          DET_NOISE_MEAN, 0.0f, 1);
    m = dp_correlator_create_2d (ref, BY, BX, 3, 1, 0, 0, -1);
    if (_det_cross_load_refused (m, d, x, BN, 0))
      return 1;
    dp_correlator_destroy (d);
    dp_correlator_destroy (m);
  }

  /* The threshold is a mutator's value, so it travels with the blob. */
  {
    dp_correlator_state_t *a = dp_correlator_create_detector_2d (
        ref, BY, BX, 3, 1, BN - 1, DET_NOISE_MEAN, 2.0f, 1);
    dp_correlator_state_t *b = dp_correlator_create_detector_2d (
        ref, BY, BX, 3, 1, BN - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_REQUIRE (a && b);
    DP_CHECK (dp_correlator_set_threshold (a, 7.5f) == DP_OK);
    const size_t   sb   = dp_correlator_state_bytes (a);
    unsigned char *blob = malloc (sb);
    DP_REQUIRE (blob);
    dp_correlator_get_state (a, blob);
    DP_CHECK (dp_correlator_set_state (b, blob) == DP_OK);
    DP_CHECK (b->threshold == 7.5f);
    free (blob);
    dp_correlator_destroy (a);
    dp_correlator_destroy (b);
  }

  /* Validate before commit: a blob whose framer child is forged is refused
     with nothing written -- the count, the accumulator, the threshold and
     the carry all as they were. The framer child is the blob's last
     check, after the keys and the accumulator; a restore that committed as
     it read would have written those already. */
  {
    dp_correlator_hit_t    hits[4];
    dp_correlator_state_t *a = dp_correlator_create_detector_2d (
        ref, BY, BX, 3, 1, BN - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_state_t *b = dp_correlator_create_detector_2d (
        ref, BY, BX, 3, 1, BN - 1, DET_NOISE_MEAN, 4.0f, 1);
    DP_REQUIRE (a && b);
    (void)dp_correlator_push (a, x, BN + 3, hits, 4); /* count 1, carry 3 */
    (void)dp_correlator_push (b, x, 2 * BN, hits, 4); /* count 2, no carry */
    DP_REQUIRE (a->count == 1 && b->count == 2);
    const size_t   sb   = dp_correlator_state_bytes (a);
    unsigned char *blob = malloc (sb), *pre = malloc (sb), *post = malloc (sb);
    DP_REQUIRE (blob && pre && post);
    dp_correlator_get_state (a, blob);
    dp_correlator_get_state (b, pre);
    /* The framer child starts after the header, the seven map u64s, the
       accumulator, the three noise u64s and the threshold. */
    const size_t fr = sizeof (dp_state_hdr_t) + 7 * sizeof (uint64_t)
                      + BN * sizeof (float _Complex) + 3 * sizeof (uint64_t)
                      + sizeof (float);
    DP_REQUIRE (fr + sizeof (dp_state_hdr_t) < sb);
    blob[fr] ^= 0x5A; /* the child's magic */
    DP_CHECK (dp_correlator_set_state (b, blob) == DP_ERR_INVALID);
    dp_correlator_get_state (b, post);
    DP_CHECK (memcmp (pre, post, sb) == 0);
    DP_CHECK (b->count == 2 && b->threshold == 4.0f);
    blob[fr] ^= 0x5A; /* the positive control: the unforged blob loads */
    DP_CHECK (dp_correlator_set_state (b, blob) == DP_OK);
    DP_CHECK (b->count == 1 && b->threshold == 0.0f);

    /* The count must name the framer's position: one frame handed out at
       dwell 3 is count 1, so a blob claiming count 2 is refused, by
       set_state and by the check alike, and changes nothing. The count is
       the seventh u64 after the header. */
    const size_t cnt = sizeof (dp_state_hdr_t) + 6 * sizeof (uint64_t);
    uint64_t     c64 = 2;
    memcpy (blob + cnt, &c64, sizeof c64);
    dp_correlator_get_state (b, pre);
    DP_CHECK (dp_correlator_state_check (b, blob) == DP_ERR_INVALID);
    DP_CHECK (dp_correlator_set_state (b, blob) == DP_ERR_INVALID);
    dp_correlator_get_state (b, post);
    DP_CHECK (memcmp (pre, post, sb) == 0);
    c64 = 1;
    memcpy (blob + cnt, &c64, sizeof c64);
    DP_CHECK (dp_correlator_state_check (b, blob) == DP_OK);

    /* The last peak must be a cell of the map: a row at ny is refused. It
       follows the framer child. */
    const size_t pk  = fr + dp_f32_framer_state_bytes (&a->framer);
    uint64_t     r64 = BY;
    memcpy (blob + pk, &r64, sizeof r64);
    DP_CHECK (dp_correlator_state_check (b, blob) == DP_ERR_INVALID);
    DP_CHECK (dp_correlator_set_state (b, blob) == DP_ERR_INVALID);
    dp_correlator_get_state (b, post);
    DP_CHECK (memcmp (pre, post, sb) == 0);
    free (blob);
    free (pre);
    free (post);
    dp_correlator_destroy (a);
    dp_correlator_destroy (b);
  }
  return 0;
}

/* Each flavor refuses the other's methods, with nothing changed: a
   detector every execute, batch and bank call (its frames come through
   push), and a map push and set_threshold. */
static void
_noop_fold (void *ctx, size_t h, const float _Complex *map, size_t n)
{
  (void)map, (void)n;
  *(size_t *)ctx += h + 1; /* any call at all shows */
}

static int
_test_flavor_refusals (void)
{
  enum
  {
    RY = 2,
    RX = 8,
    RN = RY * RX
  };
  float _Complex ref[RN] = { 0 }, x[RN], cout[4 * RN];
  float    fout[4 * RN], f3[4];
  uint32_t u1[4 * RN], u2[4];
  int32_t  i1[4 * RN];
  uint32_t u3[4 * RN], u4[4 * RN];
  for (size_t i = 0; i < RX; i++)
    ref[i] = (float)(i % 3) - 1.0f + I;
  for (size_t i = 0; i < RN; i++)
    x[i] = (float)(i % 5) + 0.5f * I;

  /* Both detector flavors: the 2-D one, and the 1-D one at RN samples. */
  for (int one = 0; one < 2; one++)
    {
      dp_correlator_state_t *d
          = one ? dp_correlator_create_detector_1d (ref, RN, 2, 1, RN - 1,
                                                    DET_NOISE_MEAN, 0.0f, 1)
                : dp_correlator_create_detector_2d (ref, RY, RX, 2, 1, RN - 1,
                                                    DET_NOISE_MEAN, 0.0f, 1);
      DP_REQUIRE (d);
      size_t               calls = 0;
      dp_correlator_fold_t fold  = { _noop_fold, &calls };
      DP_CHECK (dp_correlator_execute_fold (d, x, RN, &fold) == SIZE_MAX);
      DP_CHECK (dp_correlator_bank_fold (d, x, RN, 1, 0, 0, NULL, 0, &fold)
                == SIZE_MAX);
      DP_CHECK (calls == 0);
      DP_CHECK (dp_correlator_execute (d, x, RN, cout, RN, NULL) == SIZE_MAX);
      DP_CHECK (dp_correlator_execute_all (d, x, RN, cout, 4 * RN, NULL)
                == SIZE_MAX);
      DP_CHECK (dp_correlator_execute_coherent (d, x, RN, cout, 4 * RN, NULL)
                == SIZE_MAX);
      DP_CHECK (dp_correlator_execute_power_sum (d, x, RN, fout, 4 * RN, NULL)
                == SIZE_MAX);
      DP_CHECK (
          dp_correlator_execute_power_max (d, x, RN, fout, u1, 4 * RN, NULL)
          == SIZE_MAX);
      DP_CHECK (
          dp_correlator_execute_peaks (d, x, RN, fout, u1, u2, f3, 4, NULL)
          == SIZE_MAX);
      DP_CHECK (dp_correlator_bank_power_max (d, x, RN, 1, 0, 0, NULL, 0, fout,
                                              u1, 4 * RN, NULL, NULL)
                == SIZE_MAX);
      DP_CHECK (dp_correlator_bank_detect (d, x, RN, 1, 1, 0, 0, 1.0f, 1.0f,
                                           NULL, 0, fout, i1, u1, u3, u4,
                                           4 * RN, NULL, NULL)
                == SIZE_MAX);
      DP_CHECK (d->count == 0 && d->_last_corr_valid == 0);
      dp_correlator_destroy (d);
    }

  dp_correlator_state_t *m
      = dp_correlator_create_2d (ref, RY, RX, 2, 1, 0, 0, -1);
  DP_REQUIRE (m);
  dp_correlator_hit_t hits[2];
  DP_CHECK (dp_correlator_push (m, x, RN, hits, 2) == SIZE_MAX);
  DP_CHECK (dp_correlator_consumed (m) == 0);
  DP_CHECK (dp_correlator_set_threshold (m, 3.0f) == DP_ERR_UNSUPPORTED);
  DP_CHECK (m->count == 0 && m->threshold == 0.0f);
  dp_correlator_destroy (m);
  return 0;
}

int
main (void)
{

  /* ── lifecycle ────────────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->ny == NY);
    DP_CHECK (det->nx == NX);
    DP_CHECK (det->n == N);
    DP_CHECK (det->ring_cap >= N);
    DP_CHECK (det->ring != NULL);
    DP_CHECK (det->flavor
              == DP_CORRELATOR_DETECTOR_2D); /* the detector flavor */
    DP_CHECK (det->_last_corr_valid == 0);

    dp_correlator_destroy (det);
    dp_correlator_destroy (NULL);
  }

  /* ── noise_hi sentinel clamp ────────────────────────────────────── *
   * The binding passes (size_t)-1 for the documented "ny*nx-1" default. *
   * It must clamp to N-1, not overflow the scratch sizing / OOB-read.   */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, (size_t)-1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    DP_CHECK (det->noise_lo == 0);
    DP_CHECK (det->noise_hi == N - 1);

    dp_correlator_hit_t results[16];
    size_t              ndet = dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);
    DP_CHECK (isfinite (results[0].noise_est) && results[0].noise_est > 0.0f);
    DP_CHECK (isfinite (results[0].test_stat) && results[0].test_stat > 1.0f);

    dp_correlator_destroy (det);
  }

  /* ── impulse ref: peak at (row=0, col=0) ────────────────────────── *
   * noise_lo=0 includes the peak so noise_est = 1/N > 0 and            *
   * test_stat = N >> 1.                                                 */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[16];
    size_t              ndet = dp_correlator_push (det, ref, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 0);
    DP_CHECK (results[0].col == 0);
    DP_CHECK (results[0].peak_mag > 0.9f && results[0].peak_mag < 1.1f);
    DP_CHECK (results[0].test_stat > 1.0f);
    DP_CHECK (det->_last_corr_valid == 1);

    dp_correlator_destroy (det);
  }

  /* ── sub-frame push ──────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[16];

    size_t n1 = dp_correlator_push (det, ref, N / 2, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = dp_correlator_push (det, ref + N / 2, N / 2, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].row == 0 && results[0].col == 0);

    dp_correlator_destroy (det);
  }

  /* ── dwell=2 ─────────────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[16];

    size_t n1 = dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (n1 == 0);

    size_t n2 = dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (n2 == 1);
    DP_CHECK (results[0].peak_mag > 1.9f && results[0].peak_mag < 2.1f);

    dp_correlator_destroy (det);
  }

  /* ── 2-D shift: ref=δ[0,0] in=δ[1,0] → peak at (row=1, col=0) ──── */
  {
    float _Complex ref[N] = { 0 };
    float _Complex in[N]  = { 0 };
    ref[0]                = 1.0f;
    in[NX]                = 1.0f; /* row 1, col 0 */

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[16];
    size_t              ndet = dp_correlator_push (det, in, N, results, 16);

    DP_CHECK (ndet == 1);
    DP_CHECK (results[0].row == 1);
    DP_CHECK (results[0].col == 0);

    dp_correlator_destroy (det);
  }

  /* ── threshold gate ──────────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 1000.0f, 1);
    dp_correlator_hit_t results[16];
    size_t              ndet = dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 0);

    dp_correlator_set_threshold (det, 0.0f);
    ndet = dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (ndet == 1);

    dp_correlator_destroy (det);
  }

  /* ── reset clears state ──────────────────────────────────────────── */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 2, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[16];

    dp_correlator_push (det, ref, N, results, 16);
    DP_CHECK (det->count == 1);

    dp_correlator_reset (det);
    DP_CHECK (det->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);

    dp_correlator_destroy (det);
  }

  /* serializable state — corr2d child + ring residual + result fields. */
  {
    float _Complex ref[16], in[72];
    dp_correlator_hit_t res[16];
    for (int i = 0; i < 16; i++)
      ref[i] = (float)(i % 4) + 0.5f * I;
    for (int i = 0; i < 72; i++)
      in[i] = (float)(i % 3) - 1.0f + 0.2f * I;
    dp_correlator_state_t *a = dp_correlator_create_detector_2d (
        ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_state_t *b = dp_correlator_create_detector_2d (
        ref, 4, 4, 3, 1, 15, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_correlator_push (a, in, 72, res, 16);
    DP_STATE_ROUNDTRIP_TEST (dp_correlator, a, b);
    DP_CHECK (b->count == a->count); /* the dwell resumed */
    /* 72 samples at 4 x 4: four frames, so a dump at the third and
       one frame toward the next, and a carry of 8 */
    DP_CHECK (a->count == 1);
    DP_CHECK (dp_f32_framer_pending (&a->framer) == 8);
    DP_CHECK (dp_f32_framer_pending (&b->framer)
              == dp_f32_framer_pending (&a->framer));
    DP_CHECK (dp_correlator_consumed (a) == 72); /* the push took all of in */
    DP_CHECK (dp_correlator_consumed (b) == 0);  /* set_state: no last push */
    /* the last surface is not state: a's push dumped, b has no view */
    DP_CHECK (a->_last_corr_valid == 1);
    DP_CHECK (b->_last_corr_valid == 0);
    dp_correlator_destroy (a);
    dp_correlator_destroy (b);
  }

  /* ── dwell = 0 is refused, inherited from corr2d ──────────────────────
   *
   * detector2d's own header says dwell "must be >= 1" and it validates
   * nothing itself -- it forwards straight to dp_correlator_create_2d, which
   * is where the rule now lives (see test_correlator_core.c). Pinned here too
   * because this is the object a caller constructs, and because it is the
   * assertion that would notice if the forwarding were ever replaced by a
   * local copy that forgot the rule. */
  {
    float _Complex ref[N] = { 0 };
    ref[0]                = 1.0f;
    DP_CHECK (dp_correlator_create_detector_2d (ref, NY, NX, 0, 1, N - 1,
                                                DET_NOISE_MEAN, 0.0f, 1)
              == NULL);
    /* Not vacuous: the neighbouring value builds. */
    dp_correlator_state_t *ok = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (ok != NULL);
    dp_correlator_destroy (ok);
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
        dp_correlator_state_t *det = dp_correlator_create_detector_2d (
            ref, NY, NX, 1, lo, hi, (det_noise_mode_t)m, 0.0f, 1);
        DP_CHECK (det != NULL);
        dp_correlator_hit_t res[4];
        size_t              nd = dp_correlator_push (det, in, N, res, 4);
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
        dp_correlator_destroy (det);
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
   * reference is accepted, and it returns -1 when dp_correlator_set_ref
   * refuses.
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

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref_a, NY, NX, 2, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    dp_correlator_hit_t res[4];

    /* Dirty EVERY piece of state the reset is supposed to clear, or the
       assertions below pass on things that were already clean. Measured
       while writing this: asserting only `corr->count == 0` is satisfied
       by dp_correlator_set_ref's own reset, so it tests corr2d rather than
       this function -- deleting dp_correlator_reset from set_ref left it
       green. The ring residue and the last-dump flag are what set_ref adds. */
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 0); /* dwell 1 of 2 */
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 1); /* dumps       */
    DP_CHECK (det->_last_corr_valid == 1);
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 0); /* accumulator */
    DP_CHECK (det->count == 1);
    DP_CHECK (dp_correlator_push (det, in, N / 2, res, 4) == 0); /* carry    */
    DP_CHECK (dp_f32_framer_pending (&det->framer) == N / 2);
    DP_CHECK (dp_correlator_consumed (det) == N / 2);

    /* Accepted, and it reset all three even though the reference was fine. */
    DP_CHECK (dp_correlator_set_ref (det, ref_b) == 0);
    DP_CHECK (det->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);
    DP_CHECK (dp_f32_framer_pending (&det->framer) == 0);
    DP_CHECK (dp_correlator_consumed (det) == 0);

    /* The new reference is genuinely in force: the impulse moved two
       columns, so the correlation peak moves with it. */
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 0);
    size_t nd = dp_correlator_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2);

    /* A multi-row reference is REFUSED on a fast-path object, and the
       refusal is not destructive -- the object still works afterwards.
       It still resets first (the header's contract, CorrDetector2D's
       since before the one correlator): dirty the count, the carry and the
       last dump, and all three must be clear after the refusal. The
       accepted branch above cannot tell, because an accepted set_ref
       resets on its own way out. */
    DP_CHECK (det->_last_corr_valid == 1);
    /* in is one frame long: a whole frame, then half of one. */
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 0);
    DP_CHECK (dp_correlator_push (det, in, N / 2, res, 4) == 0);
    DP_CHECK (det->count == 1);
    DP_CHECK (dp_f32_framer_pending (&det->framer) == N / 2);
    DP_CHECK (dp_correlator_set_ref (det, ref_2d) != 0);
    DP_CHECK (det->count == 0);
    DP_CHECK (det->_last_corr_valid == 0);
    DP_CHECK (dp_f32_framer_pending (&det->framer) == 0);
    DP_CHECK (dp_correlator_push (det, in, N, res, 4) == 0);
    nd = dp_correlator_push (det, in, N, res, 4);
    DP_CHECK (nd == 1);
    if (nd == 1)
      DP_CHECK (res[0].row == 0 && res[0].col == NX - 2); /* still ref_b */

    dp_correlator_destroy (det);
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

    dp_correlator_state_t *det = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f, 1);
    DP_CHECK (det != NULL);
    dp_correlator_hit_t res[4];

    DP_CHECK (dp_correlator_push (det, in_a, N, res, 4) == 1);
    float stat_open = det->test_stat;
    DP_CHECK (stat_open > 1.0f);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    /* Raise the gate above what the surface produces: nothing emitted ... */
    dp_correlator_set_threshold (det, stat_open * 10.0f);
    DP_CHECK (det->threshold == stat_open * 10.0f);
    DP_CHECK (dp_correlator_push (det, in_b, N, res, 4) == 0);

    /* ... and yet the last-dump POSITION followed the new input, which is
       the claim. A stale copy would still read (0,0) from in_a. */
    DP_CHECK (det->peak_row == 2 && det->peak_col == 5);
    DP_CHECK (det->_last_corr_valid == 1);

    /* Dropping the gate re-opens it with no rebuild. */
    dp_correlator_set_threshold (det, 0.0f);
    DP_CHECK (dp_correlator_push (det, in_a, N, res, 4) == 1);
    DP_CHECK (det->peak_row == 0 && det->peak_col == 0);

    dp_correlator_destroy (det);
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
          .out_size = sizeof (dp_correlator_hit_t),
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
   * re-offered from dp_correlator_consumed() until it is used up, must give
   * exactly the detections of one push with room for all of them, never
   * write past the room, and stop on a frame boundary. Then room for none
   * takes nothing, and room for one takes frame 0 and not a sample more. */
  {
    float _Complex ref[N];
    static float _Complex x[CI_LEN];
    ci_d2_stream (ref, x);
    const ci_d2_cfg_t   cfg = { ref, 1, 0, N - 1, DET_NOISE_MEAN, 0.0f };
    dp_correlator_hit_t want[CI_LEN / N + 1], got[CI_LEN / N + 4];

    dp_correlator_state_t *one = ci_d2_create ((void *)&cfg);
    dp_correlator_state_t *d   = ci_d2_create ((void *)&cfg);
    DP_CHECK (one != NULL && d != NULL);
    DP_CHECK (d != NULL && dp_correlator_consumed (d) == 0); /* create */
    const size_t n_want
        = dp_correlator_push (one, x, CI_LEN, want, CI_LEN / N + 1);
    DP_CHECK (n_want == CI_LEN / N);                   /* every frame */
    DP_CHECK (dp_correlator_consumed (one) == CI_LEN); /* room: all   */

    size_t       stops, wrong_stop, overfull;
    const size_t n_got
        = ci_d2_resume (d, x, CI_LEN, 1, 3 * N, got, CI_LEN / N + 4, &stops,
                        &wrong_stop, &overfull);
    DP_CHECK (stops > 0);
    DP_CHECK (wrong_stop == 0);
    DP_CHECK (overfull == 0);
    DP_CHECK (n_got == n_want && ci_d2_equal (got, want, n_want));
    dp_correlator_reset (d);
    DP_CHECK (dp_correlator_consumed (d) == 0); /* after reset */

    DP_CHECK (dp_correlator_push (d, x, 3 * N, got, 0) == 0);
    DP_CHECK (dp_correlator_consumed (d) == 0);
    DP_CHECK (dp_correlator_push (d, x, 3 * N, got, 1) == 1);
    /* frame 0, and not a sample more */
    DP_CHECK (dp_correlator_consumed (d) == N);
    DP_CHECK (ci_d2_equal (got, want, 1));

    /* A full push keeps a rest that completes no frame, as the carry, and
     * takes nothing of one that would. Continuing the stream at frame 1:
     * room 1 on N + 3 takes frame 1 and the 3 after it; room 0 is full from
     * the start, so it takes N - 4 more (the carry reaches N - 1, still no
     * frame) and then not the sample that would complete frame 2; room 1
     * takes that sample and reports frame 2. */
    DP_CHECK (dp_correlator_push (d, x + N, N + 3, got, 1) == 1);
    DP_CHECK (dp_correlator_consumed (d) == N + 3);
    DP_CHECK (dp_f32_framer_pending (&d->framer) == 3);
    DP_CHECK (ci_d2_equal (got, want + 1, 1));
    DP_CHECK (dp_correlator_push (d, x + 2 * N + 3, N - 4, got, 0) == 0);
    DP_CHECK (dp_correlator_consumed (d) == N - 4);
    DP_CHECK (dp_correlator_push (d, x + 3 * N - 1, 1, got, 0) == 0);
    DP_CHECK (dp_correlator_consumed (d) == 0);
    DP_CHECK (dp_correlator_push (d, x + 3 * N - 1, 1, got, 1) == 1);
    DP_CHECK (dp_correlator_consumed (d) == 1);
    DP_CHECK (ci_d2_equal (got, want + 2, 1));
    dp_correlator_destroy (one);
    dp_correlator_destroy (d);
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
          dp_correlator_hit_t    want[CI_LEN / N + 1], got[CI_LEN / N + 4];
          dp_correlator_state_t *one = ci_d2_create ((void *)&cfgs[c]);
          dp_correlator_state_t *d   = ci_d2_create ((void *)&cfgs[c]);
          DP_CHECK (one != NULL && d != NULL);
          if (!one || !d)
            continue;
          const size_t n_want
              = dp_correlator_push (one, x, CI_LEN, want, CI_LEN / N + 1);
          size_t       stops, wrong_stop, overfull;
          const size_t n_got
              = ci_d2_resume (d, x, CI_LEN, cap, CI_LEN, got, CI_LEN / N + 4,
                              &stops, &wrong_stop, &overfull);
          DP_CHECK (stops > 0);
          DP_CHECK (wrong_stop == 0);
          DP_CHECK (overfull == 0);
          DP_CHECK (n_got == n_want && ci_d2_equal (got, want, n_want));
          dp_correlator_destroy (one);
          dp_correlator_destroy (d);
        }
  }

  /* reset clears the last dump's fields, so a reset detector's blob is a
     fresh one's (doppler#2642). */
  {
    float _Complex ref[N]    = { 0 };
    ref[0]                   = 1.0f;
    dp_correlator_state_t *a = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, (size_t)-1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_state_t *b = dp_correlator_create_detector_2d (
        ref, NY, NX, 1, 0, (size_t)-1, DET_NOISE_MEAN, 0.0f, 1);
    dp_correlator_hit_t results[4];
    DP_CHECK (dp_correlator_push (a, ref, N, results, 4) == 1);
    DP_CHECK (a->peak_mag != 0.0f);
    dp_correlator_reset (a);
    size_t         sb = dp_correlator_state_bytes (a);
    unsigned char *ba = malloc (sb), *bb = malloc (sb);
    dp_correlator_get_state (a, ba);
    dp_correlator_get_state (b, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);
    free (ba);
    free (bb);
    dp_correlator_destroy (a);
    dp_correlator_destroy (b);
  }

  /* ── det_peak_scan, masked, over many blocks ──────────────────────────
   * The masked scan runs the plain loop only on the blocks whose largest
   * candidate beats the pick so far, so every case here spans several
   * blocks: candidates dense and sparse, whole blocks masked, a maximum in
   * the partial tail block, a candidate only in the last block, ties across
   * blocks (the FIRST maximum), and ranges that start mid-block, as the
   * acquisition engine's per-tile chunks do. */
  {
    enum
    {
      SN = 3001
    };
    float   *surf = malloc (SN * sizeof *surf);
    uint8_t *mask = malloc (SN);
    DP_REQUIRE (surf && mask);
    uint32_t     rs        = 0x5ca9u;
    const double density[] = { 0.0, 0.5, 0.97, 1.0 };
    size_t       cases = 0, agree = 0;
    for (int ties = 0; ties < 2; ties++)
      for (size_t d = 0; d < sizeof density / sizeof *density; d++)
        for (int trial = 0; trial < 40; trial++)
          {
            for (size_t k = 0; k < SN; k++)
              {
                /* Ties: a few levels, so equal maxima land in many blocks. */
                const double u = dp_uni (&rs);
                surf[k]        = ties ? (float)(int)(u * 4.0) : (float)u;
                mask[k]        = dp_uni (&rs) < density[d];
              }
            if (trial % 4 == 1) /* the tail block holds the maximum */
              {
                surf[SN - 2] = 10.0f;
                mask[SN - 2] = 0;
              }
            if (trial % 4 == 2) /* only the last cell is a candidate */
              {
                memset (mask, 1, SN);
                mask[SN - 1] = 0;
              }
            const size_t k0
                = trial % 3 == 0 ? 0 : (size_t)(dp_uni (&rs) * 700);
            const size_t k1
                = trial % 3 == 0 ? SN : SN - (size_t)(dp_uni (&rs) * 700);
            cases++;
            agree += det_peak_scan (surf, mask, k0, k1)
                     == _scan_oracle (surf, mask, k0, k1);
          }
    DP_CHECK (cases == 320);
    DP_CHECK (agree == cases);
    /* A masked cell larger than every candidate is never the pick. */
    for (size_t k = 0; k < SN; k++)
      {
        surf[k] = 1.0f;
        mask[k] = 0;
      }
    surf[1500] = 9.0f;
    mask[1500] = 1;
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 0);
    /* Ties, placed: the maximum duplicated in two blocks, then twice in one
       block -- the FIRST index wins, as in the plain loop. */
    memset (mask, 0, SN);
    for (size_t k = 0; k < SN; k++)
      surf[k] = 1.0f;
    surf[300]  = 5.0f; /* block 1 */
    surf[2000] = 5.0f; /* block 7 */
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 300);
    DP_CHECK (det_peak_scan (surf, mask, 301, SN) == 2000);
    surf[300] = surf[2000] = 1.0f;
    surf[600] = surf[700] = 5.0f; /* both in block 2 */
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 600);
    /* All candidates negative: a masked cell must not read as 0 and win. */
    for (size_t k = 0; k < SN; k++)
      {
        surf[k] = -1.0f - (float)k;
        mask[k] = k % 3 == 0;
      }
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 1);
    DP_CHECK (det_peak_scan (surf, mask, 0, SN)
              == _scan_oracle (surf, mask, 0, SN));
    /* NaN: a NaN is never a candidate (U2). Nothing compares larger than
       a NaN, so a scan that let one in as its FIRST candidate would keep it
       against every later cell; the lane-wise pass must not let a NaN hide
       its block's true maximum either. */
    memset (mask, 0, SN);
    for (size_t k = 0; k < SN; k++)
      surf[k] = 1.0f;
    /* The true maximum in the NaN's own LANE (8 cells on), after it and
       then before it: a lane max that let the NaN through would read NaN
       there, and the block would be skipped. */
    surf[1100] = NAN;
    surf[1108] = 7.0f;
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 1108);
    surf[1108] = NAN;
    surf[1100] = 7.0f;
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 1100);
    DP_CHECK (det_peak_scan (surf, mask, 0, SN)
              == _scan_oracle (surf, mask, 0, SN));
    /* A NaN at the first candidate: the true maximum is still the pick,
       masked and unmasked alike (both seeds skip it; the masked lane pass
       also maps it to -INFINITY), at k0 = 0 and at an interior k0. */
    surf[0] = NAN;
    DP_CHECK (_scan_oracle (surf, mask, 0, SN) == 1100);
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 1100);
    DP_CHECK (det_peak_scan (surf, NULL, 0, SN) == 1100);
    surf[500] = NAN;
    DP_CHECK (det_peak_scan (surf, mask, 500, SN) == 1100);
    DP_CHECK (det_peak_scan (surf, NULL, 500, SN) == 1100);
    /* A NaN run at the start, then finite cells below the maximum: the
       seed walks the run and the plain argmax takes over. */
    for (size_t k = 0; k < 20; k++)
      surf[k] = NAN;
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == 1100);
    DP_CHECK (det_peak_scan (surf, NULL, 0, SN) == 1100);
    /* A range of NaNs only: k1 from both paths, as a fully masked range,
       and det_peak_list lists nothing (and reads no cell past the end). */
    DP_CHECK (det_peak_scan (surf, mask, 0, 20) == 20);
    DP_CHECK (det_peak_scan (surf, NULL, 0, 20) == 20);
    DP_CHECK (det_peak_scan (surf, NULL, 3, 20) == 20);
    {
      det_peak_t pk = { 7, 7, 7.0f };
      DP_CHECK (det_peak_list (surf, 4, 5, -1.0f, 0, 0, NULL, &pk, 1) == 0);
      DP_CHECK (pk.row == 7 && pk.col == 7); /* untouched */
    }
    /* Both detectors' call, at both shapes: det_peak_list at one peak with
       no gate and no mask, over a 1 x n row (the 1-D detector) and an
       ny x nx surface (the 2-D one). A NaN in cell 0 beside a finite peak:
       the finite peak is listed. A NaN beside the peak elsewhere: the peak
       is listed as before. Neither is reachable through push, since a NaN
       sample spreads through the transform to every cell; push's all-NaN
       frame is pinned in each detector's own test. */
    for (int sh = 0; sh < 2; sh++)
      {
        const size_t ny = sh ? 4 : 1, nx = sh ? 8 : 32, n = ny * nx;
        float        s2[32];
        for (size_t k = 0; k < n; k++)
          s2[k] = 1.0f;
        s2[13]        = 5.0f;
        s2[0]         = NAN;
        det_peak_t pk = { 0 };
        DP_CHECK (det_peak_list (s2, ny, nx, -1.0f, 0, 0, NULL, &pk, 1) == 1);
        DP_CHECK (pk.row == 13 / nx && pk.col == 13 % nx);
        DP_CHECK (pk.value == 5.0f);
        s2[0]  = 1.0f;
        s2[14] = NAN;
        pk     = (det_peak_t){ 0 };
        DP_CHECK (det_peak_list (s2, ny, nx, -1.0f, 0, 0, NULL, &pk, 1) == 1);
        DP_CHECK (pk.row == 13 / nx && pk.col == 13 % nx);
        DP_CHECK (pk.value == 5.0f);
      }
    /* Nothing a candidate: k1. */
    memset (mask, 1, SN);
    DP_CHECK (det_peak_scan (surf, mask, 0, SN) == SN);
    DP_CHECK (det_peak_scan (surf, mask, 7, 7) == 7);
    free (surf);
    free (mask);
  }

  /* -- U2: an all-NaN frame, as test_correlator_detector_1d.c's: the peak
     reads (0, 0), magnitude +0.0, the noise NaN and test_stat 0, so threshold
     0 emits that hit and a positive threshold none. A single NaN cell is not
     reachable through push (it spreads through the transform); the picker
     is pinned for it above, at both detectors' shapes. */
  for (int m = 0; m < 4; m++)
    for (int t = 0; t < 2; t++)
      {
        float _Complex ref[N] = { 0 }, x[N];
        ref[0]                = 1.0f;
        for (size_t i = 0; i < N; i++)
          x[i] = NAN;
        const float            thr = t ? 2.5f : 0.0f;
        dp_correlator_state_t *det = dp_correlator_create_detector_2d (
            ref, NY, NX, 1, 0, N - 1, (det_noise_mode_t)m, thr, 1);
        DP_REQUIRE (det != NULL);
        dp_correlator_hit_t r[2]  = { { .row       = 9,
                                        .col       = 9,
                                        .peak_mag  = 9.0f,
                                        .noise_est = 9.0f,
                                        .test_stat = 9.0f } };
        const size_t        nhits = dp_correlator_push (det, x, N, r, 2);
        DP_CHECK (nhits == (t ? 0u : 1u));
        DP_CHECK (det->peak_row == 0 && det->peak_col == 0);
        DP_CHECK (det->peak_mag == 0.0f && !signbit (det->peak_mag));
        DP_CHECK (isnan (det->noise_est));
        DP_CHECK (det->test_stat == 0.0f);
        if (nhits == 1)
          DP_CHECK (r[0].row == 0 && r[0].col == 0 && r[0].peak_mag == 0.0f
                    && isnan (r[0].noise_est) && r[0].test_stat == 0.0f);
        dp_correlator_destroy (det);
      }

  DP_REQUIRE (_test_detector_blob () == 0);
  DP_REQUIRE (_test_flavor_refusals () == 0);

  DP_TEST_END ("test_correlator_detector_2d");
}
