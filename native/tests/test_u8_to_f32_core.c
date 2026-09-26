/* test_u8_to_f32_core.c -- the offset-binary uint8 (RTL-SDR cu8) converter.
 *
 * Every claim in u8_to_f32_core.h that a number can settle is pinned here,
 * over ALL 256 codes rather than a few literals, because a converter has no
 * other inputs: the whole domain is cheaper to check than to argue about.
 *
 *   1. create() rejects an unknown mode.
 *   2. shift  == (x - 128) / 128 EXACTLY, for every code.
 *   3. shift reads 0.5/128 low: the mean over an analog zero (codes 127 and
 *      128, equally likely) is -0.5/128, and the mean over all 256 codes is
 *      the same -- the documented DC bias, measured rather than asserted.
 *   4. midpoint == (x - 127.5) / 127.5 to within the last bit, every code.
 *   5. midpoint is exactly antisymmetric, y(x) == -y(255 - x), so it is
 *      exactly zero-mean over all codes and over an analog zero.
 *   6. midpoint reaches both rails: y(0) == -1 and y(255) == +1.
 *   7. steps() == step() element for element, in both modes, for a block
 *      and for the same stream fed in uneven pieces (the block path is a
 *      separate loop per mode, so it must be checked, not assumed).
 *   8. reset() changes nothing.
 */
#include "doppler/u8_to_f32/u8_to_f32_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NSTREAM 1000

int
main (void)
{
  /* 1. mode validation */
  DP_CHECK (u8_to_f32_create (-1) == NULL);
  DP_CHECK (u8_to_f32_create (2) == NULL);

  u8_to_f32_state_t *sh = u8_to_f32_create (U8_TO_F32_SHIFT);
  u8_to_f32_state_t *mp = u8_to_f32_create (U8_TO_F32_MIDPOINT);
  DP_REQUIRE (sh != NULL && mp != NULL);

  /* 2. shift is exact over the whole domain */
  int shift_bad = 0;
  for (int x = 0; x < 256; x++)
    {
      float want = (float)(x - 128) / 128.0f; /* power-of-two divide: exact */
      if (u8_to_f32_step (sh, (uint8_t)x) != want)
        shift_bad++;
    }
  DP_CHECK_MSG (shift_bad == 0, "shift != (x - 128)/128 exactly");
  DP_CHECK (u8_to_f32_step (sh, 0) == -1.0f);
  DP_CHECK (u8_to_f32_step (sh, 128) == 0.0f);
  DP_CHECK (u8_to_f32_step (sh, 255) == 127.0f / 128.0f);

  /* 3. the shift DC bias, measured */
  double zero_mean
      = 0.5 * (u8_to_f32_step (sh, 127) + u8_to_f32_step (sh, 128));
  DP_CHECK_NEAR (zero_mean, -0.5 / 128.0, 0.0);
  double all_mean = 0.0;
  for (int x = 0; x < 256; x++)
    all_mean += u8_to_f32_step (sh, (uint8_t)x);
  all_mean /= 256.0;
  DP_CHECK_NEAR (all_mean, -0.5 / 128.0, 1e-12);

  /* 4. midpoint is the true quotient to within the last bit */
  double worst = 0.0;
  for (int x = 0; x < 256; x++)
    {
      double want = ((double)x - 127.5) / 127.5;
      double err  = fabs ((double)u8_to_f32_step (mp, (uint8_t)x) - want);
      if (err > worst)
        worst = err;
    }
  /* |y| <= 1, so one float ulp at the top of the range is FLT_EPSILON / 2
     below 1 and FLT_EPSILON at 1; allow one of the larger. */
  DP_CHECK_NEAR (worst, 0.0, FLT_EPSILON);

  /* 5. exact antisymmetry, hence exact zero mean */
  int    anti_bad = 0;
  double mp_mean  = 0.0;
  for (int x = 0; x < 256; x++)
    {
      float y = u8_to_f32_step (mp, (uint8_t)x);
      if (y != -u8_to_f32_step (mp, (uint8_t)(255 - x)))
        anti_bad++;
      mp_mean += y;
    }
  DP_CHECK_MSG (anti_bad == 0, "midpoint not exactly antisymmetric");
  DP_CHECK_NEAR (mp_mean, 0.0, 0.0);
  DP_CHECK_NEAR (u8_to_f32_step (mp, 127) + u8_to_f32_step (mp, 128), 0.0,
                 0.0);

  /* 6. both rails */
  DP_CHECK_NEAR (u8_to_f32_step (mp, 0), -1.0, FLT_EPSILON);
  DP_CHECK_NEAR (u8_to_f32_step (mp, 255), 1.0, FLT_EPSILON);

  /* 7. steps() agrees with step(), whole-block and piecewise */
  uint8_t  in[NSTREAM];
  float    blk[NSTREAM], pcs[NSTREAM];
  uint32_t rng = 12345u;
  for (int i = 0; i < NSTREAM; i++)
    in[i] = (uint8_t)(dp_xs32 (&rng) >> 24);
  u8_to_f32_state_t *objs[2] = { sh, mp };
  for (int k = 0; k < 2; k++)
    {
      u8_to_f32_steps (objs[k], in, blk, NSTREAM);
      int blk_bad = 0;
      for (int i = 0; i < NSTREAM; i++)
        if (blk[i] != u8_to_f32_step (objs[k], in[i]))
          blk_bad++;
      DP_CHECK_MSG (blk_bad == 0, "steps() != step() over a block");

      /* uneven pieces, including a zero-length and a single-sample call */
      static const size_t cut[] = { 0, 1, 7, 64, 3, 500, 0, 425 };
      size_t              off   = 0;
      for (size_t c = 0; c < sizeof cut / sizeof cut[0]; c++)
        {
          u8_to_f32_steps (objs[k], in + off, pcs + off, cut[c]);
          off += cut[c];
        }
      DP_REQUIRE (off == NSTREAM);
      DP_CHECK_MSG (memcmp (blk, pcs, sizeof blk) == 0,
                    "piecewise steps() != one block");
    }

  /* 8. reset is a no-op */
  float before = u8_to_f32_step (mp, 200);
  u8_to_f32_reset (mp);
  u8_to_f32_reset (sh);
  DP_CHECK (u8_to_f32_step (mp, 200) == before);
  DP_CHECK (u8_to_f32_step (sh, 200) == 72.0f / 128.0f);

  u8_to_f32_destroy (sh);
  u8_to_f32_destroy (mp);
  u8_to_f32_destroy (NULL);
  DP_TEST_END ("test_u8_to_f32_core");
}
