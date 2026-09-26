#include "doppler/i16u64_to_f32/i16u64_to_f32_core.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  DP_CHECK (dp_i16u64_to_f32_create (0.0f) == NULL);
  DP_CHECK (dp_i16u64_to_f32_create (-1.0f) == NULL);

  {
    dp_i16u64_to_f32_state_t *obj = dp_i16u64_to_f32_create (32768.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_nearf (dp_i16u64_to_f32_step (obj, 0x0000000000007FFFull),
                        32767.0f / 32768.0f, 1e-6f));
    DP_CHECK (dp_nearf (dp_i16u64_to_f32_step (obj, 0x0000000000008000ull),
                        -1.0f, 1e-6f));
    DP_CHECK (dp_nearf (dp_i16u64_to_f32_step (obj, 0ull), 0.0f, 1e-7f));
    /* Upper 48 bits must be ignored */
    float a = dp_i16u64_to_f32_step (obj, 0x0000000000007FFFull);
    float b = dp_i16u64_to_f32_step (obj, 0xDEADBEEFCAFE7FFFull);
    DP_CHECK (dp_nearf (a, b, 1e-7f));
    dp_i16u64_to_f32_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    dp_i16u64_to_f32_state_t *oa = dp_i16u64_to_f32_create (32768.0f);
    dp_i16u64_to_f32_state_t *ob = dp_i16u64_to_f32_create (32768.0f);
    DP_CHECK (oa && ob);
    uint64_t input[32];
    float    bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = (uint64_t)(i * 1024);
    dp_i16u64_to_f32_steps (oa, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = dp_i16u64_to_f32_step (ob, input[i]);
    DP_CHECK (memcmp (bulk, loop, 32 * sizeof (float)) == 0);
    dp_i16u64_to_f32_destroy (oa);
    dp_i16u64_to_f32_destroy (ob);
  }

  DP_TEST_END ("test_i16u64_to_f32_core");
}
