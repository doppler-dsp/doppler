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
 *
 * The float boundary (see nco_core.h's own header for why confining it
 * is structural rather than stylistic):
 *
 *  12. nco_phase_units — the one conversion's total contract
 *  13. nco_norm_fold_ — the fold must never hand the cast a 1.0
 *
 * Claim-tagged sections, added from an audit of nco_core.h's own prose
 * against what this file actually pins. Tagged by claim rather than
 * numbered, so they do not collide with sections landing separately:
 *
 *  C18. reset() zeroes the phase and NOTHING else — section 1 tests this
 *       on an all-zero NCO, where it cannot fail
 *  C3.  the realised increment is at most one step LOW and NEVER high,
 *       swept against a long double oracle rather than pinned at four
 *       literals
 *  C16. the carry is WRONG under a negative control — the defect
 *       nco_step_u32_ovf_ctrl's own @warning describes, recorded as
 *       current behaviour so its fix arrives as a visible change
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

/* ==================================================================
 * Independent oracle for the control port's carry/borrow semantics.
 *
 * The property under test is not "does the flag look right" but "does
 * the flag COUNT the period boundaries the composite rate actually
 * crosses". So the expected count is computed from exact real
 * arithmetic in long double, with no phase word and no fold anywhere --
 * a model that shares no code with the thing it checks. Anything that
 * mirrored the implementation's own integer steps would be tautological.
 *
 * Sign matters: a forward crossing is +1 (an extra output is due) and a
 * backward one is -1 (one fewer), so a control that slews out and back
 * must net to zero rather than accumulating |events|.
 * ================================================================== */
static long
crossings_oracle (double base, const double *ctrl, size_t n)
{
  long double acc = 0.0L;
  long        c   = 0;
  for (size_t i = 0; i < n; i++)
    {
      acc += (long double)base + (long double)ctrl[i];
      while (acc >= 1.0L)
        {
          acc -= 1.0L;
          c++;
        }
      while (acc < 0.0L)
        {
          acc += 1.0L;
          c--;
        }
    }
  return c;
}

/* Signed event count from the 32-bit control stepper. The flag is
   unsigned, so the direction comes from the composite rate -- which is
   exactly the contract a consumer implements. */
static long
events_u32 (double base, const double *ctrl, size_t n)
{
  nco_state_t *s = nco_create (base, 0);
  long         c = 0;
  for (size_t i = 0; i < n; i++)
    {
      uint8_t e;
      nco_step_u32_ovf_ctrl (s, ctrl[i], &e);
      double d = base + ctrl[i];
      if (e)
        c += (d > 0.0) ? 1 : ((d < 0.0) ? -1 : 0);
    }
  nco_destroy (s);
  return c;
}

#define SLEW_N 4096
static double slew_buf[SLEW_N];

/* Fill a control record. `shape` selects the trajectory; every one of
   them is chosen to drive the composite through a boundary that the
   sign rule has to get right. */
static void
fill_ctrl (int shape, double amp, double bias, double base)
{
  for (int i = 0; i < SLEW_N; i++)
    {
      double t = (double)i / (double)SLEW_N;
      switch (shape)
        {
        case 0: /* constant */
          slew_buf[i] = bias;
          break;
        case 1: /* symmetric slew out and back -- must net to zero */
          slew_buf[i] = bias + amp * sin (2.0 * M_PI * t);
          break;
        case 2: /* linear ramp through zero and out the far side */
          slew_buf[i] = bias + amp * (2.0 * t - 1.0);
          break;
        case 3: /* ramp that drags the composite through DC */
          slew_buf[i] = -base + amp * (2.0 * t - 1.0);
          break;
        case 4: /* alternating sign, every sample */
          slew_buf[i] = bias + ((i & 1) ? amp : -amp);
          break;
        default: /* slam: long quiet, then a hard excursion and back */
          slew_buf[i]
              = bias + ((i > SLEW_N / 2 && i < SLEW_N / 2 + 64) ? amp : 0.0);
          break;
        }
    }
}

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
    nco_steps_u32 (nco, 8, out, 8);
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
    nco_steps_u32 (nco, 4, out, 4);
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
    nco_steps_u32 (a, 16, ref, 16);
    nco_steps_u32 (b, 8, blk, 8);
    for (int i = 0; i < 8; i++)
      CHECK (blk[i] == ref[i]);
    nco_steps_u32 (b, 8, blk, 8);
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
    nco_steps_u32_scaled (nco, 5, out, 5);
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
    nco_steps_u32_ovf (nco, 8, ph, ov, 8);
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
    nco_steps_u32 (a, 5, ref, 5); /* advance, then snapshot */
    size_t sb   = nco_state_bytes (a);
    void  *blob = malloc (sb);
    nco_get_state (a, blob);
    nco_steps_u32 (a, 16, ref, 16); /* reference continuation */

    nco_state_t *b = nco_create (0.123, 0);
    CHECK (nco_set_state (b, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (nco_set_state (b, blob) == DP_ERR_INVALID);
    nco_steps_u32 (b, 16, got, 16);
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
    nco_steps_u32_ctrl (nco_ctrl, ctrl, 8, out_ctrl, 8);
    nco_steps_u32 (nco_ref, 8, out_ref, 8);

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
    nco_steps_u32_scaled_ctrl (nco_ctrl, ctrl, 8, out_ctrl, 8);
    nco_steps_u32_scaled (nco_ref, 8, out_ref, 8);

    for (int i = 0; i < 8; i++)
      CHECK (out_ctrl[i] == out_ref[i]);
    CHECK (nco_get_norm_freq (nco_ctrl) == 0.0);
    CHECK (nco_get_phase_inc (nco_ctrl) == 0u);

    /* nmax == 0 falls back to raw, identical to steps_u32_ctrl. */
    nco_state_t *nco_raw = nco_create (0.0, 0);
    uint32_t     out_raw[8], out_plain[8];
    nco_steps_u32_scaled_ctrl (nco_raw, ctrl, 8, out_raw, 8);
    nco_state_t *nco_plain = nco_create (0.0, 0);
    nco_steps_u32_ctrl (nco_plain, ctrl, 8, out_plain, 8);
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
    nco_steps_u32_ovf_ctrl (nco_ctrl, ctrl, 8, ph_ctrl, ov_ctrl, 8);
    nco_steps_u32_ovf (nco_ref, 8, ph_ref, ov_ref, 8);
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
    nco_steps_u32_ovf_ctrl (nco_big, big_ctrl, 4, ph_big, ov_big, 4);
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
    nco_state_t *batch  = nco_create (0.1, 5);
    nco_state_t *single = nco_create (0.1, 5);
    uint32_t     bout[8];
    nco_steps_u32 (batch, 8, bout, 8);
    for (int i = 0; i < 8; i++)
      CHECK (nco_step_u32 (single) == bout[i]);
    nco_destroy (batch);
    nco_destroy (single);
  }
  {
    nco_state_t *batch  = nco_create (0.1, 5);
    nco_state_t *single = nco_create (0.1, 5);
    uint32_t     bout[8];
    nco_steps_u32_scaled (batch, 8, bout, 8);
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
    nco_steps_u32_ovf (batch, 8, bout, bov, 8);
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
    CHECK (nco_steps_u32_ctrl_max_out (batch) >= 8);
    nco_steps_u32_ctrl (batch, ctrl, 8, bout, 8);
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
    CHECK (nco_steps_u32_scaled_ctrl_max_out (batch) >= 8);
    nco_steps_u32_scaled_ctrl (batch, ctrl, 8, bout, 8);
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
    CHECK (nco_steps_u32_ovf_ctrl_max_out (batch) >= 8);
    nco_steps_u32_ovf_ctrl (batch, ctrl, 8, bout, bov, 8);
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

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* Every stepper in the family clamps to the caller's capacity and
     * advances only by what it emitted. */
    nco_state_t *nco = nco_create (0.01, 0);
    nco_state_t *ref = nco_create (0.01, 0);
    uint32_t     out[16], expect[5];
    uint8_t      carry[16];
    for (int i = 0; i < 16; i++)
      out[i] = 0xDEADBEEFu;

    CHECK (nco_steps_u32 (nco, 16, out, 5) == 5);
    for (int i = 5; i < 16; i++)
      CHECK (out[i] == 0xDEADBEEFu); /* tail untouched */
    nco_steps_u32 (ref, 5, expect, 5);
    for (int i = 0; i < 5; i++)
      CHECK (out[i] == expect[i]);
    CHECK (nco_get_phase (nco) == nco_get_phase (ref));

    /* Zero capacity emits nothing and does not advance the phase. */
    uint32_t before = nco_get_phase (nco);
    CHECK (nco_steps_u32 (nco, 16, out, 0) == 0);
    CHECK (nco_get_phase (nco) == before);

    CHECK (nco_steps_u32_scaled (nco, 16, out, 4) == 4);
    CHECK (nco_steps_u32_ovf (nco, 16, out, carry, 4) == 4);

    /* Control-port forms: ctrl_len is the request, max_out the capacity. */
    const float ctrl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    CHECK (nco_steps_u32_ctrl (nco, ctrl, 8, out, 3) == 3);
    CHECK (nco_steps_u32_scaled_ctrl (nco, ctrl, 8, out, 3) == 3);
    CHECK (nco_steps_u32_ovf_ctrl (nco, ctrl, 8, out, carry, 3) == 3);

    nco_destroy (nco);
    nco_destroy (ref);
  }

  /* ----------------------------------------------------------------
   * 13. nco_norm_fold_ — the fold must never hand the cast a 1.0
   *
   * The documented contract is a truncated fraction in [0, 2^32), and
   * the stated reason for truncating rather than rounding is
   * HOST-DETERMINISM: "a bare truncating cast is bit-identical on every
   * host". The proof offered is `d < 1 makes d*2^32 strictly < 2^32`.
   *
   * That proof holds for the real fold and NOT for the floating-point
   * one. `cycles - floor (cycles)` is mathematically in [0, 1), but for
   * any cycles in [-2^-53, 0) the subtraction ROUNDS to exactly 1.0 --
   * and (uint32_t)(1.0 * 2^32) is the out-of-range float->unsigned
   * conversion (C99 6.3.1.4) the header itself warns freezes the NCO.
   * x86 yields 0 here, arm64 saturates to 2^32-1: the one property the
   * convention exists to provide is the one that breaks.
   *
   * The true fraction of a tiny negative is in (1 - 2^-53, 1), so the
   * truncated answer is 2^32-1 for every one of these -- the same value
   * -1e-16 already returns, one representable step away. A settled
   * timing loop's ctrl passes through this band routinely.
   * ---------------------------------------------------------------- */
  /* ----------------------------------------------------------------
   * 12. nco_phase_units — the one conversion's total contract
   *
   * Pinned here because every other conversion site in the library now
   * inherits it. `volatile` throughout for the constant-folding reason
   * section 13 explains.
   * ---------------------------------------------------------------- */
  {
    volatile double u;

    u = -1.0;
    CHECK (nco_phase_units (u) == 0u);
    u = -1e-300;
    CHECK (nco_phase_units (u) == 0u);
    u = 0.0;
    CHECK (nco_phase_units (u) == 0u);
    u = -0.0;
    CHECK (nco_phase_units (u) == 0u);
    u = 0.0 / 0.0;
    CHECK (nco_phase_units (u) == 0u); /* NaN, not a wrap */
    u = 1.0 / 0.0;
    CHECK (nco_phase_units (u) == 4294967295u); /* +inf  */
    u = -1.0 / 0.0;
    CHECK (nco_phase_units (u) == 0u); /* -inf  */

    /* Saturation at and above one full cycle per sample. */
    u = 4294967296.0;
    CHECK (nco_phase_units (u) == 4294967295u);
    u = 8589934592.0;
    CHECK (nco_phase_units (u) == 4294967295u);
    u = 4294967295.5;
    CHECK (nco_phase_units (u) == 4294967295u);

    /* In range: truncate toward zero, so the value is at most one step low
       and never high -- the convention the whole family relies on. */
    u = 0.9;
    CHECK (nco_phase_units (u) == 0u);
    u = 1.9;
    CHECK (nco_phase_units (u) == 1u);
    u = 2147483648.0;
    CHECK (nco_phase_units (u) == 2147483648u);
    u = 4294967294.7;
    CHECK (nco_phase_units (u) == 4294967294u);
  }

  {
    /* `volatile` is load-bearing, not decoration. With literal arguments
       the compiler CONSTANT-FOLDS the out-of-range conversion using its
       own saturating rules and the bug vanishes: at -O2 gcc folds these
       to 2^32-1 and the test passes without exercising the cast at all.
       Routing each value through a volatile forces the real runtime
       instruction, which is what a control arriving from a loop is. */
    volatile double v;

    /* Just outside the band: already correct, and the value every case
       below must agree with. */
    v = -1e-16;
    CHECK (nco_norm_freq_to_inc (v) == 4294967295u);
    v = -1e-15;
    CHECK (nco_norm_freq_to_inc (v) == 4294967295u);

    /* Inside the band, where the fold rounds up to 1.0. */
    v = -5e-17;
    CHECK (nco_norm_freq_to_inc (v) == 4294967295u);
    v = -1e-20;
    CHECK (nco_norm_freq_to_inc (v) == 4294967295u);
    v = -1e-30;
    CHECK (nco_norm_freq_to_inc (v) == 4294967295u);

    /* Exact zero is genuinely zero advance -- not the same case. */
    v = 0.0;
    CHECK (nco_norm_freq_to_inc (v) == 0u);
    v = -0.0;
    CHECK (nco_norm_freq_to_inc (v) == 0u);

    /* A control that small is a stopped NCO, not a frozen one: the
       phase must still retreat one unit per step, not stick. */
    nco_state_t *tiny = nco_create (0.0, 0);
    uint8_t      tc;
    v = -1e-20;
    nco_step_u32_ovf_ctrl (tiny, v, &tc);
    CHECK (nco_get_phase (tiny) == 4294967295u);
    nco_destroy (tiny);

    /* The two faces are ONE body. They exist to let a call site declare
       whether it holds a rate or an angle, NOT to convert differently --
       the moment they disagree, the library has two conventions again
       and the drift this file documents is back. Values chosen where
       truncate and round-to-nearest differ, so re-rounding one face
       breaks this rather than passing vacuously. */
    v = 0.1;
    CHECK (nco_norm_freq_to_inc (v) == nco_norm_phase_to_word (v));
    v = 51.0 / 21.0e6;
    CHECK (nco_norm_freq_to_inc (v) == nco_norm_phase_to_word (v));
    v = -0.3;
    CHECK (nco_norm_freq_to_inc (v) == nco_norm_phase_to_word (v));
    v = -1e-20;
    CHECK (nco_norm_freq_to_inc (v) == nco_norm_phase_to_word (v));
  }

  /* ----------------------------------------------------------------
   * C18. reset() zeroes the phase and NOTHING else.
   *
   * Section 1 exercises this on an NCO built with norm_freq = 0.0 and
   * nmax = 0, where phase, phase_inc, norm_freq and nmax are ALL zero
   * already -- so a reset() that wiped the entire configuration would
   * pass it unchanged. The claim it is meant to protect ("norm_freq,
   * phase_inc, and nmax are unchanged") is exactly the one it cannot
   * catch. Build with every field non-zero so it can fail.
   * ---------------------------------------------------------------- */
  {
    nco_state_t *nco = nco_create (0.3, 1000);
    CHECK (nco != NULL);
    if (nco)
      {
        uint32_t inc_before = nco_get_phase_inc (nco);
        uint32_t out[8];
        nco_steps_u32 (nco, 8, out, 8);

        /* Vacuity preconditions: the fields must be non-zero BEFORE the
           reset, or this section proves nothing. */
        CHECK (inc_before != 0u);
        CHECK (nco_get_phase (nco) != 0u);

        nco_reset (nco);

        CHECK (nco_get_phase (nco) == 0u);      /* zeroed ... */
        CHECK (nco_get_norm_freq (nco) == 0.3); /* ... and only that */
        CHECK (nco_get_phase_inc (nco) == inc_before);

        /* nmax has no accessor, so read it through the behaviour it
           controls. Bounding by 1000 is NOT enough: if phase_inc were
           also lost the accumulator would sit at 0 and every output
           would pass that bound vacuously. Pin the exact value instead
           -- from phase 0 at norm_freq 0.3, the second scaled sample is
           299, which is wrong if EITHER nmax or phase_inc was disturbed
           (raw phase there is ~1.29e9).

           299 and not 300, and the reason is section C3's law showing
           through: phase_inc is trunc(0.3 * 2^32) = 1288490188, so the
           realised phase fraction is 0.29999999995 -- one step LOW, as
           the conversion guarantees -- and scaling that by 1000 floors
           to 299. A rounding conversion would put 300 here, so this
           value also pins the truncating convention from a second
           direction. */
        uint32_t sc[8];
        nco_steps_u32_scaled (nco, 8, sc, 8);
        CHECK (sc[0] == 0u);
        CHECK (sc[1] == 299u);
        int scaled_in_range = 1;
        for (int i = 0; i < 8; i++)
          if (sc[i] >= 1000u)
            scaled_in_range = 0;
        CHECK (scaled_in_range);

        nco_destroy (nco);
      }
  }

  /* ----------------------------------------------------------------
   * C3. The realised increment is at most one step LOW, and NEVER high.
   *
   * Section 12 pins nco_phase_units at four literals. nco_core.h states
   * this as a LAW over the whole range -- "the realised frequency is at
   * most one step LOW, never high" -- and the whole family relies on the
   * error being one-sided. Sweep it against an independent oracle:
   * long double, which carries the exact product with no phase word and
   * no fold anywhere, so the check cannot be tautological.
   * ---------------------------------------------------------------- */
  {
    const int    N      = 2000;
    int          n_high = 0; /* realised ABOVE ideal -- must never happen */
    int          n_far  = 0; /* low by a whole step or more              */
    int          n_live = 0;
    const double lo_f = 1e-9, hi_f = 0.25;

    for (int k = 0; k < N; k++)
      {
        /* Log-spaced across the code-NCO regime up to a quarter rate.
           volatile so nothing is constant-folded away (see section 12's
           note -- the same trap applies here). */
        volatile double f
            = lo_f * pow (hi_f / lo_f, (double)k / (double)(N - 1));
        uint32_t inc = nco_norm_freq_to_inc (f);
        if (inc == 0u)
          continue;
        n_live++;

        long double ideal = (long double)f * 4294967296.0L;
        long double got   = (long double)inc;
        if (got > ideal)
          n_high++;
        if (ideal - got >= 1.0L)
          n_far++;
      }
    CHECK (n_live > N / 2); /* the sweep must actually exercise something */
    CHECK (n_high == 0);
    CHECK (n_far == 0);
  }

  /* ----------------------------------------------------------------
   * 14. The control port COUNTS boundaries, under slewing control, at
   *     both signs, against an independent oracle.
   *
   * The tolerance is the truncation floor and it is +-1 per record: an
   * exactly-cancelling +-d pair cannot round-trip through a truncating
   * accumulator, so the count sits one below the real answer
   * indefinitely. A tighter bound is not achievable at any width -- a
   * 64-bit accumulator was built for exactly this, measured, and
   * removed on the finding that it did not help.
   *
   * Restricted to |composite| < 1 by construction: the flag is one bit,
   * so it saturates when a single sample crosses more than one
   * boundary. That case is asserted separately in 15 as "always
   * events", which is its actual contract.
   * ---------------------------------------------------------------- */
  {
    static const double bases[]
        = { 0.0,  0.05, 0.1, 0.25, 0.5, 12.0 / 13.0, 0.923076923076923,
            0.75, 0.9,  0.99 };
    static const char *shape_name[]
        = { "const",           "sine-out-and-back", "ramp-through-zero",
            "ramp-through-DC", "alternating",       "slam" };

    for (size_t b = 0; b < sizeof bases / sizeof bases[0]; b++)
      {
        double base = bases[b];
        for (int shape = 0; shape < 6; shape++)
          {
            /* Amplitude kept inside the |composite| < 1 band the flag
               can represent, and deliberately taken to both signs. */
            double amp = (base < 0.5) ? base * 0.9 + 0.04 : (1.0 - base) * 0.9;
            double bias = 0.0;
            if (amp <= 0.0)
              amp = 0.01;
            fill_ctrl (shape, amp, bias, base);

            long want = crossings_oracle (base, slew_buf, SLEW_N);
            long got  = events_u32 (base, slew_buf, SLEW_N);

            if (labs (got - want) > 1)
              fprintf (stderr, "  base=%.9f shape=%-18s oracle=%ld got=%ld\n",
                       base, shape_name[shape], want, got);
            CHECK (labs (got - want) <= 1); /* the truncation floor */
          }
      }
  }

  /* ----------------------------------------------------------------
   * 15. Control-port edge cases the sign rule has to get exactly right
   * ---------------------------------------------------------------- */
  {
    /* A composite of exactly zero is free-running: no event, ever. The
       32-bit intrinsic-style "did the unsigned add carry" test cannot
       express this -- 0 + 0 never carries either, so it agrees here by
       luck; the case that separates them is unity, below. */
    {
      nco_state_t *s = nco_create (0.25, 0);
      for (int i = 0; i < 64; i++)
        {
          uint8_t e32;
          nco_step_u32_ovf_ctrl (s, -0.25, &e32);
          CHECK (e32 == 0);
        }
      /* and the phase must not have moved */
      CHECK (nco_get_phase (s) == 0u);
      nco_destroy (s);
    }

    /* A composite of exactly 1.0 completes a period every single sample
       even though the FRACTIONAL advance is zero -- the case a
       resampler's terminal stage sits on, and the one an unsigned-carry
       test gets wrong (it adds 0 + 0 and never fires). */
    {
      nco_state_t *s = nco_create (1.0, 0);
      for (int i = 0; i < 64; i++)
        {
          uint8_t e32;
          nco_step_u32_ovf_ctrl (s, 0.0, &e32);
          CHECK (e32 == 1);
        }
      nco_destroy (s);
    }

    /* |composite| >= 1 events on every sample, both signs: the flag
       saturates rather than under-reporting. */
    {
      static const double d[] = { 1.0, 1.5, 2.0, 8.0, -1.0, -1.5, -2.0, -8.0 };
      for (size_t i = 0; i < sizeof d / sizeof d[0]; i++)
        {
          nco_state_t *s = nco_create (d[i], 0);
          for (int j = 0; j < 16; j++)
            {
              uint8_t e32;
              nco_step_u32_ovf_ctrl (s, 0.0, &e32);
              CHECK (e32 == 1);
            }
          nco_destroy (s);
        }
    }

    /* The composite's sign decides, NOT the control's. Steering a
       0.5 cyc/sample carrier by -1e-4 leaves a legitimate +0.4999
       forward rate; a ctrl-keyed rule would call every crossing a
       borrow and err by -200%. */
    {
      for (int i = 0; i < SLEW_N; i++)
        slew_buf[i] = -1e-4;
      long want = crossings_oracle (0.5, slew_buf, SLEW_N);
      CHECK (want > 0); /* the record really does run forward */
      CHECK (labs (events_u32 (0.5, slew_buf, SLEW_N) - want) <= 1);
    }

    /* Its mirror: a positive control that leaves the composite running
       backward. */
    {
      for (int i = 0; i < SLEW_N; i++)
        slew_buf[i] = 0.25;
      long want = crossings_oracle (-0.5, slew_buf, SLEW_N);
      CHECK (want < 0); /* genuinely retreating */
      CHECK (labs (events_u32 (-0.5, slew_buf, SLEW_N) - want) <= 1);
    }

    /* A symmetric excursion must NET TO ZERO, not accumulate |events|:
       the borrows on the way back have to cancel the carries out. */
    {
      fill_ctrl (1, 0.4, 0.0, 0.0); /* base 0, pure +-0.4 sine */
      long want = crossings_oracle (0.0, slew_buf, SLEW_N);
      CHECK (want == 0); /* the oracle agrees it is a closed excursion */
      CHECK (labs (events_u32 (0.0, slew_buf, SLEW_N)) <= 1);
    }
  }

  /* ----------------------------------------------------------------
   * 16. nco_steer_scale — bound the request, so the conversion never
   *     has to be the one making the decision.
   * ---------------------------------------------------------------- */
  {
    volatile double c;
    const double    lo = 2.0 / 3.0, hi = 2.0;

    /* Inside the band, it is exactly 1 + control. */
    c = 0.0;
    CHECK (nco_steer_scale (c, lo, hi) == 1.0);
    c = 0.5;
    CHECK (nco_steer_scale (c, lo, hi) == 1.5);
    c = -0.25;
    CHECK (nco_steer_scale (c, lo, hi) == 0.75);

    /* Outside, clamped to the band -- both ends. */
    c = 5.0;
    CHECK (nco_steer_scale (c, lo, hi) == hi);
    c = -0.9;
    CHECK (nco_steer_scale (c, lo, hi) == lo);

    /* The case that motivated the whole thing: a control below -1 makes
       the raw scale NEGATIVE, which floors an honest conversion to 0 --
       a stopped NCO that never strobes again. It must land on lo, which
       is a slow clock, not a dead one. */
    c = -1.0;
    CHECK (nco_steer_scale (c, lo, hi) == lo);
    c = -2.0;
    CHECK (nco_steer_scale (c, lo, hi) == lo);
    c = -1e9;
    CHECK (nco_steer_scale (c, lo, hi) == lo);
    CHECK (nco_steer_scale (c, lo, hi) > 0.0); /* never a dead clock */

    /* NaN lands on lo rather than sailing through to the cast. */
    c = 0.0 / 0.0;
    CHECK (nco_steer_scale (c, lo, hi) == lo);

    /* Composed with the conversion, the product can neither floor nor
       saturate for any control at all -- which is the whole point: the
       conversion becomes a safety net, not the active path. */
    for (int i = -400; i <= 400; i += 7)
      {
        volatile double ctl  = (double)i * 0.05;
        double          sc   = nco_steer_scale (ctl, lo, hi);
        uint32_t        base = 1073741824u; /* sps = 4 -> 2^32/4 */
        uint32_t        inc  = nco_phase_units ((double)base * sc);
        CHECK (inc > 0u);          /* never stopped   */
        CHECK (inc < 4294967295u); /* never saturated */
      }
  }

  /* ----------------------------------------------------------------
   * 17. max_out is the bound; NCO_MAX_OUT is a pre-allocation hint.
   *
   * nco_core.h said "requesting more samples per call is undefined
   * behaviour" and nco_core.c said a request past 65536 "overflows the
   * buffer". Both described the contract from before pass_capacity
   * (jm gh-138) handed the kernel the caller's capacity, and both were
   * still there long after it stopped being true -- the identical pair
   * of sentences sat in lo_core.{h,c} too, which is how this was found:
   * fixing one copy and not its sibling is the drift the no-duplicate
   * rule exists to prevent.
   *
   * Pinned on all three output mappings, since each has its own kernel
   * and could regain a private ceiling independently.
   * ---------------------------------------------------------------- */
  {
    CHECK (nco_steps_u32_max_out (NULL) == 65536u);
    CHECK (nco_steps_u32_scaled_max_out (NULL) == 65536u);
    CHECK (nco_steps_u32_ovf_max_out (NULL) == 65536u);

    const size_t BIG  = 70000;
    uint32_t    *big  = malloc (BIG * sizeof *big);
    uint8_t     *flag = malloc (BIG * sizeof *flag);
    CHECK (big != NULL && flag != NULL);
    if (big && flag)
      {
        nco_state_t *raw = nco_create (0.013, 0);
        CHECK (nco_steps_u32 (raw, BIG, big, BIG) == BIG);
        /* every sample really was written: the accumulator is exactly
         * predictable, so the last one proves the whole run */
        uint32_t inc = nco_get_phase_inc (raw);
        CHECK (big[BIG - 1] == (uint32_t)((uint64_t)inc * (BIG - 1)));
        CHECK (nco_get_phase (raw) == (uint32_t)((uint64_t)inc * BIG));
        nco_destroy (raw);

        nco_state_t *sc = nco_create (0.013, 1000);
        CHECK (nco_steps_u32_scaled (sc, BIG, big, BIG) == BIG);
        CHECK (big[BIG - 1] < 1000u);
        nco_destroy (sc);

        nco_state_t *ov = nco_create (0.013, 0);
        CHECK (nco_steps_u32_ovf (ov, BIG, big, flag, BIG) == BIG);
        nco_destroy (ov);

        free (big);
        free (flag);
      }
  }

  if (_fails)
    {
      fprintf (stderr, "test_nco_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_nco_core PASSED\n");
  return 0;
}
