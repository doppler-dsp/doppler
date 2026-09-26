#include "doppler/f32_to_i8/f32_to_i8_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  /* ── Invalid args → NULL ──────────────────────────────────────────────── */
  DP_CHECK (dp_f32_to_i8_create (0.0f) == NULL);
  DP_CHECK (dp_f32_to_i8_create (-1.0f) == NULL);

  /* ── Saturation ───────────────────────────────────────────────────────── */
  {
    dp_f32_to_i8_state_t *obj = dp_f32_to_i8_create (128.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_f32_to_i8_step (obj, 1.0f) == 127); /* +1.0 -> 128, clamped */
    DP_CHECK (dp_f32_to_i8_step (obj, -1.0f) == -128); /* exact fit */
    DP_CHECK (dp_f32_to_i8_step (obj, 2.0f) == 127);   /* hard saturation */
    DP_CHECK (dp_f32_to_i8_step (obj, -2.0f) == -128);
    DP_CHECK (dp_f32_to_i8_step (obj, 0.0f) == 0);
    dp_f32_to_i8_destroy (obj);
  }

  /* ── The sticky clip flag fires on +1.0 and NOT on -1.0 ──────────────── */
  {
    dp_f32_to_i8_state_t *obj = dp_f32_to_i8_create (128.0f);
    DP_CHECK (obj != NULL);
    (void)dp_f32_to_i8_step (obj, -1.0f);
    DP_CHECK (obj->clipped == 0); /* -128 is representable */
    (void)dp_f32_to_i8_step (obj, 1.0f);
    DP_CHECK (obj->clipped == 1); /* +128 is not */
    dp_f32_to_i8_reset (obj);
    DP_CHECK (obj->clipped == 0);
    dp_f32_to_i8_destroy (obj);
  }

  /* ── Round-to-nearest, NOT truncation: 0.5 LSB = 0.5/128 ─────────────── */
  {
    dp_f32_to_i8_state_t *obj = dp_f32_to_i8_create (128.0f);
    DP_CHECK (obj != NULL);
    float half_lsb = 0.5f / 128.0f;
    DP_CHECK (dp_f32_to_i8_step (obj, half_lsb) == 1);
    DP_CHECK (dp_f32_to_i8_step (obj, -half_lsb) == -1);
    /* Just under a whole LSB still rounds up; truncation would give 0. */
    DP_CHECK (dp_f32_to_i8_step (obj, 0.99f / 128.0f) == 1);
    dp_f32_to_i8_destroy (obj);
  }

  /* ── Custom scale (scale=1.0: integers pass through unchanged) ────────── */
  {
    dp_f32_to_i8_state_t *obj = dp_f32_to_i8_create (1.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_f32_to_i8_step (obj, 100.0f) == 100);
    DP_CHECK (dp_f32_to_i8_step (obj, -50.0f) == -50);
    dp_f32_to_i8_destroy (obj);
  }

  /* ── steps() matches per-sample step() loop ──────────────────────────── */
  {
    dp_f32_to_i8_state_t *obj_bulk = dp_f32_to_i8_create (128.0f);
    dp_f32_to_i8_state_t *obj_loop = dp_f32_to_i8_create (128.0f);
    DP_CHECK (obj_bulk && obj_loop);

    float  input[64];
    int8_t out_bulk[64], out_loop[64];
    for (int i = 0; i < 64; i++)
      input[i] = -1.5f + 3.0f * i / 63.0f;

    dp_f32_to_i8_steps (obj_bulk, input, out_bulk, 64);
    for (int i = 0; i < 64; i++)
      out_loop[i] = dp_f32_to_i8_step (obj_loop, input[i]);

    DP_CHECK (memcmp (out_bulk, out_loop, 64 * sizeof (int8_t)) == 0);
    dp_f32_to_i8_destroy (obj_bulk);
    dp_f32_to_i8_destroy (obj_loop);
  }

  /* ── reset preserves scale ───────────────────────────────────────────── */
  {
    dp_f32_to_i8_state_t *obj = dp_f32_to_i8_create (128.0f);
    DP_CHECK (obj != NULL);
    dp_f32_to_i8_reset (obj);
    DP_CHECK (dp_f32_to_i8_step (obj, 0.5f) == 64);
    dp_f32_to_i8_destroy (obj);
  }

  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_f32_to_i8_state_t *a = dp_f32_to_i8_create (128.0f);
    dp_f32_to_i8_state_t *b = dp_f32_to_i8_create (128.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_f32_to_i8_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (dp_f32_to_i8, a, b);
    DP_CHECK (b->clipped == a->clipped);
    dp_f32_to_i8_destroy (a);
    dp_f32_to_i8_destroy (b);
  }

  DP_TEST_END ("test_f32_to_i8_core");
}
