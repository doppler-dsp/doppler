#include "doppler/f32_to_i16u64/f32_to_i16u64_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  DP_CHECK (dp_f32_to_i16u64_create (0.0f) == NULL);
  DP_CHECK (dp_f32_to_i16u64_create (-1.0f) == NULL);

  {
    dp_f32_to_i16u64_state_t *obj = dp_f32_to_i16u64_create (32768.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_f32_to_i16u64_step (obj, 1.0f) == 0x0000000000007FFFull);
    DP_CHECK (dp_f32_to_i16u64_step (obj, -1.0f) == 0x0000000000008000ull);
    DP_CHECK (dp_f32_to_i16u64_step (obj, 0.0f) == 0ull);
    /* Upper 48 bits always zero */
    DP_CHECK ((dp_f32_to_i16u64_step (obj, 2.0f) & 0xFFFFFFFFFFFF0000ull)
              == 0ull);
    DP_CHECK ((dp_f32_to_i16u64_step (obj, -2.0f) & 0xFFFFFFFFFFFF0000ull)
              == 0ull);
    dp_f32_to_i16u64_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    dp_f32_to_i16u64_state_t *a = dp_f32_to_i16u64_create (32768.0f);
    dp_f32_to_i16u64_state_t *b = dp_f32_to_i16u64_create (32768.0f);
    DP_CHECK (a && b);
    float    input[32];
    uint64_t bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = -1.5f + 3.0f * i / 31.0f;
    dp_f32_to_i16u64_steps (a, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = dp_f32_to_i16u64_step (b, input[i]);
    DP_CHECK (memcmp (bulk, loop, 32 * sizeof (uint64_t)) == 0);
    dp_f32_to_i16u64_destroy (a);
    dp_f32_to_i16u64_destroy (b);
  }

  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    dp_f32_to_i16u64_state_t *a = dp_f32_to_i16u64_create (32768.0f);
    dp_f32_to_i16u64_state_t *b = dp_f32_to_i16u64_create (32768.0f);
    DP_CHECK (a != NULL && b != NULL);
    (void)dp_f32_to_i16u64_step (a, 2.0f); /* saturate → clipped = 1 */
    DP_STATE_ROUNDTRIP_TEST (dp_f32_to_i16u64, a, b);
    DP_CHECK (b->clipped == a->clipped);
    dp_f32_to_i16u64_destroy (a);
    dp_f32_to_i16u64_destroy (b);
  }

  DP_TEST_END ("test_f32_to_i16u64_core");
}
