/**
 * @file test_lo_core.c
 * @brief Unit tests for the LO (NCO + 2^16 LUT → CF32 phasors).
 *
 * Tests:
 *   1. Lifecycle  — create / reset / destroy
 *   2. DC tone    — norm_freq = 0 → all outputs are 1 + 0j
 *   3. Quarter-rate IQ — expected { 1+0j, 0+1j, -1+0j, 0-1j } × N
 *   4. Phase continuity — two blocks match one long block
 *   5. ctrl-port FM shift — lo_steps_ctrl deviates frequency per sample
 *   6. LUT accuracy — at quarter-rate, |out|² ≈ 1 and Im/Re quadrature
 *   7. Property accessors — get/set norm_freq, phase, phase_inc
 */
#include "lo/lo_core.h"
#include <math.h>
#include <stdio.h>

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

/* Absolute-value tolerance for floating-point comparisons.
 * The 2^16-entry LUT gives ~96 dBc SFDR; error on any single sample
 * is bounded by half the LUT bin width ≈ π/2^16 ≈ 4.8e-5.           */
#define TOL 1e-3f

static int
near (float a, float b)
{
  return fabsf (a - b) <= TOL;
}

static int
near_c (float complex a, float complex b)
{
  return near (crealf (a), crealf (b)) && near (cimagf (a), cimagf (b));
}

int
main (void)
{
  int _fails = 0;

  /* ----------------------------------------------------------------
   * 1. Lifecycle
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.0);
    CHECK (lo != NULL);
    if (!lo)
      return 1;
    lo_reset (lo);
    CHECK (lo_get_phase (lo) == 0u);
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 2. DC tone — norm_freq = 0 → phase_inc = 0 → all 1 + 0j
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.0);
    float complex out[8];
    lo_steps (lo, 8, out);
    for (int i = 0; i < 8; i++)
      CHECK (near_c (out[i], 1.0f + 0.0f * I));
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 3. Quarter-rate IQ
   *
   * norm_freq = 0.25 → phase_inc = 0x40000000.
   * Phase emitted before increment, LUT maps:
   *   phase=0x00000000 → cos=1, sin=0  →  1 + 0j
   *   phase=0x40000000 → cos=0, sin=1  →  0 + 1j
   *   phase=0x80000000 → cos=-1, sin=0 → -1 + 0j
   *   phase=0xC0000000 → cos=0, sin=-1 →  0 - 1j
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.25);
    float complex out[8];
    lo_steps (lo, 8, out);
    CHECK (near_c (out[0], 1.0f + 0.0f * I));
    CHECK (near_c (out[1], 0.0f + 1.0f * I));
    CHECK (near_c (out[2], -1.0f + 0.0f * I));
    CHECK (near_c (out[3], 0.0f - 1.0f * I));
    CHECK (near_c (out[4], 1.0f + 0.0f * I));
    CHECK (near_c (out[5], 0.0f + 1.0f * I));
    CHECK (near_c (out[6], -1.0f + 0.0f * I));
    CHECK (near_c (out[7], 0.0f - 1.0f * I));
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 4. Phase continuity across two blocks
   *
   * Two consecutive calls of length N must match a single call of 2N.
   * ---------------------------------------------------------------- */
  {
    lo_state_t *a = lo_create (0.1);
    lo_state_t *b = lo_create (0.1);
    float complex ref[16], blk[8];
    lo_steps (a, 16, ref);
    lo_steps (b, 8, blk);
    for (int i = 0; i < 8; i++)
      CHECK (near_c (blk[i], ref[i]));
    lo_steps (b, 8, blk);
    for (int i = 0; i < 8; i++)
      CHECK (near_c (blk[i], ref[8 + i]));
    lo_destroy (a);
    lo_destroy (b);
  }

  /* ----------------------------------------------------------------
   * 5. ctrl-port FM shift
   *
   * lo_steps_ctrl with a constant ctrl offset of 0.25 should produce
   * the same output as lo_steps at norm_freq + 0.25 — but without
   * modifying the LO's base norm_freq.
   *
   * Use a zero base (norm_freq = 0) and ctrl = 0.25 for all samples;
   * expect the same output as a DC+0.25 LO.
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo_ctrl = lo_create (0.0);
    lo_state_t *lo_ref = lo_create (0.25);

    float ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.25f;

    float complex out_ctrl[8], out_ref[8];
    lo_steps_ctrl (lo_ctrl, ctrl, 8, out_ctrl);
    lo_steps (lo_ref, 8, out_ref);

    for (int i = 0; i < 8; i++)
      CHECK (near_c (out_ctrl[i], out_ref[i]));

    /* Base norm_freq is unchanged after steps_ctrl. */
    CHECK (lo_get_norm_freq (lo_ctrl) == 0.0);

    lo_destroy (lo_ctrl);
    lo_destroy (lo_ref);
  }

  /* ----------------------------------------------------------------
   * 6. LUT accuracy — unit magnitude and I/Q orthogonality
   *
   * At quarter-rate, 4 consecutive phasors span the unit circle.
   * Verify |out[k]|² ≈ 1 and that out[1] is close to j·out[0].
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.25);
    float complex out[4];
    lo_steps (lo, 4, out);
    for (int k = 0; k < 4; k++)
      {
        float mag2 = crealf (out[k]) * crealf (out[k])
                     + cimagf (out[k]) * cimagf (out[k]);
        CHECK (near (mag2, 1.0f));
      }
    /* out[1] should equal j * out[0] at quarter-rate */
    float complex expected1 = I * out[0];
    CHECK (near_c (out[1], expected1));
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 7. Property accessors
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.25);
    CHECK (lo_get_norm_freq (lo) == 0.25);
    CHECK (lo_get_phase (lo) == 0u);
    CHECK (lo_get_phase_inc (lo) == 0x40000000u);

    lo_set_phase (lo, 0x80000000u);
    CHECK (lo_get_phase (lo) == 0x80000000u);

    lo_set_norm_freq (lo, 0.5);
    CHECK (lo_get_phase_inc (lo) == 0x80000000u);
    CHECK (lo_get_phase (lo) == 0x80000000u); /* unchanged */

    lo_reset (lo);
    CHECK (lo_get_phase (lo) == 0u);
    CHECK (lo_get_phase_inc (lo) == 0x80000000u); /* unchanged */

    lo_destroy (lo);
  }

  if (_fails)
    {
      fprintf (stderr, "test_lo_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_lo_core PASSED\n");
  return 0;
}
