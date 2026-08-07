/**
 * @file test_nco_core.c
 * @brief Unit tests for the NCO pure phase-accumulator.
 *
 * Tests:
 *   1. Lifecycle — create / reset / destroy
 *   2. Zero freq — phase_inc = 0, all outputs are 0
 *   3. Quarter-rate — phase_inc = 0x40000000, 4-sample sequence
 *   4. Phase continuity — two consecutive blocks share state
 *   5. nmax scaling — steps_u32_scaled maps [0, 2^32) -> [0, nmax)
 *   6. Overflow flag — carry fires exactly once per full cycle
 *   7. Property accessors — get/set norm_freq, phase, phase_inc
 *      (+ serializable state, and pass_capacity/max_out, unnumbered)
 *   8. ctrl-port FM shift — steps_u32_ctrl deviates phase per sample
 *      without touching phase_inc/norm_freq (mirrors lo_steps_ctrl)
 *   9. steps_u32_scaled_ctrl — nmax scaling + ctrl port combined
 *  10. steps_u32_ovf_ctrl — carry detection + ctrl port combined,
 *      including a ctrl large enough to force >1 wrap in one sample
 *  11. Single-sample primitives (nco_step_u32*) — every batch stepper
 *      is exactly a loop over its single-sample counterpart
 *
 * The float boundary and the timing clock (see nco_core.h's own header
 * for why these are one file's worth of concern):
 *
 *  12. nco_phase_units — the one conversion's total contract
 *  13. nco_norm_to_inc — the fold must never hand the cast a 1.0
 *  14. The control port COUNTS boundaries, at both widths and both
 *      signs, under slewing control, against an independent oracle
 *  15. Control-port edge cases the sign rule has to get exactly right
 *  16. nco_clock_units / nco_clock_norm_to_inc — the 64-bit twin of 12
 *  17. What the 64-bit word actually buys: PHASE error, not count
 *  18. nco_steer_scale — bound the request, so the conversion never
 *      has to be the one making the decision
 *
 * Several of these use `volatile` inputs deliberately: with literal
 * arguments the compiler CONSTANT-FOLDS an out-of-range float->integer
 * conversion using its own saturating rules, and the check passes at -O2
 * without the cast ever executing.
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

/* The same, for the 64-bit timing clock. */
static long
events_clock (double base, const double *ctrl, size_t n)
{
  nco_clock_t k;
  nco_clock_init (&k, base);
  long c = 0;
  for (size_t i = 0; i < n; i++)
    {
      uint8_t e;
      nco_clock_tick (&k, ctrl[i], &e);
      double d = base + ctrl[i];
      if (e)
        c += (d > 0.0) ? 1 : ((d < 0.0) ? -1 : 0);
    }
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
   * steps_u32_ovf at norm_freq=0.25 (phase AND carry), and its mirror
   * image -- a constant ctrl of -0.25 at norm_freq=0.5 -- must match
   * the SAME reference, again in phase AND carry. Separately, a ctrl
   * large enough that phase_inc + ctrl_inc alone would overflow a
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

    /* The negative-control mirror of the case above: norm_freq=0.5
       steered by a constant ctrl of -0.25 is the same 0.25 cyc/sample
       composite, so it must match the SAME plain-NCO reference in
       carry as well as phase.

       nco_norm_to_inc folds bipolar to unipolar (-0.25 -> 3x2^30), so
       the modulo phase is exact either way -- but the sign is gone
       before the add, and 2^31 + 3x2^30 sets bit 32 on EVERY step.
       A carry consumer therefore sees 8 wraps where the composite
       advance produces 2. The carry must follow the sign of the
       composite (base + ctrl, formed BEFORE the fold), not the bare
       64-bit sum. */
    nco_state_t *nco_neg    = nco_create (0.5, 0);
    nco_state_t *nco_negref = nco_create (0.25, 0);
    float        neg_ctrl[8];
    for (int i = 0; i < 8; i++)
      neg_ctrl[i] = -0.25f;
    uint32_t ph_neg[8], ph_negref[8];
    uint8_t  ov_neg[8], ov_negref[8];
    nco_steps_u32_ovf_ctrl (nco_neg, neg_ctrl, 8, ph_neg, ov_neg, 8);
    nco_steps_u32_ovf (nco_negref, 8, ph_negref, ov_negref, 8);
    for (int i = 0; i < 8; i++)
      {
        CHECK (ph_neg[i] == ph_negref[i]);
        CHECK (ov_neg[i] == ov_negref[i]);
      }
    nco_destroy (nco_neg);
    nco_destroy (nco_negref);

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
   * 13. nco_norm_to_inc — the fold must never hand the cast a 1.0
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
    CHECK (nco_norm_to_inc (v) == 4294967295u);
    v = -1e-15;
    CHECK (nco_norm_to_inc (v) == 4294967295u);

    /* Inside the band, where the fold rounds up to 1.0. */
    v = -5e-17;
    CHECK (nco_norm_to_inc (v) == 4294967295u);
    v = -1e-20;
    CHECK (nco_norm_to_inc (v) == 4294967295u);
    v = -1e-30;
    CHECK (nco_norm_to_inc (v) == 4294967295u);

    /* Exact zero is genuinely zero advance -- not the same case. */
    v = 0.0;
    CHECK (nco_norm_to_inc (v) == 0u);
    v = -0.0;
    CHECK (nco_norm_to_inc (v) == 0u);

    /* A control that small is a stopped NCO, not a frozen one: the
       phase must still retreat one unit per step, not stick. */
    nco_state_t *tiny = nco_create (0.0, 0);
    uint8_t      tc;
    v = -1e-20;
    nco_step_u32_ovf_ctrl (tiny, v, &tc);
    CHECK (nco_get_phase (tiny) == 4294967295u);
    nco_destroy (tiny);
  }

  /* ----------------------------------------------------------------
   * 14. The control port counts boundaries, at both widths and both
   *     signs, under slewing control.
   *
   * The NCO is the foundation every rate-bearing object in the library
   * stands on, so this is the exhaustive pass: a matrix of base rates
   * crossed with control trajectories, each checked against the exact
   * real-arithmetic oracle above rather than against a remembered
   * answer.
   *
   * Two families are covered together because they must agree on
   * semantics and differ only in resolution: the 32-bit
   * nco_step_u32_ovf_ctrl and the 64-bit nco_clock_tick.
   *
   * TOLERANCE IS NOT SLOP, it is the truncation floor, and BOTH widths
   * have one. Both fold-and-truncate, so the realised rate is at most
   * one phase step low per sample and the record's accumulated deficit
   * can defer exactly one crossing past the end. A first draft of this
   * test demanded the 64-bit clock be EXACT and it failed on three
   * cells -- correctly: an exactly-cancelling +-d pair cannot round
   * trip through a truncating accumulator at any width, so the count
   * sits one below the real answer indefinitely. +-1 per record is the
   * honest bound for both.
   *
   * Which means a count over a short record does NOT demonstrate what
   * the 64-bit word buys. Section 17 measures that directly, as phase
   * error, where the two differ by 2^32.
   *
   * Restricted to |composite| < 1 by construction: the flag is one bit,
   * so it saturates when a single sample crosses more than one
   * boundary. That case is asserted separately below as "always
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
            long g32  = events_u32 (base, slew_buf, SLEW_N);
            long g64  = events_clock (base, slew_buf, SLEW_N);

            if (labs (g32 - want) > 1 || labs (g64 - want) > 1)
              fprintf (
                  stderr,
                  "  base=%.9f shape=%-18s oracle=%ld u32=%ld clock=%ld\n",
                  base, shape_name[shape], want, g32, g64);
            CHECK (labs (g32 - want) <= 1); /* truncation floor, both  */
            CHECK (labs (g64 - want) <= 1); /* widths; see section 17   */
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
      nco_clock_t  k;
      nco_clock_init (&k, 0.25);
      for (int i = 0; i < 64; i++)
        {
          uint8_t e32, e64;
          nco_step_u32_ovf_ctrl (s, -0.25, &e32);
          nco_clock_tick (&k, -0.25, &e64);
          CHECK (e32 == 0);
          CHECK (e64 == 0);
        }
      /* and the phase must not have moved */
      CHECK (nco_get_phase (s) == 0u);
      CHECK (k.phase == 0u);
      nco_destroy (s);
    }

    /* A composite of exactly 1.0 completes a period every single sample
       even though the FRACTIONAL advance is zero -- the case a
       resampler's terminal stage sits on, and the one an unsigned-carry
       test gets wrong (it adds 0 + 0 and never fires). */
    {
      nco_state_t *s = nco_create (1.0, 0);
      nco_clock_t  k;
      nco_clock_init (&k, 1.0);
      for (int i = 0; i < 64; i++)
        {
          uint8_t e32, e64;
          nco_step_u32_ovf_ctrl (s, 0.0, &e32);
          nco_clock_tick (&k, 0.0, &e64);
          CHECK (e32 == 1);
          CHECK (e64 == 1);
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
          nco_clock_t  k;
          nco_clock_init (&k, d[i]);
          for (int j = 0; j < 16; j++)
            {
              uint8_t e32, e64;
              nco_step_u32_ovf_ctrl (s, 0.0, &e32);
              nco_clock_tick (&k, 0.0, &e64);
              CHECK (e32 == 1);
              CHECK (e64 == 1);
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
      CHECK (events_clock (0.5, slew_buf, SLEW_N) == want);
    }

    /* Its mirror: a positive control that leaves the composite running
       backward. */
    {
      for (int i = 0; i < SLEW_N; i++)
        slew_buf[i] = 0.25;
      long want = crossings_oracle (-0.5, slew_buf, SLEW_N);
      CHECK (want < 0); /* genuinely retreating */
      CHECK (labs (events_u32 (-0.5, slew_buf, SLEW_N) - want) <= 1);
      CHECK (events_clock (-0.5, slew_buf, SLEW_N) == want);
    }

    /* A symmetric excursion must NET TO ZERO, not accumulate |events|:
       the borrows on the way back have to cancel the carries out. */
    {
      fill_ctrl (1, 0.4, 0.0, 0.0); /* base 0, pure +-0.4 sine */
      long want = crossings_oracle (0.0, slew_buf, SLEW_N);
      CHECK (want == 0); /* the oracle agrees it is a closed excursion */
      CHECK (labs (events_u32 (0.0, slew_buf, SLEW_N)) <= 1);
      CHECK (labs (events_clock (0.0, slew_buf, SLEW_N)) <= 1);
    }
  }

  /* ----------------------------------------------------------------
   * 16. nco_clock_units / nco_clock_norm_to_inc — the 64-bit twin of
   *     the conversion contract pinned in section 12.
   * ---------------------------------------------------------------- */
  {
    volatile double u; /* defeats constant folding; see section 12 */

    u = -1.0;
    CHECK (nco_clock_units (u) == 0u);
    u = 0.0;
    CHECK (nco_clock_units (u) == 0u);
    u = -0.0;
    CHECK (nco_clock_units (u) == 0u);
    u = 0.0 / 0.0;
    CHECK (nco_clock_units (u) == 0u); /* NaN   */
    u = -1.0 / 0.0;
    CHECK (nco_clock_units (u) == 0u); /* -inf  */
    u = 1.0 / 0.0;
    CHECK (nco_clock_units (u) == 18446744073709551615u);
    u = 18446744073709551616.0; /* 2^64 */
    CHECK (nco_clock_units (u) == 18446744073709551615u);
    u = 36893488147419103232.0; /* 2^65 */
    CHECK (nco_clock_units (u) == 18446744073709551615u);
    u = 9223372036854775808.0; /* 2^63 */
    CHECK (nco_clock_units (u) == 9223372036854775808u);

    /* The fold's 1.0 case, which is what the guard exists for. */
    volatile double c;
    c = -1e-20;
    CHECK (nco_clock_norm_to_inc (c) == 18446744073709551615u);
    c = -5e-17;
    CHECK (nco_clock_norm_to_inc (c) == 18446744073709551615u);
    c = 0.0;
    CHECK (nco_clock_norm_to_inc (c) == 0u);
    c = 1.0;
    CHECK (nco_clock_norm_to_inc (c) == 0u); /* frac(1) == 0 */
    c = 0.5;
    CHECK (nco_clock_norm_to_inc (c) == 9223372036854775808u);
    c = -0.5;
    CHECK (nco_clock_norm_to_inc (c) == 9223372036854775808u);
    c = 0.25;
    CHECK (nco_clock_norm_to_inc (c) == 4611686018427387904u);
    c = -0.25;
    CHECK (nco_clock_norm_to_inc (c) == 13835058055282163712u);

    /* Monotone in the fraction, and never above the true value. */
    for (int i = 1; i < 64; i++)
      {
        double   f0 = (double)(i - 1) / 64.0, f1 = (double)i / 64.0;
        uint64_t a = nco_clock_norm_to_inc (f0),
                 b = nco_clock_norm_to_inc (f1);
        CHECK (a < b);
        CHECK ((long double)b <= (long double)f1 * 18446744073709551616.0L);
      }

    /* nco_clock_frac: top bits, total at bits == 0. */
    nco_clock_t k;
    nco_clock_init (&k, 0.0);
    k.phase = 18446744073709551615u; /* just under a full period */
    CHECK (nco_clock_frac (&k, 10) == 1023u);
    CHECK (nco_clock_frac (&k, 1) == 1u);
    CHECK (nco_clock_frac (&k, 0) == 0u);
    k.phase = 0u;
    CHECK (nco_clock_frac (&k, 10) == 0u);

    /* nco_clock_advance: reports the wrap, and only the wrap. */
    k.phase         = 0u;
    uint8_t wrapped = nco_clock_advance (&k, 9223372036854775808u); /* +1/2 */
    CHECK (wrapped == 0 && k.phase == 9223372036854775808u);
    wrapped = nco_clock_advance (&k, 9223372036854775808u);
    CHECK (wrapped == 1 && k.phase == 0u);
    wrapped = nco_clock_advance (&k, 0u);
    CHECK (wrapped == 0 && k.phase == 0u);
  }

  /* ----------------------------------------------------------------
   * 17. What the 64-bit word actually buys: PHASE error, not count
   *
   * A crossing count over a short record cannot separate the widths --
   * both sit within one of the truth (section 14). The property that
   * does separate them is the realised rate, and it shows up as the
   * phase error accumulated over a long constant-rate run:
   *
   *     |phase(N) - frac(N * rate)|  ~  N * 2^-W
   *
   * because truncation biases each step low by up to one word step.
   * That is the whole argument for the timing clock: a resampler is
   * handed an exactly rational rate like m/sps, which puts the wrap
   * precisely ON the boundary every period, so a phase deficit does not
   * average out -- it defers a strobe and permanently shifts the
   * parity. This asserts the two floors are what they should be, and it
   * is the regression that fires if anyone narrows the clock back to 32
   * bits.
   * ---------------------------------------------------------------- */
  {
    static const double rates[]
        = { 12.0 / 13.0, 0.1, 0.3333333333333333, 0.7, 0.9999 };
    for (size_t r = 0; r < sizeof rates / sizeof rates[0]; r++)
      {
        const long N    = 100000;
        double     rate = rates[r];

        nco_state_t *s32 = nco_create (rate, 0);
        nco_clock_t  k64;
        nco_clock_init (&k64, rate);
        for (long i = 0; i < N; i++)
          {
            uint8_t e;
            nco_step_u32_ovf_ctrl (s32, 0.0, &e);
            nco_clock_tick (&k64, 0.0, &e);
          }

        /* Exact expected fraction, computed without either accumulator. */
        long double exact = (long double)rate * (long double)N;
        exact -= floorl (exact);

        long double got32 = (long double)nco_get_phase (s32) / 4294967296.0L;
        long double got64 = (long double)k64.phase / 18446744073709551616.0L;
        long double e32   = fabsl (got32 - exact);
        long double e64   = fabsl (got64 - exact);
        if (e32 > 0.5L)
          e32 = 1.0L - e32; /* circular */
        if (e64 > 0.5L)
          e64 = 1.0L - e64;

        /* The 32-bit floor is real and about N*2^-32 = 2.3e-5 here. */
        CHECK (e32 < 1e-3L);
        /* The 64-bit floor is about N*2^-64 = 5e-15; demand four orders
           of magnitude better than the 32-bit word can ever manage. */
        CHECK (e64 < 1e-9L);
        if (!(e64 < 1e-9L) || !(e32 < 1e-3L))
          fprintf (stderr, "  rate=%.17g  phase err: u32=%.3Le  clock=%.3Le\n",
                   rate, e32, e64);
      }
  }

  /* ----------------------------------------------------------------
   * 18. nco_steer_scale — bound the request, so the conversion never
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

  if (_fails)
    {
      fprintf (stderr, "test_nco_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_nco_core PASSED\n");
  return 0;
}
