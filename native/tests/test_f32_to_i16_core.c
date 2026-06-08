#include "f32_to_i16/f32_to_i16_core.h"
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

  /* ── Invalid args → NULL ──────────────────────────────────────────────── */
  CHECK (f32_to_i16_create (0.0f) == NULL);
  CHECK (f32_to_i16_create (-1.0f) == NULL);

  /* ── Saturation ───────────────────────────────────────────────────────── */
  {
    f32_to_i16_state_t *obj = f32_to_i16_create (32768.0f);
    CHECK (obj != NULL);
    CHECK (f32_to_i16_step (obj, 1.0f) == 32767);   /* clamp at Q15_MAX */
    CHECK (f32_to_i16_step (obj, -1.0f) == -32768); /* exact fit */
    CHECK (f32_to_i16_step (obj, 2.0f) == 32767);   /* hard saturation */
    CHECK (f32_to_i16_step (obj, -2.0f) == -32768);
    CHECK (f32_to_i16_step (obj, 0.0f) == 0);
    f32_to_i16_destroy (obj);
  }

  /* ── Round-to-nearest: 0.5 LSB = 0.5/32768 ───────────────────────────── */
  {
    f32_to_i16_state_t *obj = f32_to_i16_create (32768.0f);
    CHECK (obj != NULL);
    float half_lsb = 0.5f / 32768.0f;
    CHECK (f32_to_i16_step (obj, half_lsb) == 1);
    CHECK (f32_to_i16_step (obj, -half_lsb) == -1);
    f32_to_i16_destroy (obj);
  }

  /* ── Custom scale (scale=1.0: integers pass through unchanged) ────────── */
  {
    f32_to_i16_state_t *obj = f32_to_i16_create (1.0f);
    CHECK (obj != NULL);
    CHECK (f32_to_i16_step (obj, 100.0f) == 100);
    CHECK (f32_to_i16_step (obj, -50.0f) == -50);
    f32_to_i16_destroy (obj);
  }

  /* ── steps() matches per-sample step() loop ──────────────────────────── */
  {
    f32_to_i16_state_t *obj_bulk = f32_to_i16_create (32768.0f);
    f32_to_i16_state_t *obj_loop = f32_to_i16_create (32768.0f);
    CHECK (obj_bulk && obj_loop);

    float   input[64];
    int16_t out_bulk[64], out_loop[64];
    for (int i = 0; i < 64; i++)
      input[i] = -1.5f + 3.0f * i / 63.0f;

    f32_to_i16_steps (obj_bulk, input, out_bulk, 64);
    for (int i = 0; i < 64; i++)
      out_loop[i] = f32_to_i16_step (obj_loop, input[i]);

    CHECK (memcmp (out_bulk, out_loop, 64 * sizeof (int16_t)) == 0);
    f32_to_i16_destroy (obj_bulk);
    f32_to_i16_destroy (obj_loop);
  }

  /* ── reset is a no-op (no dynamic state) ─────────────────────────────── */
  {
    f32_to_i16_state_t *obj = f32_to_i16_create (32768.0f);
    CHECK (obj != NULL);
    f32_to_i16_reset (obj);
    CHECK (f32_to_i16_step (obj, 0.5f) == 16384);
    f32_to_i16_destroy (obj);
  }

  if (_fails)
    {
      fprintf (stderr, "test_f32_to_i16_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_f32_to_i16_core PASSED\n");
  return 0;
}
