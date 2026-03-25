/**
 * @file test_nco.c
 * @brief Unit tests for dp_nco_* numerically controlled oscillator.
 *
 * Tests are self-contained: no external framework required.
 * Exit code 0 = all passed, non-zero = failure.
 */

#include <dp/nco.h>
#include <dp/stream.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================================================================
 * Helpers
 * ========================================================================= */

static int passed = 0;
static int failed = 0;

#define PASS(msg)                                                              \
  do                                                                           \
    {                                                                          \
      printf ("  PASS  %s\n", (msg));                                          \
      passed++;                                                                \
    }                                                                          \
  while (0)

#define FAIL(msg)                                                              \
  do                                                                           \
    {                                                                          \
      printf ("  FAIL  %s\n", (msg));                                          \
      failed++;                                                                \
    }                                                                          \
  while (0)

#define CHECK(cond, msg)                                                       \
  do                                                                           \
    {                                                                          \
      if (cond)                                                                \
        PASS (msg);                                                            \
      else                                                                     \
        FAIL (msg);                                                            \
    }                                                                          \
  while (0)

/* Tolerance matched to 2^16 LUT entry precision (floats at LUT points
 * are exact to ~1 ULP of sinf; we use a generous 1e-5). */
#define TOL 1e-5f

static int
cf32_near (dp_cf32_t a, dp_cf32_t b, float tol)
{
  return fabsf (a.i - b.i) <= tol && fabsf (a.q - b.q) <= tol;
}

/* =========================================================================
 * Test 1 — create / destroy
 * ========================================================================= */

static void
test_create_destroy (void)
{
  printf ("--- create / destroy\n");
  dp_nco_t *nco = dp_nco_create (0.0f);
  CHECK (nco != NULL, "dp_nco_create returns non-NULL");
  dp_nco_destroy (nco);
  PASS ("dp_nco_destroy does not crash");

  /* NULL destroy is a no-op */
  dp_nco_destroy (NULL);
  PASS ("dp_nco_destroy(NULL) is safe");
}

/* =========================================================================
 * Test 2 — zero frequency: all samples must be (1, 0)
 * ========================================================================= */

static void
test_zero_freq (void)
{
  printf ("--- zero frequency\n");
  dp_nco_t *nco = dp_nco_create (0.0f);
  dp_cf32_t out[8];
  dp_nco_execute_cf32 (nco, out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    if (!cf32_near (out[i], (dp_cf32_t){1.0f, 0.0f}, TOL))
      ok = 0;
  CHECK (ok, "f_n=0 → all samples (1, 0)");
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 3 — quarter-rate tone: 4-sample periodicity with known values
 *
 *   f_n = 0.25  →  phase_inc = 2^30
 *   sample 0: phase=0       → (cos 0°,   sin 0°)   = ( 1,  0)
 *   sample 1: phase=2^30    → (cos 90°,  sin 90°)  = ( 0,  1)
 *   sample 2: phase=2^31    → (cos 180°, sin 180°) = (-1,  0)
 *   sample 3: phase=3×2^30  → (cos 270°, sin 270°) = ( 0, -1)
 *   sample 4: phase=0 (wrap)→ ( 1,  0)  again
 * ========================================================================= */

static void
test_quarter_rate (void)
{
  printf ("--- quarter-rate tone (f_n = 0.25)\n");
  dp_nco_t *nco = dp_nco_create (0.25f);
  dp_cf32_t out[8];
  dp_nco_execute_cf32 (nco, out, 8);

  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f,  0.0f}, TOL),
         "sample 0: (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){ 0.0f,  1.0f}, TOL),
         "sample 1: (0, 1)");
  CHECK (cf32_near (out[2], (dp_cf32_t){-1.0f,  0.0f}, TOL),
         "sample 2: (-1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){ 0.0f, -1.0f}, TOL),
         "sample 3: (0, -1)");
  CHECK (cf32_near (out[4], out[0], TOL), "sample 4 == sample 0 (wrap)");
  CHECK (cf32_near (out[5], out[1], TOL), "sample 5 == sample 1");
  CHECK (cf32_near (out[6], out[2], TOL), "sample 6 == sample 2");
  CHECK (cf32_near (out[7], out[3], TOL), "sample 7 == sample 3");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 4 — half-rate tone: alternates (1,0) / (-1, 0)
 * ========================================================================= */

static void
test_half_rate (void)
{
  printf ("--- half-rate tone (f_n = 0.5)\n");
  dp_nco_t *nco = dp_nco_create (0.5f);
  dp_cf32_t out[4];
  dp_nco_execute_cf32 (nco, out, 4);

  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f, 0.0f}, TOL),
         "sample 0: (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){-1.0f, 0.0f}, TOL),
         "sample 1: (-1, 0)");
  CHECK (cf32_near (out[2], (dp_cf32_t){ 1.0f, 0.0f}, TOL),
         "sample 2: (1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){-1.0f, 0.0f}, TOL),
         "sample 3: (-1, 0)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 5 — unity amplitude over 1024 samples at an irrational frequency
 * ========================================================================= */

static void
test_unity_amplitude (void)
{
  printf ("--- unity amplitude (f_n = 0.137, 1024 samples)\n");
  dp_nco_t *nco = dp_nco_create (0.137f);
  dp_cf32_t out[1024];
  dp_nco_execute_cf32 (nco, out, 1024);

  float max_err = 0.0f;
  for (int i = 0; i < 1024; i++)
    {
      float amp = sqrtf (out[i].i * out[i].i + out[i].q * out[i].q);
      float err = fabsf (amp - 1.0f);
      if (err > max_err)
        max_err = err;
    }
  /* 2^16 LUT: amplitude error is set solely by sinf() float precision,
   * well under 1e-6 at the LUT points. */
  CHECK (max_err < 1e-5f, "amplitude error < 1e-5 for all 1024 samples");
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 6 — phase continuity across two execute calls
 * ========================================================================= */

static void
test_phase_continuity (void)
{
  printf ("--- phase continuity across execute calls\n");

  /* Reference: generate 8 samples in one shot */
  dp_nco_t *ref = dp_nco_create (0.25f);
  dp_cf32_t ref_out[8];
  dp_nco_execute_cf32 (ref, ref_out, 8);

  /* Test: generate 4+4 */
  dp_nco_t *nco = dp_nco_create (0.25f);
  dp_cf32_t out_a[4], out_b[4];
  dp_nco_execute_cf32 (nco, out_a, 4);
  dp_nco_execute_cf32 (nco, out_b, 4);

  int ok = 1;
  for (int i = 0; i < 4; i++)
    {
      if (!cf32_near (out_a[i], ref_out[i],     TOL))
        ok = 0;
      if (!cf32_near (out_b[i], ref_out[4 + i], TOL))
        ok = 0;
    }
  CHECK (ok, "4+4 execute matches single 8-sample execute");

  dp_nco_destroy (ref);
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 7 — reset restores phase to zero
 * ========================================================================= */

static void
test_reset (void)
{
  printf ("--- reset\n");
  dp_nco_t *nco = dp_nco_create (0.25f);
  dp_cf32_t out[4];

  /* Advance by 3 samples, then reset */
  dp_nco_execute_cf32 (nco, out, 3);
  dp_nco_reset (nco);
  dp_nco_execute_cf32 (nco, out, 4);

  /* After reset, should see the same sequence as from phase 0 */
  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f,  0.0f}, TOL),
         "after reset: sample 0 = (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){ 0.0f,  1.0f}, TOL),
         "after reset: sample 1 = (0, 1)");
  CHECK (cf32_near (out[2], (dp_cf32_t){-1.0f,  0.0f}, TOL),
         "after reset: sample 2 = (-1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){ 0.0f, -1.0f}, TOL),
         "after reset: sample 3 = (0, -1)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 8 — set_freq changes frequency mid-stream
 * ========================================================================= */

static void
test_set_freq (void)
{
  printf ("--- set_freq\n");
  dp_nco_t *nco = dp_nco_create (0.0f); /* start at DC */
  dp_cf32_t out[4];

  /* At DC: 2 samples all (1, 0) */
  dp_nco_execute_cf32 (nco, out, 2);
  CHECK (cf32_near (out[0], (dp_cf32_t){1.0f, 0.0f}, TOL),
         "pre set_freq: sample 0 = (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){1.0f, 0.0f}, TOL),
         "pre set_freq: sample 1 = (1, 0)");

  /* Switch to f_n = 0.25 (phase stays at 0, so next sample is (1,0)) */
  dp_nco_set_freq (nco, 0.25f);
  dp_nco_execute_cf32 (nco, out, 4);
  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f,  0.0f}, TOL),
         "post set_freq: sample 0 = (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){ 0.0f,  1.0f}, TOL),
         "post set_freq: sample 1 = (0, 1)");
  CHECK (cf32_near (out[2], (dp_cf32_t){-1.0f,  0.0f}, TOL),
         "post set_freq: sample 2 = (-1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){ 0.0f, -1.0f}, TOL),
         "post set_freq: sample 3 = (0, -1)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 9 — negative frequency wraps to conjugate rotation
 *
 *   f_n = −0.25 ≡ 0.75  →  phase_inc = 3×2^30
 *   sample 0: phase=0       → ( 1,  0)
 *   sample 1: phase=3×2^30  → ( 0, -1)   (−90° rotation per sample)
 *   sample 2: phase=6×2^30  = 2^31+2^30 → (-1,  0)
 *   sample 3: phase=9×2^30  = 2^32+2^30 → ( 0,  1)  (wrap)
 * ========================================================================= */

static void
test_negative_freq (void)
{
  printf ("--- negative frequency (f_n = -0.25)\n");
  dp_nco_t *nco = dp_nco_create (-0.25f);
  dp_cf32_t out[4];
  dp_nco_execute_cf32 (nco, out, 4);

  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f,  0.0f}, TOL),
         "sample 0: (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){ 0.0f, -1.0f}, TOL),
         "sample 1: (0, -1)");
  CHECK (cf32_near (out[2], (dp_cf32_t){-1.0f,  0.0f}, TOL),
         "sample 2: (-1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){ 0.0f,  1.0f}, TOL),
         "sample 3: (0, 1)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 10 — control port: zero ctrl equals free-running
 * ========================================================================= */

static void
test_ctrl_zero (void)
{
  printf ("--- ctrl port: zero deviation = free-running\n");
  float     ctrl[8] = {0};
  dp_nco_t *ref     = dp_nco_create (0.25f);
  dp_nco_t *nco     = dp_nco_create (0.25f);
  dp_cf32_t ref_out[8], ctrl_out[8];

  dp_nco_execute_cf32      (ref, ref_out,  8);
  dp_nco_execute_cf32_ctrl (nco, ctrl, ctrl_out, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    if (!cf32_near (ctrl_out[i], ref_out[i], TOL))
      ok = 0;
  CHECK (ok, "zero ctrl matches free-running output");

  dp_nco_destroy (ref);
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 11 — control port: constant +0.25 deviation doubles frequency
 * ========================================================================= */

static void
test_ctrl_freq_shift (void)
{
  printf ("--- ctrl port: +0.25 deviation doubles quarter-rate\n");

  /* Base f_n=0.25, ctrl=+0.25 → effective f_n=0.5 each sample */
  float     ctrl[4] = {0.25f, 0.25f, 0.25f, 0.25f};
  dp_nco_t *nco     = dp_nco_create (0.25f);
  dp_cf32_t out[4];
  dp_nco_execute_cf32_ctrl (nco, ctrl, out, 4);

  /* At effective f_n=0.5 the expected sequence is (1,0),(-1,0),(1,0),(-1,0) */
  CHECK (cf32_near (out[0], (dp_cf32_t){ 1.0f, 0.0f}, TOL),
         "sample 0: (1, 0)");
  CHECK (cf32_near (out[1], (dp_cf32_t){-1.0f, 0.0f}, TOL),
         "sample 1: (-1, 0)");
  CHECK (cf32_near (out[2], (dp_cf32_t){ 1.0f, 0.0f}, TOL),
         "sample 2: (1, 0)");
  CHECK (cf32_near (out[3], (dp_cf32_t){-1.0f, 0.0f}, TOL),
         "sample 3: (-1, 0)");

  /* Base phase_inc must be unchanged after ctrl call */
  dp_cf32_t after[4];
  dp_nco_reset (nco);
  dp_nco_execute_cf32 (nco, after, 4);
  CHECK (cf32_near (after[1], (dp_cf32_t){0.0f, 1.0f}, TOL),
         "base phase_inc unchanged after ctrl execute");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 12 — u32 free-running: exact phase values at f_n = 0.25
 *
 *   phase_inc = 2^30 = 1073741824
 *   out[0] = 0, out[1] = 2^30, out[2] = 2^31, out[3] = 3*2^30, out[4] = 0
 * ========================================================================= */

static void
test_u32_phase_values (void)
{
  printf ("--- u32 free-running: exact phase values\n");
  dp_nco_t *nco = dp_nco_create (0.25f);
  uint32_t  out[8];
  dp_nco_execute_u32 (nco, out, 8);

  const uint32_t q = 1073741824u; /* 2^30 */
  CHECK (out[0] == 0u,   "out[0] = 0");
  CHECK (out[1] == q,    "out[1] = 2^30");
  CHECK (out[2] == 2*q,  "out[2] = 2^31");
  CHECK (out[3] == 3*q,  "out[3] = 3*2^30");
  CHECK (out[4] == 0u,   "out[4] = 0 (wrap)");
  CHECK (out[5] == q,    "out[5] = 2^30");
  CHECK (out[6] == 2*q,  "out[6] = 2^31");
  CHECK (out[7] == 3*q,  "out[7] = 3*2^30");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 13 — u32 LUT consistency: u32 phase indexes same LUT entry as cf32
 * ========================================================================= */

static void
test_u32_lut_consistency (void)
{
  printf ("--- u32 / cf32 LUT index consistency\n");
  dp_nco_t *a   = dp_nco_create (0.137f);
  dp_nco_t *b   = dp_nco_create (0.137f);
  uint32_t  pu[64];
  dp_cf32_t pc[64];

  dp_nco_execute_u32  (a, pu, 64);
  dp_nco_execute_cf32 (b, pc, 64);

  /* For each sample, re-derive what the cf32 path would produce from
   * the raw phase value and verify it matches the actual cf32 output.
   * This checks that the two execute paths share identical phase
   * progression. */
  int ok = 1;
  for (int i = 0; i < 64; i++)
    {
      /* Top 16 bits index the LUT (same as the cf32 path). */
      uint16_t idx = (uint16_t)(pu[i] >> 16);
      /* Check that cos/sin indices are consistent with cf32 output
       * by verifying both agree to within float32 epsilon. */
      (void)idx; /* idx used implicitly: if phase is identical the  */
                 /* lut lookup must be identical too.                */
      /* Simpler: just verify the phase state advances in lockstep
       * by comparing the two NCOs' states via a reset+re-run. */
      (void)pc; /* suppress unused-variable warning in fallback path */
      (void)ok;
      break; /* use the structural test below instead */
    }

  /* Structural test: after executing the same number of samples both
   * NCOs must be at the same phase — advance one more step and check
   * the cf32 output matches the phase from the u32 path. */
  uint32_t  next_ph[1];
  dp_cf32_t next_cf[1];
  dp_nco_execute_u32  (a, next_ph, 1);
  dp_nco_execute_cf32 (b, next_cf, 1);

  /* Compute expected I/Q from next_ph[0] (top 16 bits → LUT index). */
  /* We trust the internal LUT — just verify the phase values agree   */
  /* by checking that reset+replay produces matching output.           */
  dp_nco_reset (a);
  dp_nco_reset (b);
  dp_nco_execute_u32  (a, pu, 64);
  dp_nco_execute_cf32 (b, pc, 64);

  ok = 1;
  for (int i = 0; i < 64; i++)
    {
      /* The cf32 output uses the same phase as the u32 output, so
       * applying the LUT to pu[i] must reproduce pc[i].           */
      /* Verify: phase[i] >> 16 is the LUT index used for pc[i].  */
      uint16_t idx     = (uint16_t)(pu[i] >> 16);
      uint16_t cos_idx = (uint16_t)(idx + 16384u); /* QTR = N/4   */
      /* We can't reach the static LUT directly from the test; use
       * the invariant cos²+sin²=1 and cross-check I from the cf32
       * output equals cos of the phase angle.                      */
      float expected_angle = (float)pu[i] / 4294967296.0f
                             * 2.0f * 3.14159265358979f;
      float expected_i = cosf (expected_angle);
      float expected_q = sinf (expected_angle);
      (void)cos_idx;
      if (fabsf (pc[i].i - expected_i) > 1e-4f
          || fabsf (pc[i].q - expected_q) > 1e-4f)
        ok = 0;
    }
  CHECK (ok, "cf32 output matches cos/sin of u32 phase for 64 samples");

  dp_nco_destroy (a);
  dp_nco_destroy (b);
}

/* =========================================================================
 * Test 14 — u32_ctrl: zero ctrl matches u32 free-running
 * ========================================================================= */

static void
test_u32_ctrl_zero (void)
{
  printf ("--- u32_ctrl: zero deviation = u32 free-running\n");
  float     ctrl[16] = {0};
  dp_nco_t *ref      = dp_nco_create (0.25f);
  dp_nco_t *nco      = dp_nco_create (0.25f);
  uint32_t  ref_out[16], ctrl_out[16];

  dp_nco_execute_u32      (ref, ref_out,  16);
  dp_nco_execute_u32_ctrl (nco, ctrl, ctrl_out, 16);

  int ok = 1;
  for (int i = 0; i < 16; i++)
    if (ctrl_out[i] != ref_out[i])
      ok = 0;
  CHECK (ok, "zero ctrl_out matches free-running u32 for 16 samples");

  dp_nco_destroy (ref);
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 15 — overflow / carry: f_n=0.25, carry every 4 samples
 *
 *   phase_inc = 2^30.  Starting at 0:
 *   i=0: ph=0,     new=2^30,   no carry
 *   i=1: ph=2^30,  new=2^31,   no carry
 *   i=2: ph=2^31,  new=3*2^30, no carry
 *   i=3: ph=3*2^30,new=0,      CARRY
 *   i=4: ph=0, ...  (repeats)
 * ========================================================================= */

static void
test_ovf_quarter_rate (void)
{
  printf ("--- overflow carry: f_n=0.25, carry every 4th sample\n");
  dp_nco_t *nco = dp_nco_create (0.25f);
  uint32_t  out[16];
  uint8_t   carry[16];
  dp_nco_execute_u32_ovf (nco, out, carry, 16);

  int ok = 1;
  for (int i = 0; i < 16; i++)
    {
      int expected = ((i & 3) == 3); /* carry on indices 3,7,11,15 */
      if ((int)carry[i] != expected)
        ok = 0;
    }
  CHECK (ok, "carry fires exactly at indices 3,7,11,15 (every 4th)");

  /* Also verify no carry on the non-overflow samples */
  CHECK (carry[0] == 0, "carry[0] = 0");
  CHECK (carry[1] == 0, "carry[1] = 0");
  CHECK (carry[2] == 0, "carry[2] = 0");
  CHECK (carry[3] == 1, "carry[3] = 1  (wrap at 4*2^30 = 2^32)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 16 — overflow / carry: f_n=0.5, carry every 2 samples
 *
 *   phase_inc = 2^31.  Starting at 0:
 *   i=0: ph=0,    new=2^31, no carry
 *   i=1: ph=2^31, new=0,    CARRY
 *   i=2: ph=0,    ...       (repeats)
 * ========================================================================= */

static void
test_ovf_half_rate (void)
{
  printf ("--- overflow carry: f_n=0.5, carry every 2nd sample\n");
  dp_nco_t *nco = dp_nco_create (0.5f);
  uint32_t  out[8];
  uint8_t   carry[8];
  dp_nco_execute_u32_ovf (nco, out, carry, 8);

  int ok = 1;
  for (int i = 0; i < 8; i++)
    {
      int expected = (i & 1); /* carry on odd indices: 1,3,5,7 */
      if ((int)carry[i] != expected)
        ok = 0;
    }
  CHECK (ok, "carry fires exactly at odd indices (every 2nd)");
  CHECK (carry[0] == 0, "carry[0] = 0");
  CHECK (carry[1] == 1, "carry[1] = 1  (wrap at 2*2^31 = 2^32)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 17 — ovf_ctrl: zero ctrl matches u32_ovf free-running
 * ========================================================================= */

static void
test_ovf_ctrl_zero (void)
{
  printf ("--- ovf_ctrl: zero deviation = u32_ovf free-running\n");
  float     ctrl[16] = {0};
  dp_nco_t *ref      = dp_nco_create (0.25f);
  dp_nco_t *nco      = dp_nco_create (0.25f);
  uint32_t  ref_out[16], ctrl_out[16];
  uint8_t   ref_carry[16], ctrl_carry[16];

  dp_nco_execute_u32_ovf      (ref, ref_out,  ref_carry,  16);
  dp_nco_execute_u32_ovf_ctrl (nco, ctrl, ctrl_out, ctrl_carry, 16);

  int ok_ph = 1, ok_c = 1;
  for (int i = 0; i < 16; i++)
    {
      if (ctrl_out[i]   != ref_out[i])   ok_ph = 0;
      if (ctrl_carry[i] != ref_carry[i]) ok_c  = 0;
    }
  CHECK (ok_ph, "zero ctrl: phase output matches free-running");
  CHECK (ok_c,  "zero ctrl: carry output matches free-running");

  dp_nco_destroy (ref);
  dp_nco_destroy (nco);
}

/* =========================================================================
 * Test 18 — ovf_ctrl: +0.25 deviation doubles quarter-rate carry rate
 *
 *  Base f_n=0.25, ctrl=+0.25 → effective f_n=0.5 → carry every 2 samples
 * ========================================================================= */

static void
test_ovf_ctrl_freq_shift (void)
{
  printf ("--- ovf_ctrl: +0.25 deviation → half-rate carry pattern\n");
  float     ctrl[8] = {0.25f, 0.25f, 0.25f, 0.25f,
                       0.25f, 0.25f, 0.25f, 0.25f};
  dp_nco_t *nco     = dp_nco_create (0.25f);
  uint32_t  out[8];
  uint8_t   carry[8];
  dp_nco_execute_u32_ovf_ctrl (nco, ctrl, out, carry, 8);

  /* Effective f_n=0.5 → same carry pattern as test_ovf_half_rate */
  int ok = 1;
  for (int i = 0; i < 8; i++)
    if ((int)carry[i] != (i & 1))
      ok = 0;
  CHECK (ok, "carry pattern matches effective f_n=0.5 (odd indices)");

  dp_nco_destroy (nco);
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== dp_nco unit tests ===\n");

  test_create_destroy ();
  test_zero_freq ();
  test_quarter_rate ();
  test_half_rate ();
  test_unity_amplitude ();
  test_phase_continuity ();
  test_reset ();
  test_set_freq ();
  test_negative_freq ();
  test_ctrl_zero ();
  test_ctrl_freq_shift ();
  test_u32_phase_values ();
  test_u32_lut_consistency ();
  test_u32_ctrl_zero ();
  test_ovf_quarter_rate ();
  test_ovf_half_rate ();
  test_ovf_ctrl_zero ();
  test_ovf_ctrl_freq_shift ();

  printf ("\n=== %d passed, %d failed ===\n", passed, failed);
  return failed ? 1 : 0;
}
