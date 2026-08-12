#include "dp_test.h"
#include "i16u64_to_f32/i16u64_to_f32_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  DP_CHECK (i16u64_to_f32_create (0.0f) == NULL);
  DP_CHECK (i16u64_to_f32_create (-1.0f) == NULL);

  {
    i16u64_to_f32_state_t *obj = i16u64_to_f32_create (32768.0f);
    DP_CHECK (obj != NULL);
    DP_CHECK (dp_nearf (i16u64_to_f32_step (obj, 0x0000000000007FFFull),
                        32767.0f / 32768.0f, 1e-6f));
    DP_CHECK (dp_nearf (i16u64_to_f32_step (obj, 0x0000000000008000ull), -1.0f,
                        1e-6f));
    DP_CHECK (dp_nearf (i16u64_to_f32_step (obj, 0ull), 0.0f, 1e-7f));
    /* Upper 48 bits must be ignored */
    float a = i16u64_to_f32_step (obj, 0x0000000000007FFFull);
    float b = i16u64_to_f32_step (obj, 0xDEADBEEFCAFE7FFFull);
    DP_CHECK (dp_nearf (a, b, 1e-7f));
    i16u64_to_f32_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    i16u64_to_f32_state_t *oa = i16u64_to_f32_create (32768.0f);
    i16u64_to_f32_state_t *ob = i16u64_to_f32_create (32768.0f);
    DP_CHECK (oa && ob);
    uint64_t input[32];
    float    bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = (uint64_t)(i * 1024);
    i16u64_to_f32_steps (oa, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = i16u64_to_f32_step (ob, input[i]);
    DP_CHECK (memcmp (bulk, loop, 32 * sizeof (float)) == 0);
    i16u64_to_f32_destroy (oa);
    i16u64_to_f32_destroy (ob);
  }

  DP_TEST_END ("test_i16u64_to_f32_core");
}
