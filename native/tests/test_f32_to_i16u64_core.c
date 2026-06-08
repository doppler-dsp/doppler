#include "f32_to_i16u64/f32_to_i16u64_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

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

  CHECK (f32_to_i16u64_create (0.0f) == NULL);
  CHECK (f32_to_i16u64_create (-1.0f) == NULL);

  {
    f32_to_i16u64_state_t *obj = f32_to_i16u64_create (32768.0f);
    CHECK (obj != NULL);
    CHECK (f32_to_i16u64_step (obj, 1.0f) == 0x0000000000007FFFull);
    CHECK (f32_to_i16u64_step (obj, -1.0f) == 0x0000000000008000ull);
    CHECK (f32_to_i16u64_step (obj, 0.0f) == 0ull);
    /* Upper 48 bits always zero */
    CHECK ((f32_to_i16u64_step (obj, 2.0f) & 0xFFFFFFFFFFFF0000ull) == 0ull);
    CHECK ((f32_to_i16u64_step (obj, -2.0f) & 0xFFFFFFFFFFFF0000ull) == 0ull);
    f32_to_i16u64_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    f32_to_i16u64_state_t *a = f32_to_i16u64_create (32768.0f);
    f32_to_i16u64_state_t *b = f32_to_i16u64_create (32768.0f);
    CHECK (a && b);
    float    input[32];
    uint64_t bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = -1.5f + 3.0f * i / 31.0f;
    f32_to_i16u64_steps (a, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = f32_to_i16u64_step (b, input[i]);
    CHECK (memcmp (bulk, loop, 32 * sizeof (uint64_t)) == 0);
    f32_to_i16u64_destroy (a);
    f32_to_i16u64_destroy (b);
  }

  if (_fails)
    {
      fprintf (stderr, "test_f32_to_i16u64_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_f32_to_i16u64_core PASSED\n");
  return 0;
}
