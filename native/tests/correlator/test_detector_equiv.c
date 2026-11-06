/* test_detector_equiv.c -- the 1-D detector is the 2-D one at one row, hit
 * for hit (#2752 step 3.3, docs/design/correlator.md section 2.1).
 *
 * Step 3.3 retires `CorrDetector` and `CorrDetector2D` by making them the
 * two detector flavors of the one correlator, the 1-D one being the 2-D one
 * at ny = 1. That is a refactor only if the two objects agreed, so this
 * test pushed the same streams through the retired `dp_detector_*` (n) and
 * `dp_detector2d_*` (ny = 1, nx = n) first, in the commit "test(detector):
 * CorrDetector is CorrDetector2D at one row, hit for hit" (cited by
 * subject, since a rebase changes its hash). It now runs the two detector
 * flavors, dp_correlator_create_detector_1d and _2d at ny = 1, and compares
 * everything a caller can see, exactly, never by tolerance:
 *
 *   - every push's return (hits written) and its consumed count;
 *   - every hit: (lag, peak, noise, stat) against (0, col, peak, noise,
 *     stat), the floats by bit pattern;
 *   - the stored last dump (lag against row 0 and col, peak, noise, stat),
 *     whether a correlation is held, and the held correlation by memcmp;
 *   - the dwell count;
 *   - the state blob, which is one layout for both and differs only in its
 *     first field, the flavor (a reject key, so a 1-D detector's blob never
 *     restores into the 2-D one).
 *
 * Ties are in the grid on purpose: a zero dump ties every cell, and both
 * must report the first.
 *
 * The grid is the design page's prototype, committed: the four noise
 * modes, dwells 1 and 2, n = 31, 64 and 127 (prime, a power of two, and
 * 2^k - 1), two noise windows (the full default and an interior one), two
 * thresholds (0, which always fires, and 2.5, which a noise-only frame
 * mostly fails), and four chunkings of one stream: 7, 13 and 1000 samples a
 * push, and the whole stream in one push. The stream mixes frames that
 * carry the reference at a planted circular lag with noise-only frames, so
 * both sides of the gate are exercised.
 *
 * One frame of each stream is NaN. Until U2 (docs/design/correlator.md)
 * the two disagreed there: the 1-D detector reported the NaN as its peak
 * and the 2-D one listed nothing. Both now take the one picker, where a
 * NaN is never a candidate.
 *
 * Proven red by sabotage on the retired objects' sources, each turning
 * this test red: the 2-D detector's noise window shifted by one bin; the
 * 1-D argmax taking `>=` (the last maximum, not the first); and a 2-D
 * dwell one frame short.  Against the flavors both sides run the same
 * code, so for the arithmetic it is circular; it is kept for what still
 * differs between them, the flavor and its blob key, and for the 1-D
 * flavor's own direct single-row call, which the 2-D one does not take.
 */
#include "doppler/correlator/correlator_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_LEN 3
#define N_DWELL 2
#define N_MODE 4
#define N_WIN 2
#define N_THR 2
#define N_CHUNK 4
static const size_t lens[N_LEN]       = { 31, 64, 127 };
static const size_t dwells[N_DWELL]   = { 1, 2 };
static const float  thresholds[N_THR] = { 0.0f, 2.5f };
static const size_t chunks[N_CHUNK]   = { 7, 13, 1000, 0 }; /* 0 = whole */

/* Frames a stream carries: enough for several dumps at dwell 2, and to
   cross a ring wrap at the shortest n. */
#define N_FRAMES 40

/* One frame of NaN: its dump's map is NaN in every cell, which both must
   report alike (U2: a NaN is never the peak). Frame 21 is mid-stream and
   the second of a dwell-2 pair. */
#define NAN_FRAME 21

/* Room for every hit a push can make: one per frame. */
#define MAX_HITS (N_FRAMES + 1)

static int
same_f32 (float a, float b)
{
  return memcmp (&a, &b, sizeof a) == 0;
}

/* The stream: frame f carries the reference at circular lag (5 f + 3) mod
   n, scaled, over unit-variance noise, except every third frame, which is
   noise only, and frames 6 and 7 of every 8, which are all zero. A zero
   pair makes a zero dump at dwell 1 and 2 alike, where every cell ties:
   the tie rule (the first maximum, lag 0) is compared there, which noise
   alone never reaches. */
static void
make_stream (float _Complex *x, const float _Complex *ref, size_t n,
             uint32_t *st)
{
  for (size_t f = 0; f < N_FRAMES; f++)
    {
      const size_t    lag = (5 * f + 3) % n;
      const int       sig = f % 3 != 2;
      float _Complex *y   = x + f * n;
      if (f % 8 >= 6)
        {
          memset (y, 0, n * sizeof *y);
          continue;
        }
      if (f == NAN_FRAME)
        {
          for (size_t i = 0; i < n; i++)
            y[i] = NAN;
          continue;
        }
      for (size_t i = 0; i < n; i++)
        {
          y[i] = dp_cgauss (st);
          if (sig)
            y[i] += 0.8f * ref[(i + n - lag) % n];
        }
    }
}

/* The two blobs: equal size; each names its own flavor first, and the rest
   of the payload is equal. The flavor refuses: a 1-D blob does not restore
   into the 2-D twin, nor the reverse. */
static int
check_blobs (const dp_correlator_state_t *a, dp_correlator_state_t *b)
{
  const size_t na = dp_correlator_state_bytes (a);
  DP_REQUIRE (na == dp_correlator_state_bytes (b));
  unsigned char *ba = malloc (na), *bb = malloc (na);
  DP_REQUIRE (ba && bb);
  dp_correlator_get_state (a, ba);
  dp_correlator_get_state (b, bb);
  const size_t h = sizeof (dp_state_hdr_t);
  uint64_t     fa, fb;
  memcpy (&fa, ba + h, sizeof fa);
  memcpy (&fb, bb + h, sizeof fb);
  DP_CHECK (fa == DP_CORRELATOR_DETECTOR_1D
            && fb == DP_CORRELATOR_DETECTOR_2D);
  DP_CHECK (memcmp (ba + h + sizeof fa, bb + h + sizeof fa, na - h - sizeof fa)
            == 0);
  DP_CHECK (dp_correlator_set_state (b, ba) == DP_ERR_INVALID);
  free (ba);
  free (bb);
  return 0;
}

/* Both detectors' stored state after a push, compared. */
static int
check_state (const dp_correlator_state_t *a, const dp_correlator_state_t *b)
{
  DP_CHECK (a->peak_col == b->peak_col);
  DP_CHECK (a->peak_row == 0 && b->peak_row == 0);
  DP_CHECK (same_f32 (a->peak_mag, b->peak_mag));
  DP_CHECK (same_f32 (a->noise_est, b->noise_est));
  DP_CHECK (same_f32 (a->test_stat, b->test_stat));
  DP_CHECK (a->_last_corr_valid == b->_last_corr_valid);
  if (a->_last_corr_valid && b->_last_corr_valid)
    DP_CHECK (memcmp (a->det_out, b->det_out, a->n * sizeof *a->det_out) == 0);
  DP_CHECK (a->count == b->count);
  return 0;
}

/* One stream through both, pushed `chunk` samples at a time. Returns 0,
   or 1 when a REQUIRE failed; *hits receives the hits compared. */
static int
run_case (size_t n, size_t dwell, det_noise_mode_t mode, size_t lo, size_t hi,
          float thr, size_t chunk, uint32_t seed, size_t *hits)
{
  const size_t    total = (size_t)N_FRAMES * n;
  float _Complex *ref   = malloc (n * sizeof *ref);
  float _Complex *x     = malloc (total * sizeof *x);
  DP_REQUIRE (ref && x);
  uint32_t st = seed;
  for (size_t i = 0; i < n; i++)
    ref[i] = dp_cgauss (&st);
  make_stream (x, ref, n, &st);

  dp_correlator_state_t *a
      = dp_correlator_create_detector_1d (ref, n, dwell, lo, hi, mode, thr, 1);
  dp_correlator_state_t *b = dp_correlator_create_detector_2d (
      ref, 1, n, dwell, lo, hi, mode, thr, 1);
  DP_REQUIRE (a && b);
  DP_CHECK (a->noise_lo == b->noise_lo && a->noise_hi == b->noise_hi);

  dp_correlator_hit_t ra[MAX_HITS];
  dp_correlator_hit_t rb[MAX_HITS];
  const size_t        step = chunk ? chunk : total;
  *hits                    = 0;
  for (size_t off = 0; off < total;)
    {
      const size_t len = total - off < step ? total - off : step;
      memset (ra, 0xA5, sizeof ra);
      memset (rb, 0xA5, sizeof rb);
      const size_t ka = dp_correlator_push (a, x + off, len, ra, MAX_HITS);
      const size_t kb = dp_correlator_push (b, x + off, len, rb, MAX_HITS);
      DP_REQUIRE (ka == kb);
      DP_REQUIRE (dp_correlator_consumed (a) == dp_correlator_consumed (b));
      /* Room for every frame's hit, so a push takes all of its input. */
      DP_REQUIRE (dp_correlator_consumed (a) == len);
      for (size_t k = 0; k < ka; k++)
        {
          DP_CHECK (ra[k].row == 0 && rb[k].row == 0);
          DP_CHECK (ra[k].col == rb[k].col);
          DP_CHECK (same_f32 (ra[k].peak_mag, rb[k].peak_mag));
          DP_CHECK (same_f32 (ra[k].noise_est, rb[k].noise_est));
          DP_CHECK (same_f32 (ra[k].test_stat, rb[k].test_stat));
        }
      *hits += ka;
      if (check_state (a, b))
        return 1;
      off += len;
    }

  if (check_blobs (a, b))
    return 1;
  dp_correlator_destroy (a);
  dp_correlator_destroy (b);
  free (ref);
  free (x);
  return 0;
}

int
main (void)
{
  uint32_t seed  = 0x2753u;
  size_t   cases = 0, hits_open = 0, hits_gated = 0, frames_gated = 0;
  for (size_t l = 0; l < N_LEN; l++)
    for (size_t d = 0; d < N_DWELL; d++)
      for (int m = 0; m < N_MODE; m++)
        for (size_t w = 0; w < N_WIN; w++)
          for (size_t t = 0; t < N_THR; t++)
            {
              const size_t n = lens[l];
              /* The full window by the binding's sentinel, or the middle
                 half. */
              const size_t lo = w ? n / 4 : 0;
              const size_t hi = w ? (3 * n) / 4 : SIZE_MAX;
              /* One stream per case: every chunking pushes the same data
                 (the seed is fixed across the chunk loop). */
              const uint32_t s = seed++;
              for (size_t c = 0; c < N_CHUNK; c++)
                {
                  size_t hits = 0;
                  DP_REQUIRE (run_case (n, dwells[d], (det_noise_mode_t)m, lo,
                                        hi, thresholds[t], chunks[c], s, &hits)
                              == 0);
                  const size_t dumps = N_FRAMES / dwells[d];
                  if (t == 0)
                    {
                      /* Threshold 0 always fires: one hit per dump. */
                      DP_CHECK (hits == dumps);
                      hits_open += hits;
                    }
                  else
                    {
                      hits_gated += hits;
                      frames_gated += dumps;
                    }
                  cases++;
                }
            }
  DP_CHECK (cases
            == (size_t)N_LEN * N_DWELL * N_MODE * N_WIN * N_THR * N_CHUNK);
  /* The gate must have cut both ways somewhere, or the gated half of the
     grid compared only one side of it. */
  DP_CHECK (hits_gated > 0 && hits_gated < frames_gated);
  printf ("test_detector_equiv: %zu cases, %zu hits always-fire, %zu of %zu "
          "dumps through the gate\n",
          cases, hits_open, hits_gated, frames_gated);
  DP_TEST_END ("test_detector_equiv");
}
