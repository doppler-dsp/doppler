#include "doppler/f32_to_i32/f32_to_i32_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  /* ── Invalid args → NULL ──────────────────────────────────────────────── */
  DP_CHECK (dp_f32_to_i32_create (0.0f) == NULL);
  DP_CHECK (dp_f32_to_i32_create (-1.0f) == NULL);

  /* ── Saturation ───────────────────────────────────────────────────────── */
  {
    dp_f32_to_i32_state_t *obj = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (obj != NULL);
    /* +1.0 * 2^31 is 2147483648 -- one PAST INT32_MAX by construction. The
       float spelling of the clamp bound cannot catch this: 2147483647.0f
       rounds up to 2^31, so fminf() would pass it straight through and the
       following lround() would overflow. The double clamp is what makes
       this line hold. */
    DP_CHECK (dp_f32_to_i32_step (obj, 1.0f) == 2147483647);
    DP_CHECK (dp_f32_to_i32_step (obj, -1.0f) == -2147483648); /* exact fit */
    DP_CHECK (dp_f32_to_i32_step (obj, 2.0f) == 2147483647);
    DP_CHECK (dp_f32_to_i32_step (obj, -2.0f) == -2147483648);
    DP_CHECK (dp_f32_to_i32_step (obj, 0.0f) == 0);
    dp_f32_to_i32_destroy (obj);
  }

  /* ── The sticky clip flag fires on +1.0 and NOT on -1.0 ──────────────── */
  {
    dp_f32_to_i32_state_t *obj = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (obj != NULL);
    (void)dp_f32_to_i32_step (obj, -1.0f);
    DP_CHECK (obj->clipped == 0);
    (void)dp_f32_to_i32_step (obj, 1.0f);
    DP_CHECK (obj->clipped == 1);
    dp_f32_to_i32_reset (obj);
    DP_CHECK (obj->clipped == 0);
    dp_f32_to_i32_destroy (obj);
  }

  /* ── Dyadic inputs land exactly on the code grid ─────────────────────── */
  {
    dp_f32_to_i32_state_t *obj = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_f32_to_i32_step (obj, 0.5f) == 1073741824);
    DP_CHECK (dp_f32_to_i32_step (obj, 0.25f) == 536870912);
    DP_CHECK (dp_f32_to_i32_step (obj, -0.5f) == -1073741824);
    dp_f32_to_i32_destroy (obj);
  }

  /* ── Round-to-nearest, NOT truncation ────────────────────────────────── */
  {
    dp_f32_to_i32_state_t *obj = dp_f32_to_i32_create (1.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_f32_to_i32_step (obj, 0.5f) == 1);   /* truncation gives 0 */
    DP_CHECK (dp_f32_to_i32_step (obj, -0.5f) == -1); /* truncation gives 0 */
    DP_CHECK (dp_f32_to_i32_step (obj, 100.4f) == 100);
    dp_f32_to_i32_destroy (obj);
  }

  /* ── steps() matches per-sample step() loop ──────────────────────────── */
  {
    dp_f32_to_i32_state_t *obj_bulk = dp_f32_to_i32_create (2147483648.0f);
    dp_f32_to_i32_state_t *obj_loop = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (obj_bulk && obj_loop);

    float   input[64];
    int32_t out_bulk[64], out_loop[64];
    for (int i = 0; i < 64; i++)
      input[i] = -1.5f + 3.0f * i / 63.0f;

    dp_f32_to_i32_steps (obj_bulk, input, out_bulk, 64);
    for (int i = 0; i < 64; i++)
      out_loop[i] = dp_f32_to_i32_step (obj_loop, input[i]);

    DP_CHECK (memcmp (out_bulk, out_loop, 64 * sizeof (int32_t)) == 0);
    dp_f32_to_i32_destroy (obj_bulk);
    dp_f32_to_i32_destroy (obj_loop);
  }

  /* ── reset preserves scale ───────────────────────────────────────────── */
  {
    dp_f32_to_i32_state_t *obj = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (obj != NULL);
    dp_f32_to_i32_reset (obj);
    DP_CHECK (dp_f32_to_i32_step (obj, 0.5f) == 1073741824);
    dp_f32_to_i32_destroy (obj);
  }

  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_f32_to_i32_state_t *a = dp_f32_to_i32_create (2147483648.0f);
    dp_f32_to_i32_state_t *b = dp_f32_to_i32_create (2147483648.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_f32_to_i32_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (dp_f32_to_i32, a, b);
    DP_CHECK (b->clipped == a->clipped);
    dp_f32_to_i32_destroy (a);
    dp_f32_to_i32_destroy (b);
  }

  DP_TEST_END ("test_f32_to_i32_core");
}
