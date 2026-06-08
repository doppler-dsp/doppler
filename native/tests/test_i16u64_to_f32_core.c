#include "i16u64_to_f32/i16u64_to_f32_core.h"
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

static inline int
_feq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}

int
main (void)
{
  int _fails = 0;

  CHECK (i16u64_to_f32_create (0.0f) == NULL);
  CHECK (i16u64_to_f32_create (-1.0f) == NULL);

  {
    i16u64_to_f32_state_t *obj = i16u64_to_f32_create (32768.0f);
    CHECK (obj != NULL);
    CHECK (_feq (i16u64_to_f32_step (obj, 0x0000000000007FFFull),
                 32767.0f / 32768.0f, 1e-6f));
    CHECK (
        _feq (i16u64_to_f32_step (obj, 0x0000000000008000ull), -1.0f, 1e-6f));
    CHECK (_feq (i16u64_to_f32_step (obj, 0ull), 0.0f, 1e-7f));
    /* Upper 48 bits must be ignored */
    float a = i16u64_to_f32_step (obj, 0x0000000000007FFFull);
    float b = i16u64_to_f32_step (obj, 0xDEADBEEFCAFE7FFFull);
    CHECK (_feq (a, b, 1e-7f));
    i16u64_to_f32_destroy (obj);
  }

  /* ── steps() matches per-sample loop ─────────────────────────────── */
  {
    i16u64_to_f32_state_t *oa = i16u64_to_f32_create (32768.0f);
    i16u64_to_f32_state_t *ob = i16u64_to_f32_create (32768.0f);
    CHECK (oa && ob);
    uint64_t input[32];
    float    bulk[32], loop[32];
    for (int i = 0; i < 32; i++)
      input[i] = (uint64_t)(i * 1024);
    i16u64_to_f32_steps (oa, input, bulk, 32);
    for (int i = 0; i < 32; i++)
      loop[i] = i16u64_to_f32_step (ob, input[i]);
    CHECK (memcmp (bulk, loop, 32 * sizeof (float)) == 0);
    i16u64_to_f32_destroy (oa);
    i16u64_to_f32_destroy (ob);
  }

  if (_fails)
    {
      fprintf (stderr, "test_i16u64_to_f32_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_i16u64_to_f32_core PASSED\n");
  return 0;
}
