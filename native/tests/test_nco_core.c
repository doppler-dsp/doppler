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
 */
#include "nco/nco_core.h"
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

  if (_fails)
    {
      fprintf (stderr, "test_nco_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_nco_core PASSED\n");
  return 0;
}
