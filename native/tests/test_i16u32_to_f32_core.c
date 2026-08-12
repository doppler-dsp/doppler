#include "dp_test.h"
#include "i16u32_to_f32/i16u32_to_f32_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  DP_CHECK (i16u32_to_f32_create (0.0f) == NULL);
  DP_CHECK (i16u32_to_f32_create (-1.0f) == NULL);

  {
    i16u32_to_f32_state_t *obj = i16u32_to_f32_create (32768.0f);
    DP_CHECK (obj != NULL);
    /* 0x00007FFF = int16 +32767 */
    DP_CHECK (dp_nearf (i16u32_to_f32_step (obj, 0x00007FFFu),
                        32767.0f / 32768.0f, 1e-6f));
    /* 0x00008000 = int16 -32768 (two's complement) */
    DP_CHECK (dp_nearf (i16u32_to_f32_step (obj, 0x00008000u), -1.0f, 1e-6f));
    DP_CHECK (dp_nearf (i16u32_to_f32_step (obj, 0u), 0.0f, 1e-7f));
    /* Upper 16 bits must be ignored */
    float a = i16u32_to_f32_step (obj, 0x00007FFFu);
    float b = i16u32_to_f32_step (obj, 0xDEAD7FFFu);
    DP_CHECK (dp_nearf (a, b, 1e-7f));
    i16u32_to_f32_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    i16u32_to_f32_state_t *oa = i16u32_to_f32_create (32768.0f);
    i16u32_to_f32_state_t *ob = i16u32_to_f32_create (32768.0f);
    DP_CHECK (oa && ob);
    uint32_t input[32];
    float    bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = (uint32_t)(i * 1024);
    i16u32_to_f32_steps (oa, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = i16u32_to_f32_step (ob, input[i]);
    DP_CHECK (memcmp (bulk, loop, 32 * sizeof (float)) == 0);
    i16u32_to_f32_destroy (oa);
    i16u32_to_f32_destroy (ob);
  }

  DP_TEST_END ("test_i16u32_to_f32_core");
}
