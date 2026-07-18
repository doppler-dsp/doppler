/**
 * @file test_nco_core.c
 * @brief Unit tests for the NCO pure phase-accumulator.
 *
 * Tests:
 *   1. Lifecycle  — create / reset / destroy
 *   2. Zero freq  — phase_inc = 0, all outputs are 0
 *   3. Quarter-rate — phase_inc = 0x40000000, 4-sample sequence
 *   4. Phase continuity — two consecutive blocks share state
 *   5. nmax scaling — steps_u32_scaled maps [0, 2^32) → [0, nmax)
 *   6. Overflow flag — carry fires exactly once per full cycle
 *   7. Property accessors — get/set norm_freq, phase, phase_inc
 *   8. ctrl-port FM shift — steps_u32_ctrl deviates phase per sample
 *      without touching phase_inc/norm_freq (mirrors lo_steps_ctrl)
 *   9. steps_u32_scaled_ctrl — nmax scaling + ctrl port combined
 *  10. steps_u32_ovf_ctrl — carry detection + ctrl port combined,
 *      including a ctrl large enough to force >1 wrap in one sample
 *  11. Single-sample primitives (nco_step_u32*) — every batch stepper
 *      is exactly a loop over its single-sample counterpart
 */
#include "nco/nco_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

int
main (void)
{
  int _fails = 0;

  /* ----------------------------------------------------------------
   * 1. Lifecycle
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.0, 0);
    CHECK (nco != NULL);
    if (!nco)
      return 1;
    nco_reset (nco);
    CHECK (nco_get_phase (nco) == 0);
    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * 2. Zero frequency — phase_inc = 0, accumulator stays at 0
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.0, 0);
    uint32_t     out[8];
    nco_steps_u32 (nco, 8, out);
    for (int i = 0; i < 8; i++)
      CHECK (out[i] == 0u);
    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * 3. Quarter-rate — norm_freq = 0.25 → phase_inc = 0x40000000
   *
   * Four consecutive samples (phase emitted before increment):
   *   out[0] = 0x00000000  (phase at entry)
   *   out[1] = 0x40000000
   *   out[2] = 0x80000000
   *   out[3] = 0xC0000000
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.25, 0);
    CHECK (nco_get_phase_inc (nco) == 0x40000000u);
    uint32_t out[4];
    nco_steps_u32 (nco, 4, out);
    CHECK (out[0] == 0x00000000u);
    CHECK (out[1] == 0x40000000u);
    CHECK (out[2] == 0x80000000u);
    CHECK (out[3] == 0xC0000000u);
    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * 4. Phase continuity across two blocks
   *
   * A single call of length 2N should produce the same values as
   * two consecutive calls of length N.
   * ---------------------------------------------------------------- */
  {
    nco_state_t *a = nco_create (0.1, 0);
    nco_state_t *b = nco_create (0.1, 0);
    uint32_t     ref[16], blk[8];
    nco_steps_u32 (a, 16, ref);
    nco_steps_u32 (b, 8, blk);
    for (int i = 0; i < 8; i++)
      CHECK (blk[i] == ref[i]);
    nco_steps_u32 (b, 8, blk);
    for (int i = 0; i < 8; i++)
      CHECK (blk[i] == ref[8 + i]);
    nco_destroy (a);
    nco_destroy (b);
  }

  /* ----------------------------------------------------------------
   * 5. nmax scaling — values mapped to [0, nmax)
   *
   * At quarter-rate (phase_inc = 0x40000000) with nmax = 4:
   *   out[k] = (uint64_t)(k * 0x40000000) * 4 >> 32 = k
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.25, 4);
    uint32_t     out[5];
    nco_steps_u32_scaled (nco, 5, out);
    CHECK (out[0] == 0u);
    CHECK (out[1] == 1u);
    CHECK (out[2] == 2u);
    CHECK (out[3] == 3u);
    CHECK (out[4] == 0u); /* wrapped back to 0 */
    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * 6. Overflow carry flag
   *
   * At norm_freq = 0.25, the accumulator wraps every 4 samples.
   * Steps 0–3: out[0]=0, carry only on step 4 (when wrap occurs).
   * Generate 8 samples; carry should fire at samples 4 and ...
   * Actually let's verify: phase starts at 0, inc = 0x40000000.
   *   sample 0: out=0x00000000, carry=0 (0+0x40000000=0x40000000, no wrap)
   *   sample 1: out=0x40000000, carry=0
   *   sample 2: out=0x80000000, carry=0
   *   sample 3: out=0xC0000000, carry=1 (0xC0000000+0x40000000 wraps)
   *   sample 4: out=0x00000000, carry=0
   *   sample 5: out=0x40000000, carry=0
   *   sample 6: out=0x80000000, carry=0
   *   sample 7: out=0xC0000000, carry=1
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.25, 0);
    uint32_t     ph[8];
    uint8_t      ov[8];
    nco_steps_u32_ovf (nco, 8, ph, ov);
    CHECK (ph[0] == 0x00000000u);
    CHECK (ov[0] == 0);
    CHECK (ph[1] == 0x40000000u);
    CHECK (ov[1] == 0);
    CHECK (ph[2] == 0x80000000u);
    CHECK (ov[2] == 0);
    CHECK (ph[3] == 0xC0000000u);
    CHECK (ov[3] == 1);
    CHECK (ph[4] == 0x00000000u);
    CHECK (ov[4] == 0);
    CHECK (ph[5] == 0x40000000u);
    CHECK (ov[5] == 0);
    CHECK (ph[6] == 0x80000000u);
    CHECK (ov[6] == 0);
    CHECK (ph[7] == 0xC0000000u);
    CHECK (ov[7] == 1);
    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * 7. Property accessors
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.25, 0);
    CHECK (nco_get_norm_freq (nco) == 0.25);
    CHECK (nco_get_phase (nco) == 0u);
    CHECK (nco_get_phase_inc (nco) == 0x40000000u);

    /* set_phase */
    nco_set_phase (nco, 0x80000000u);
    CHECK (nco_get_phase (nco) == 0x80000000u);

    /* set_norm_freq updates phase_inc but not phase */
    nco_set_norm_freq (nco, 0.5);
    CHECK (nco_get_phase_inc (nco) == 0x80000000u);
    CHECK (nco_get_phase (nco) == 0x80000000u); /* unchanged */

    /* reset zeroes phase only */
    nco_reset (nco);
    CHECK (nco_get_phase (nco) == 0u);
    CHECK (nco_get_phase_inc (nco) == 0x80000000u);

    nco_destroy (nco);
  }

  /* ----------------------------------------------------------------
   * Serializable state — advance, serialize, restore into a fresh NCO,
   * and the phase stream continues identically; a clobbered blob rejects.
   * ---------------------------------------------------------------- */
  {
    nco_state_t *a = nco_create (0.123, 0);
    uint32_t     ref[16], got[16];
    nco_steps_u32 (a, 5, ref); /* advance, then snapshot */
    size_t sb   = nco_state_bytes (a);
    void  *blob = malloc (sb);
    nco_get_state (a, blob);
    nco_steps_u32 (a, 16, ref); /* reference continuation */

    nco_state_t *b = nco_create (0.123, 0);
    CHECK (nco_set_state (b, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (nco_set_state (b, blob) == DP_ERR_INVALID);
    nco_steps_u32 (b, 16, got);
    for (int i = 0; i < 16; i++)
      CHECK (got[i] == ref[i]);
    nco_destroy (a);
    nco_destroy (b);
    free (blob);
  }

  /* ----------------------------------------------------------------
   * 8. ctrl-port FM shift
   *
   * steps_u32_ctrl with a constant ctrl offset of 0.25 must produce
   * the same phase sequence as steps_u32 at norm_freq + 0.25 -- but
   * without modifying the NCO's base norm_freq/phase_inc.
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco_ctrl = nco_create (0.0, 0);
    nco_state_t *nco_ref  = nco_create (0.25, 0);

    float ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.25f;

    uint32_t out_ctrl[8], out_ref[8];
    nco_steps_u32_ctrl (nco_ctrl, ctrl, 8, out_ctrl);
    nco_steps_u32 (nco_ref, 8, out_ref);

    for (int i = 0; i < 8; i++)
      CHECK (out_ctrl[i] == out_ref[i]);

    /* Base norm_freq/phase_inc unchanged after steps_u32_ctrl. */
    CHECK (nco_get_norm_freq (nco_ctrl) == 0.0);
    CHECK (nco_get_phase_inc (nco_ctrl) == 0u);

    nco_destroy (nco_ctrl);
    nco_destroy (nco_ref);
  }

  /* ----------------------------------------------------------------
   * 9. steps_u32_scaled_ctrl — nmax scaling + ctrl port combined
   *
   * A constant ctrl offset of 0.25 at nmax=4, norm_freq=0.0 must match
   * plain steps_u32_scaled at norm_freq=0.25, nmax=4 (same identity as
   * test 8, through the scaled output mapping instead of raw).
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco_ctrl = nco_create (0.0, 4);
    nco_state_t *nco_ref  = nco_create (0.25, 4);

    float ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.25f;

    uint32_t out_ctrl[8], out_ref[8];
    nco_steps_u32_scaled_ctrl (nco_ctrl, ctrl, 8, out_ctrl);
    nco_steps_u32_scaled (nco_ref, 8, out_ref);

    for (int i = 0; i < 8; i++)
      CHECK (out_ctrl[i] == out_ref[i]);
    CHECK (nco_get_norm_freq (nco_ctrl) == 0.0);
    CHECK (nco_get_phase_inc (nco_ctrl) == 0u);

    /* nmax == 0 falls back to raw, identical to steps_u32_ctrl. */
    nco_state_t *nco_raw = nco_create (0.0, 0);
    uint32_t     out_raw[8], out_plain[8];
    nco_steps_u32_scaled_ctrl (nco_raw, ctrl, 8, out_raw);
    nco_state_t *nco_plain = nco_create (0.0, 0);
    nco_steps_u32_ctrl (nco_plain, ctrl, 8, out_plain);
    for (int i = 0; i < 8; i++)
      CHECK (out_raw[i] == out_plain[i]);

    nco_destroy (nco_ctrl);
    nco_destroy (nco_ref);
    nco_destroy (nco_raw);
    nco_destroy (nco_plain);
  }

  /* ----------------------------------------------------------------
   * 10. steps_u32_ovf_ctrl — carry detection + ctrl port combined
   *
   * A constant ctrl of 0.25 at norm_freq=0.0 must match plain
   * steps_u32_ovf at norm_freq=0.25 (phase AND carry). Separately, a
   * ctrl large enough that phase_inc + ctrl_inc alone would overflow a
   * plain uint32 add must still report the correct single-wrap carry
   * (the 64-bit-sum path, not a naive uint32 add).
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco_ctrl = nco_create (0.0, 0);
    nco_state_t *nco_ref  = nco_create (0.25, 0);
    float        ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.25f;
    uint32_t ph_ctrl[8], ph_ref[8];
    uint8_t  ov_ctrl[8], ov_ref[8];
    nco_steps_u32_ovf_ctrl (nco_ctrl, ctrl, 8, ph_ctrl, ov_ctrl);
    nco_steps_u32_ovf (nco_ref, 8, ph_ref, ov_ref);
    for (int i = 0; i < 8; i++)
      {
        CHECK (ph_ctrl[i] == ph_ref[i]);
        CHECK (ov_ctrl[i] == ov_ref[i]);
      }
    nco_destroy (nco_ctrl);
    nco_destroy (nco_ref);

    /* norm_freq=0.9, ctrl=0.9 -> phase_inc + ctrl_inc sums to just
       under 2 full cycles (>2^32 as a plain uint32 add would silently
       wrap the SUM itself before it's even added to phase) -- the
       64-bit-sum implementation must still report exactly one carry
       per sample, every sample, matching the two-NCO cross-check: one
       step at combined rate 1.8 cyc/sample is the same as ANY single
       step whose total advance exceeds one full cycle. */
    nco_state_t *nco_big = nco_create (0.9, 0);
    float        big_ctrl[4];
    for (int i = 0; i < 4; i++)
      big_ctrl[i] = 0.9f;
    uint32_t ph_big[4];
    uint8_t  ov_big[4];
    nco_steps_u32_ovf_ctrl (nco_big, big_ctrl, 4, ph_big, ov_big);
    for (int i = 0; i < 4; i++)
      CHECK (ov_big[i] == 1); /* every step wraps at least once */
    nco_destroy (nco_big);
  }

  /* ----------------------------------------------------------------
   * 11. Single-sample primitives == a loop over the batch steppers
   *
   * Every nco_step_u32* primitive must produce, sample-for-sample,
   * the exact same phase (and carry, where applicable) as its batch
   * counterpart -- the batch functions are defined as nothing more
   * than a loop over these, so this is really testing that the
   * refactor didn't change either side's behaviour.
   * ---------------------------------------------------------------- */
  {
    nco_state_t *batch = nco_create (0.1, 5);
    nco_state_t *single = nco_create (0.1, 5);
    uint32_t     bout[8];
    nco_steps_u32 (batch, 8, bout);
    for (int i = 0; i < 8; i++)
      CHECK (nco_step_u32 (single) == bout[i]);
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.1, 5);
    nco_state_t *single = nco_create (0.1, 5);
    uint32_t     bout[8];
    nco_steps_u32_scaled (batch, 8, bout);
    for (int i = 0; i < 8; i++)
      CHECK (nco_step_u32_scaled (single) == bout[i]);
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.37, 0);
    nco_state_t *single = nco_create (0.37, 0);
    uint32_t     bout[8];
    uint8_t      bov[8];
    nco_steps_u32_ovf (batch, 8, bout, bov);
    for (int i = 0; i < 8; i++)
      {
        uint8_t carry;
        CHECK (nco_step_u32_ovf (single, &carry) == bout[i]);
        CHECK (carry == bov[i]);
      }
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.0, 0);
    nco_state_t *single = nco_create (0.0, 0);
    float        ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.05f * (float)i;
    uint32_t bout[8];
    nco_steps_u32_ctrl (batch, ctrl, 8, bout);
    for (int i = 0; i < 8; i++)
      CHECK (nco_step_u32_ctrl (single, (double)ctrl[i]) == bout[i]);
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.0, 6);
    nco_state_t *single = nco_create (0.0, 6);
    float        ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.05f * (float)i;
    uint32_t bout[8];
    nco_steps_u32_scaled_ctrl (batch, ctrl, 8, bout);
    for (int i = 0; i < 8; i++)
      CHECK (nco_step_u32_scaled_ctrl (single, (double)ctrl[i]) == bout[i]);
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.0, 0);
    nco_state_t *single = nco_create (0.0, 0);
    float        ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.05f * (float)i;
    uint32_t bout[8];
    uint8_t  bov[8];
    nco_steps_u32_ovf_ctrl (batch, ctrl, 8, bout, bov);
    for (int i = 0; i < 8; i++)
      {
        uint8_t carry;
        CHECK (nco_step_u32_ovf_ctrl (single, (double)ctrl[i], &carry)
               == bout[i]);
        CHECK (carry == bov[i]);
      }
    nco_destroy (batch);
    nco_destroy (single);
  }

  if (_fails)
    {
      fprintf (stderr, "test_nco_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_nco_core PASSED\n");
  return 0;
}
