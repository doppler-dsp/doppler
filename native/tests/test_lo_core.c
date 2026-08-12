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
#include "dp_state_test.h"
#include "dp_test.h"
#include "lo/lo_core.h"
#include <math.h>
#include <stdio.h>

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

  /* ----------------------------------------------------------------
   * 1. Lifecycle
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.0);
    DP_CHECK (lo != NULL);
    if (!lo)
      return 1;
    lo_reset (lo);
    DP_CHECK (lo_get_phase (lo) == 0u);
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 2. DC tone — norm_freq = 0 → phase_inc = 0 → all 1 + 0j
   * ---------------------------------------------------------------- */
  {
    lo_state_t   *lo = lo_create (0.0);
    float complex out[8];
    lo_steps (lo, 8, out, 8);
    for (int i = 0; i < 8; i++)
      DP_CHECK (near_c (out[i], 1.0f + 0.0f * I));
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
    lo_state_t   *lo = lo_create (0.25);
    float complex out[8];
    lo_steps (lo, 8, out, 8);
    DP_CHECK (near_c (out[0], 1.0f + 0.0f * I));
    DP_CHECK (near_c (out[1], 0.0f + 1.0f * I));
    DP_CHECK (near_c (out[2], -1.0f + 0.0f * I));
    DP_CHECK (near_c (out[3], 0.0f - 1.0f * I));
    DP_CHECK (near_c (out[4], 1.0f + 0.0f * I));
    DP_CHECK (near_c (out[5], 0.0f + 1.0f * I));
    DP_CHECK (near_c (out[6], -1.0f + 0.0f * I));
    DP_CHECK (near_c (out[7], 0.0f - 1.0f * I));
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 4. Phase continuity across two blocks
   *
   * Two consecutive calls of length N must match a single call of 2N.
   * ---------------------------------------------------------------- */
  {
    lo_state_t   *a = lo_create (0.1);
    lo_state_t   *b = lo_create (0.1);
    float complex ref[16], blk[8];
    lo_steps (a, 16, ref, 16);
    lo_steps (b, 8, blk, 8);
    for (int i = 0; i < 8; i++)
      DP_CHECK (near_c (blk[i], ref[i]));
    lo_steps (b, 8, blk, 8);
    for (int i = 0; i < 8; i++)
      DP_CHECK (near_c (blk[i], ref[8 + i]));
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
    lo_state_t *lo_ref  = lo_create (0.25);

    double ctrl[8];
    for (int i = 0; i < 8; i++)
      ctrl[i] = 0.25f;

    float complex out_ctrl[8], out_ref[8];
    lo_steps_ctrl (lo_ctrl, ctrl, 8, out_ctrl, 8);
    lo_steps (lo_ref, 8, out_ref, 8);

    for (int i = 0; i < 8; i++)
      DP_CHECK (near_c (out_ctrl[i], out_ref[i]));

    /* Base norm_freq is unchanged after steps_ctrl. */
    DP_CHECK (lo_get_norm_freq (lo_ctrl) == 0.0);

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
    lo_state_t   *lo = lo_create (0.25);
    float complex out[4];
    lo_steps (lo, 4, out, 4);
    for (int k = 0; k < 4; k++)
      {
        float mag2 = crealf (out[k]) * crealf (out[k])
                     + cimagf (out[k]) * cimagf (out[k]);
        DP_CHECK (near (mag2, 1.0f));
      }
    /* out[1] should equal j * out[0] at quarter-rate */
    float complex expected1 = I * out[0];
    DP_CHECK (near_c (out[1], expected1));
    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 7. Property accessors
   * ---------------------------------------------------------------- */
  {
    lo_state_t *lo = lo_create (0.25);
    DP_CHECK (lo_get_norm_freq (lo) == 0.25);
    DP_CHECK (lo_get_phase (lo) == 0u);
    DP_CHECK (lo_get_phase_inc (lo) == 0x40000000u);

    lo_set_phase (lo, 0x80000000u);
    DP_CHECK (lo_get_phase (lo) == 0x80000000u);

    lo_set_norm_freq (lo, 0.5);
    DP_CHECK (lo_get_phase_inc (lo) == 0x80000000u);
    DP_CHECK (lo_get_phase (lo) == 0x80000000u); /* unchanged */

    lo_reset (lo);
    DP_CHECK (lo_get_phase (lo) == 0u);
    DP_CHECK (lo_get_phase_inc (lo) == 0x80000000u); /* unchanged */

    lo_destroy (lo);
  }

  /* ----------------------------------------------------------------
   * 8. lo_step (inline single-sample) == lo_steps (block), bit-exact
   *
   * The inline composition step must reproduce the block generator
   * sample-for-sample (same LUT, same phase advance) — not merely
   * "near", but bit-identical.
   * ---------------------------------------------------------------- */
  {
    const size_t N   = 257; /* not a multiple of any SIMD width */
    lo_state_t  *blk = lo_create (0.123456);
    lo_state_t   stp;
    lo_init (&stp, 0.123456);

    float complex ref[257], got[257];
    lo_steps (blk, N, ref, N);
    for (size_t i = 0; i < N; i++)
      got[i] = lo_step (&stp);

    int exact = 1;
    for (size_t i = 0; i < N; i++)
      if (crealf (got[i]) != crealf (ref[i])
          || cimagf (got[i]) != cimagf (ref[i]))
        exact = 0;
    DP_CHECK (exact); /* bit-exact, not just near */
    /* phase accumulators must also have advanced identically */
    DP_CHECK (lo_get_phase (&stp) == lo_get_phase (blk));
    lo_destroy (blk);
  }

  /* ----------------------------------------------------------------
   * 9. lo_init (in place) == lo_create (heap), field- and output-exact
   * ---------------------------------------------------------------- */
  {
    lo_state_t *heap = lo_create (0.3);
    lo_state_t  byval;
    lo_init (&byval, 0.3);
    DP_CHECK (byval.phase == heap->phase);
    DP_CHECK (byval.phase_inc == heap->phase_inc);
    DP_CHECK (byval.norm_freq == heap->norm_freq);

    float complex a[64], b[64];
    lo_steps (heap, 64, a, 64);
    for (int i = 0; i < 64; i++)
      b[i] = lo_step (&byval);
    int exact = 1;
    for (int i = 0; i < 64; i++)
      if (crealf (a[i]) != crealf (b[i]) || cimagf (a[i]) != cimagf (b[i]))
        exact = 0;
    DP_CHECK (exact);
    lo_destroy (heap);
  }

  /* ----------------------------------------------------------------
   * 10. Long-run integer-NCO stability — the headline guarantee.
   *
   * Stream tens of millions of samples through the inline step.  The
   * uint32 accumulator wraps at 2^32 by construction, so — unlike a
   * double-precision phase accumulator feeding cexpf — there is NO
   * unbounded drift: the output stays unit-magnitude and finite for
   * the whole run, and the phase visits the full [0, 2^32) range.
   * ---------------------------------------------------------------- */
  {
    lo_state_t s;
    lo_init (&s, 0.10000000017); /* odd inc → coprime with 2^32       */
    const long     RUN     = 30000000L;
    const uint32_t inc     = lo_get_phase_inc (&s);
    int            bad_mag = 0, bad_nan = 0;
    long           wraps = 0;
    for (long k = 0; k < RUN; k++)
      {
        uint32_t      prev = s.phase;
        float complex c    = lo_step (&s);
        float         re = crealf (c), im = cimagf (c);
        float         m2 = re * re + im * im;
        if (!(m2 > 0.98f && m2 < 1.02f)) /* unit magnitude, bounded */
          bad_mag++;
        if (re != re || im != im) /* NaN check */
          bad_nan++;
        if (s.phase < prev) /* accumulator overflowed (clean wrap) */
          wraps++;
      }
    DP_CHECK (bad_mag == 0); /* never drifts off the unit circle       */
    DP_CHECK (bad_nan == 0); /* never produces NaN over 30M samples     */
    /* The integer accumulator is EXACTLY predictable after N steps —
     * the property a double-phase accumulator loses to rounding.    */
    uint32_t expected = (uint32_t)((uint64_t)inc * (uint64_t)RUN);
    DP_CHECK (s.phase == expected);
    /* and it wrapped ~RUN*norm_freq times with no stall/drift */
    DP_CHECK (wraps > 2900000L && wraps < 3100000L);
  }

  /* ----------------------------------------------------------------
   * 11. Frequency-command path — the loop's actuators.
   *
   * A tracking loop drives the NCO by writing the increment
   * (lo_set_norm_freq) and nudging the phase (lo_set_phase).  Changing
   * the frequency must take effect on the NEXT step with NO phase
   * discontinuity (the accumulator is untouched); a phase nudge must
   * apply an exact integer delta.
   * ---------------------------------------------------------------- */
  {
    lo_state_t s;
    lo_init (&s, 0.1);
    for (int i = 0; i < 5; i++)
      (void)lo_step (&s);
    uint32_t ph_before = lo_get_phase (&s);

    lo_set_norm_freq (&s, 0.2);                      /* retune */
    DP_CHECK (lo_get_phase (&s) == ph_before);       /* no jump      */
    DP_CHECK (lo_get_phase_inc (&s) == 0x33333333u); /* 0.2 * 2^32   */
    (void)lo_step (&s);
    DP_CHECK (lo_get_phase (&s) == ph_before + 0x33333333u); /* new inc  */

    /* exact integer phase nudge (proportional term of a loop) */
    uint32_t ph2 = lo_get_phase (&s);
    lo_set_phase (&s, ph2 + 0x10000000u);
    DP_CHECK (lo_get_phase (&s) == ph2 + 0x10000000u);
  }

  /* ----------------------------------------------------------------
   * 12. Edge frequencies — only the fractional part matters.
   *
   * Negative and >1 norm_freq must fold to the same phase_inc as their
   * fractional part, and lo_step must match lo_steps for them too.
   * ---------------------------------------------------------------- */
  {
    /* -0.25 folds to 0.75 → inc = 3 * 2^30 = 0xC0000000 */
    lo_state_t neg;
    lo_init (&neg, -0.25);
    DP_CHECK (lo_get_phase_inc (&neg) == 0xC0000000u);
    /* 1.25 folds to 0.25 → inc = 0x40000000, same as 0.25 */
    lo_state_t big;
    lo_init (&big, 1.25);
    DP_CHECK (lo_get_phase_inc (&big) == 0x40000000u);

    lo_state_t   *ref = lo_create (-0.25);
    float complex a[16], b[16];
    lo_steps (ref, 16, a, 16);
    for (int i = 0; i < 16; i++)
      b[i] = lo_step (&neg);
    int exact = 1;
    for (int i = 0; i < 16; i++)
      if (crealf (a[i]) != crealf (b[i]) || cimagf (a[i]) != cimagf (b[i]))
        exact = 0;
    DP_CHECK (exact);
    lo_destroy (ref);
  }

  /* ----------------------------------------------------------------
   * 13. Serializable state round-trip — the elastic-resume guarantee.
   *
   * Serialize the LO's state mid-stream, restore it into a FRESH LO
   * (same norm_freq, rebuilt from the descriptor), and continue: the
   * resumed output must equal an uninterrupted run bit-for-bit.
   * ---------------------------------------------------------------- */
  {
    const size_t  N = 100, M = 156;
    lo_state_t   *ref = lo_create (0.123456);
    float complex full[256];
    lo_steps (ref, N + M, full, N + M);
    lo_destroy (ref);

    lo_state_t   *a = lo_create (0.123456);
    float complex tmp[256];
    lo_steps (a, N, tmp, N); /* advance N */

    DP_CHECK (lo_state_bytes (a)
              == sizeof (dp_state_hdr_t) + sizeof (uint32_t));
    unsigned char blob[32];
    lo_get_state (a, blob);
    lo_destroy (a);

    lo_state_t *b = lo_create (0.123456); /* fresh, from the descriptor */
    DP_CHECK (lo_set_state (b, blob) == DP_OK);
    float complex resumed[256];
    lo_steps (b, M, resumed, M);
    lo_destroy (b);

    int exact = 1;
    for (size_t i = 0; i < M; i++)
      if (crealf (resumed[i]) != crealf (full[N + i])
          || cimagf (resumed[i]) != cimagf (full[N + i]))
        exact = 0;
    DP_CHECK (exact); /* bit-exact resume from the serialized state */
  }

  /* 14. Standard envelope round-trip + reject (the shared bytes-interface
   * gate): get_state -> set_state succeeds; a magic-clobbered blob rejects. */
  {
    lo_state_t *a = lo_create (0.2);
    lo_state_t *b = lo_create (0.2);
    lo_steps (a, 37, (float complex[37]){ 0 }, 37);
    DP_STATE_ROUNDTRIP_TEST (lo, a, b);
    lo_destroy (a);
    lo_destroy (b);
  }

  /* ----------------------------------------------------------------
   * 15. Frequency-quantization bound — round-to-nearest, not truncation.
   *
   * A 32-bit phase word can only represent frequency in fs/2^32 steps —
   * a one-time, unavoidable quantization no fixed-width accumulator can
   * be exact past (this is not a bug; it's what makes the accumulator
   * itself exact and drift-free for any run length, see test 10). What
   * IS a choice is truncating vs. rounding to the nearest representable
   * step: truncating always rounds toward zero, so its worst case is a
   * FULL step; rounding halves that bound. This test is closed-form and
   * needs no simulated waveform: it checks the exact integer phase_inc
   * norm_to_inc() produces, and bounds the resulting frequency error
   * directly from that integer -- not by comparing against a floating-
   * point reference over a long run (a double loses precision at large
   * sample counts too; the exact 32-bit modular accumulator, proven
   * exact in test 10, is the only long-run guarantee this design makes).
   *
   * fs=21e6 (this project's dsss_receiver_stress wfmgen generation rate)
   * at freq=51 Hz is a case where truncation and rounding actually
   * differ (freq=50 Hz does not: its exact phase_inc has a fractional
   * remainder < 0.5, so truncating and rounding coincide there) --
   * exact values below computed independently in Python.
   * ---------------------------------------------------------------- */
  {
    const double fs      = 21.0e6;
    const double step_hz = fs / 4294967296.0; /* one quantization step */

    /* nco_norm_freq_to_inc truncates toward zero (the natural C99
     * float->unsigned conversion), NOT round-to-nearest: the increment is then
     * deterministic across hosts (llround's rounding is host-FP-sensitive and
     * could overshoot to 2^32==0, freezing the closed-loop code NCO on arm64
     * -- see nco_norm_freq_to_inc). Truncation always floors, so the realised
     * frequency is at most one quantization step LOW, never high. */

    /* freq=50 Hz: fractional remainder ~0.11, truncates to 10226. */
    lo_state_t *lo50 = lo_create (50.0 / fs);
    DP_CHECK (lo_get_phase_inc (lo50) == 10226u);
    double actual50 = (double)lo_get_phase_inc (lo50) / 4294967296.0 * fs;
    DP_CHECK (actual50 <= 50.0 + 1e-9); /* truncation never overshoots */
    DP_CHECK (fabs (actual50 - 50.0) <= step_hz + 1e-9);
    lo_destroy (lo50);

    /* freq=51 Hz: fractional remainder ~0.635 -- truncates to 10430 (floor),
     * a ~-0.00279 Hz error (within one full step, the truncation guarantee).
     * Round-to-nearest would give 10431; this CHECK pins the truncation. */
    lo_state_t *lo51 = lo_create (51.0 / fs);
    DP_CHECK (lo_get_phase_inc (lo51) == 10430u);
    double actual51 = (double)lo_get_phase_inc (lo51) / 4294967296.0 * fs;
    DP_CHECK (actual51 <= 51.0 + 1e-9); /* truncation never overshoots */
    DP_CHECK (fabs (actual51 - 51.0) <= step_hz + 1e-9);
    lo_destroy (lo51);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    /* Asking for more than the buffer holds truncates rather than
     * overruns, and the generator advances only by what it emitted --
     * so a truncated call leaves a resumable, not a corrupted, phase. */
    lo_state_t   *lo  = lo_create (0.01);
    lo_state_t   *ref = lo_create (0.01);
    float complex out[16];
    float complex expect[5];
    for (int i = 0; i < 16; i++)
      out[i] = 42.0f + 42.0f * I;

    DP_CHECK (lo_steps (lo, 16, out, 5) == 5);
    for (int i = 5; i < 16; i++)
      DP_CHECK (out[i] == 42.0f + 42.0f * I); /* tail untouched */
    lo_steps (ref, 5, expect, 5);
    for (int i = 0; i < 5; i++)
      DP_CHECK (out[i] == expect[i]);
    DP_CHECK (lo_get_phase (lo)
              == lo_get_phase (ref)); /* advanced by 5, not 16 */

    /* Zero capacity emits nothing and does not advance the phase. */
    uint32_t before = lo_get_phase (lo);
    DP_CHECK (lo_steps (lo, 16, out, 0) == 0);
    DP_CHECK (lo_get_phase (lo) == before);

    /* The control-port form clamps on the same rule: ctrl_len is the
     * request, max_out the capacity, and the shorter one wins. */
    const double  ctrl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    float complex cout[8];
    for (int i = 0; i < 8; i++)
      cout[i] = 42.0f + 42.0f * I;
    DP_CHECK (lo_steps_ctrl (lo, ctrl, 8, cout, 3) == 3);
    for (int i = 3; i < 8; i++)
      DP_CHECK (cout[i] == 42.0f + 42.0f * I);

    lo_destroy (lo);
    lo_destroy (ref);
  }

  /* ----------------------------------------------------------------
   * 16. lo_step_ctrl — the control port.
   *
   * The header documents a full contract for this inline (added on top
   * of phase_inc for this step only, not persisted, any sign, folds
   * modulo one cycle, bit-identical to lo_step at ctrl == 0) and not
   * one clause of it was exercised anywhere: no test in this file, and
   * no binding, so no Python test either.  Each CHECK below is one
   * sentence of that contract.
   * ---------------------------------------------------------------- */
  {
    const size_t N = 129;

    /* (a) ctrl == 0 is bit-identical to lo_step(). */
    lo_state_t plain, zeroed;
    lo_init (&plain, 0.0713);
    lo_init (&zeroed, 0.0713);
    int exact = 1;
    for (size_t i = 0; i < N; i++)
      {
        float complex p = lo_step (&plain);
        float complex z = lo_step_ctrl (&zeroed, 0.0);
        if (crealf (p) != crealf (z) || cimagf (p) != cimagf (z))
          exact = 0;
      }
    DP_CHECK (exact);
    DP_CHECK (lo_get_phase (&plain) == lo_get_phase (&zeroed));

    /* (b) A constant control is the same oscillator as the equivalent
     * base rate: base 0, ctrl 0.25 == a 0.25 LO, bit-for-bit. */
    lo_state_t    driven;
    lo_state_t   *ref = lo_create (0.25);
    float complex a[64], b[64];
    lo_init (&driven, 0.0);
    lo_steps (ref, 64, a, 64);
    for (int i = 0; i < 64; i++)
      b[i] = lo_step_ctrl (&driven, 0.25);
    exact = 1;
    for (int i = 0; i < 64; i++)
      if (crealf (a[i]) != crealf (b[i]) || cimagf (a[i]) != cimagf (b[i]))
        exact = 0;
    DP_CHECK (exact);
    lo_destroy (ref);

    /* (c) A NEGATIVE control composes modularly.  The fold takes -0.25
     * to +0.75, which is the same phase-word delta: base 0.5 steered by
     * -0.25 must BE a 0.25 oscillator.  (Unlike nco's carry flag, the LO
     * has no direction to lose here — only the modular phase is
     * observable, so the fold is exactly right.) */
    lo_state_t  down;
    lo_state_t *quarter = lo_create (0.25);
    lo_init (&down, 0.5);
    lo_steps (quarter, 64, a, 64);
    for (int i = 0; i < 64; i++)
      b[i] = lo_step_ctrl (&down, -0.25);
    exact = 1;
    for (int i = 0; i < 64; i++)
      if (crealf (a[i]) != crealf (b[i]) || cimagf (a[i]) != cimagf (b[i]))
        exact = 0;
    DP_CHECK (exact);
    DP_CHECK (lo_get_phase (&down) == lo_get_phase (quarter));
    lo_destroy (quarter);

    /* (d) Only the fractional cycle survives: 1.25 == 0.25, -1.75 ==
     * 0.25.  The header says "the fractional cycle is taken, so it wraps
     * correctly" for any sign and any magnitude. */
    lo_state_t f0, f1, f2;
    lo_init (&f0, 0.0);
    lo_init (&f1, 0.0);
    lo_init (&f2, 0.0);
    /* 13 steps, not a multiple of 4: at 16 steps a quarter-rate phase is
     * back at exactly 0, so a STOPPED oscillator would compare equal and
     * the fold claim would pass vacuously.  The guard below pins that
     * precondition rather than trusting the count. */
    for (int i = 0; i < 13; i++)
      {
        (void)lo_step_ctrl (&f0, 0.25);
        (void)lo_step_ctrl (&f1, 1.25);
        (void)lo_step_ctrl (&f2, -1.75);
      }
    DP_CHECK (lo_get_phase (&f0) != 0u); /* not the vacuous comparison */
    DP_CHECK (lo_get_phase (&f1) == lo_get_phase (&f0));
    DP_CHECK (lo_get_phase (&f2) == lo_get_phase (&f0));

    /* (e) The control is transient: it never lands in phase_inc or
     * norm_freq, so the step AFTER a steered one advances by the centre
     * increment alone. */
    lo_state_t keep;
    lo_init (&keep, 0.1);
    uint32_t inc0 = lo_get_phase_inc (&keep);
    (void)lo_step_ctrl (&keep, 0.37);
    DP_CHECK (lo_get_phase_inc (&keep) == inc0);
    DP_CHECK (lo_get_norm_freq (&keep) == 0.1);
    uint32_t ph_after_steer = lo_get_phase (&keep);
    (void)lo_step_ctrl (&keep, 0.0);
    DP_CHECK (lo_get_phase (&keep) == ph_after_steer + inc0);

    /* (f) Emit BEFORE increment, on the steered path too: seeded at an
     * arbitrary phase word, the first sample is the LUT at THAT word. */
    lo_state_t seeded;
    lo_init (&seeded, 0.1);
    lo_set_phase (&seeded, 0x9ABC0000u);
    uint16_t      widx = (uint16_t)(0x9ABC0000u >> 16);
    float complex s0   = lo_step_ctrl (&seeded, 0.42);
    DP_CHECK (crealf (s0)
              == lo_sin_lut[(uint16_t)(widx + (uint16_t)LO_LUT_QTR)]);
    DP_CHECK (cimagf (s0) == lo_sin_lut[widx]);
  }

  /* ----------------------------------------------------------------
   * 17. lo_steps_ctrl is exactly a loop over lo_step_ctrl.
   *
   * Section 8 pins that equivalence for the free-running pair; the
   * steered pair had no such check, and it is the pair where the two
   * implementations genuinely differ (one folds a double per sample, the
   * other a float widened to double -- and on an AVX-512 host the block
   * form takes an entirely separate vector path).  Driven with a control
   * that changes sign and exceeds one cycle, so the fold is exercised
   * per sample rather than held constant.
   * ---------------------------------------------------------------- */
  {
    const size_t  N = 133;
    double        ctrl[133];
    float complex blk[133], one[133];
    for (size_t i = 0; i < N; i++)
      ctrl[i] = 0.03 * sin (0.11 * (double)i) - 0.007 * (double)i;

    lo_state_t *bs = lo_create (0.077);
    lo_state_t  ss;
    lo_init (&ss, 0.077);
    DP_CHECK (lo_steps_ctrl (bs, ctrl, N, blk, N) == N);
    for (size_t i = 0; i < N; i++)
      one[i] = lo_step_ctrl (&ss, (double)ctrl[i]);

    int exact = 1;
    for (size_t i = 0; i < N; i++)
      if (crealf (blk[i]) != crealf (one[i])
          || cimagf (blk[i]) != cimagf (one[i]))
        exact = 0;
    DP_CHECK (exact);
    DP_CHECK (lo_get_phase (bs) == lo_get_phase (&ss));
    /* the block form must not persist the control either */
    DP_CHECK (lo_get_phase_inc (bs) == lo_get_phase_inc (&ss));
    DP_CHECK (lo_get_norm_freq (bs) == 0.077);
    lo_destroy (bs);
  }

  /* ----------------------------------------------------------------
   * 18. lo_destroy(NULL) is a no-op.
   *
   * "May be NULL (no-op)" is a documented promise a caller writing an
   * error path relies on; nothing checked it.
   * ---------------------------------------------------------------- */
  {
    lo_destroy (NULL);
    DP_CHECK (1); /* reached: the call above did not fault */
  }

  /* ----------------------------------------------------------------
   * 19. max_out is the real bound; LO_MAX_OUT is advisory.
   *
   * lo_core.c still says "calling with n > 65536 overflows the buffer
   * and is undefined behaviour", which was true before pass_capacity
   * (jm gh-138) gave the kernel the caller's capacity.  It is now the
   * Python extension's PRE-ALLOCATION size, not a limit on the C API: a
   * C caller supplying a larger buffer and a matching max_out gets every
   * sample it asked for.  Pinned so the stale sentence cannot quietly
   * become true again.
   * ---------------------------------------------------------------- */
  {
    DP_CHECK (lo_steps_max_out (NULL) == 65536u);
    DP_CHECK (lo_steps_ctrl_max_out (NULL) == 65536u);

    const size_t   BIG = 70000;
    float complex *big = malloc (BIG * sizeof *big);
    DP_CHECK (big != NULL);
    if (big)
      {
        lo_state_t *lo = lo_create (0.013);
        DP_CHECK (lo_steps (lo, BIG, big, BIG) == BIG);
        /* every sample really was written: the phase advanced by all of
         * them, and the tail is on the unit circle rather than zeroed */
        lo_state_t *chk = lo_create (0.013);
        DP_CHECK (
            lo_get_phase (lo)
            == (uint32_t)((uint64_t)lo_get_phase_inc (chk) * (uint64_t)BIG));
        float m2 = crealf (big[BIG - 1]) * crealf (big[BIG - 1])
                   + cimagf (big[BIG - 1]) * cimagf (big[BIG - 1]);
        DP_CHECK (near (m2, 1.0f));
        lo_destroy (lo);
        lo_destroy (chk);
        free (big);
      }
  }

  /* ----------------------------------------------------------------
   * 20. The LUT itself, as a law rather than four literals.
   *
   * Sections 3 and 6 check magnitude and quadrature at quarter-rate,
   * where every LUT entry involved is exactly 0 or +-1 -- the four
   * points that cannot detect a wrong table.  These bound the error
   * across all 65536 entries, and the sin->cos quarter-cycle offset the
   * file comment claims, at every index.
   * ---------------------------------------------------------------- */
  {
    lo_state_t warm; /* forces lut_init() before reading the table */
    lo_init (&warm, 0.0);

    float worst_sin = 0.0f, worst_cos = 0.0f;
    for (unsigned i = 0; i < LO_LUT_SIZE; i++)
      {
        double th = 2.0 * M_PI * (double)i / (double)LO_LUT_SIZE;
        float  es = fabsf (lo_sin_lut[i] - (float)sin (th));
        float  ec = fabsf (lo_sin_lut[(uint16_t)(i + (uint16_t)LO_LUT_QTR)]
                           - (float)cos (th));
        if (es > worst_sin)
          worst_sin = es;
        if (ec > worst_cos)
          worst_cos = ec;
      }
    DP_CHECK (worst_sin < 1e-6f); /* the table IS sin, everywhere      */
    DP_CHECK (worst_cos < 1e-6f); /* +QTR IS cos, at every index       */

    /* Unit magnitude over a full sweep of the phase range, not just the
     * four exact points: an odd increment coprime with 2^32 visits every
     * LUT bin over a long enough run. */
    lo_state_t   *sweep = lo_create (0.10000000017);
    float complex buf[4096];
    float         worst_m = 0.0f;
    for (int blk = 0; blk < 64; blk++)
      {
        lo_steps (sweep, 4096, buf, 4096);
        for (int i = 0; i < 4096; i++)
          {
            float m2 = crealf (buf[i]) * crealf (buf[i])
                       + cimagf (buf[i]) * cimagf (buf[i]);
            if (fabsf (m2 - 1.0f) > worst_m)
              worst_m = fabsf (m2 - 1.0f);
          }
      }
    DP_CHECK (worst_m < 1e-5f);
    lo_destroy (sweep);
  }

  /* ----------------------------------------------------------------
   * 21. The control port TRUNCATES, like every other face of the
   *     conversion.
   *
   * Section 15 pins truncation on the CONFIGURE path (lo_create /
   * lo_set_norm_freq).  The control port is the other way into the same
   * accumulator, and the header comment beside lo_step_ctrl asserts the
   * opposite -- that nco_norm_freq_to_inc() "rounds, not truncates".
   * nco_core.h documents truncation, and section 15 measures it, so the
   * two faces are pinned against each other here: the SAME requested
   * frequency must produce the SAME phase word whichever way it enters.
   *
   * 51/21e6 is the section-15 case whose exact increment has fractional
   * remainder ~0.635 -- truncation gives 10430, round-to-nearest 10431 --
   * so this CHECK distinguishes the two conventions rather than landing
   * on a value they share.
   * ---------------------------------------------------------------- */
  {
    const double f51 = 51.0 / 21.0e6;

    lo_state_t *cfg = lo_create (f51);
    DP_CHECK (lo_get_phase_inc (cfg) == 10430u); /* configure path */

    lo_state_t steer;
    lo_init (&steer, 0.0);
    (void)lo_step_ctrl (&steer, f51);
    DP_CHECK (lo_get_phase (&steer) == 10430u); /* control path, same word */

    /* and the block control port agrees with both */
    lo_state_t   *bsteer = lo_create (0.0);
    const double  c1[1]  = { f51 };
    float complex o1[1];
    lo_steps_ctrl (bsteer, c1, 1, o1, 1);
    /* float32 rounds the REQUEST before the fold ever sees it, so the
     * two differ by the float32 representation error, not by the
     * conversion's rounding mode -- a few counts, never a whole step. */
    DP_CHECK (lo_get_phase (bsteer) <= 10430u + 4u);
    DP_CHECK (lo_get_phase (bsteer) + 4u >= 10430u);

    lo_destroy (cfg);
    lo_destroy (bsteer);
  }

  DP_TEST_END ("test_lo_core");
}
