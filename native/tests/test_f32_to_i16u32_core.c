#include "dp_state_test.h"
#include "dp_test.h"
#include "f32_to_i16u32/f32_to_i16u32_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  /* ── Invalid args → NULL ──────────────────────────────────────────── */
  DP_CHECK (f32_to_i16u32_create (0.0f) == NULL);
  DP_CHECK (f32_to_i16u32_create (-1.0f) == NULL);

  /* ── +1.0 → 0x00007FFF (Q15_MAX, upper 16 bits zero) ─────────────── */
  {
    f32_to_i16u32_state_t *obj = f32_to_i16u32_create (32768.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (f32_to_i16u32_step (obj, 1.0f) == 0x00007FFFu);
    /* -1.0 → int16 -32768 → uint16 0x8000 → uint32 0x00008000 */
    DP_CHECK (f32_to_i16u32_step (obj, -1.0f) == 0x00008000u);
    DP_CHECK (f32_to_i16u32_step (obj, 0.0f) == 0u);
    /* Saturation: upper 16 bits must remain zero */
    DP_CHECK ((f32_to_i16u32_step (obj, 2.0f) & 0xFFFF0000u) == 0u);
    DP_CHECK ((f32_to_i16u32_step (obj, -2.0f) & 0xFFFF0000u) == 0u);
    f32_to_i16u32_destroy (obj);
  }

  /* ── Known boundary values: verify saturation and zero-extension ──── */
  {
    f32_to_i16u32_state_t *obj = f32_to_i16u32_create (32768.0f);
    DP_CHECK (obj != NULL);
    /* -0.5 → int16 round(-16384) = -16384 → uint16 0xC000 = 49152 */
    uint32_t u     = f32_to_i16u32_step (obj, -0.5f);
    int16_t  lower = (int16_t)(uint16_t)(u & 0xFFFFu);
    DP_CHECK (lower == -16384);
    DP_CHECK ((u >> 16) == 0u);
    /* +0.5 → int16 round(16384) = 16384 → uint32 0x00004000 */
    u     = f32_to_i16u32_step (obj, 0.5f);
    lower = (int16_t)(uint16_t)(u & 0xFFFFu);
    DP_CHECK (lower == 16384);
    DP_CHECK ((u >> 16) == 0u);
    f32_to_i16u32_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    f32_to_i16u32_state_t *a = f32_to_i16u32_create (32768.0f);
    f32_to_i16u32_state_t *b = f32_to_i16u32_create (32768.0f);
    DP_CHECK (a && b);
    float    input[32];
    uint32_t bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = -1.5f + 3.0f * i / 31.0f;
    f32_to_i16u32_steps (a, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = f32_to_i16u32_step (b, input[i]);
    DP_CHECK (memcmp (bulk, loop, 32 * sizeof (uint32_t)) == 0);
    f32_to_i16u32_destroy (a);
    f32_to_i16u32_destroy (b);
  }

  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    f32_to_i16u32_state_t *a = f32_to_i16u32_create (32768.0f);
    f32_to_i16u32_state_t *b = f32_to_i16u32_create (32768.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)f32_to_i16u32_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (f32_to_i16u32, a, b);
    DP_CHECK (b->clipped == a->clipped);
    f32_to_i16u32_destroy (a);
    f32_to_i16u32_destroy (b);
  }

  DP_TEST_END ("test_f32_to_i16u32_core");
}
