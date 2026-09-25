/**
 * ring_element_view_demo.c — one element per sample, on every width.
 *
 * The ring stores SCALARS, two per complex sample, and dp_f32_wait()
 * returns a `float *`. A caller that thinks in samples would rather have
 * one ELEMENT per sample, and the `_view` functions are that: the same
 * four calls, the same memory, the same counts -- typed as the element.
 *
 *     f32   float _Complex
 *     f64   double _Complex
 *     i16   dp_iq16_t  { int16_t i, q; }
 *
 * C has no complex integer, so the 16-bit element is a two-field record.
 * It is the same 4 bytes an ADC delivers (I, Q, I, Q, ...), so a capture
 * buffer can be handed over with a cast and no copy.
 *
 * Each is a cast and nothing more: every count in buffer.h is already in
 * samples, so the two faces cannot disagree about a length.
 *
 * Build:
 *   make build
 *   ./build/native/examples/ring_element_view_demo
 */
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/i16_buffer/i16_buffer_core.h"
#include <stdio.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\\n", __FILE__, __LINE__, #cond);   \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

int
main (void)
{
  /* ── complex float: write elements, read elements ─────────────────── */
  dp_f32_t *f = dp_f32_create (1024);
  CHECK (f != NULL);

  float _Complex tone[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
  CHECK (dp_f32_write_view (f, tone, 4)); /* 4 SAMPLES, not 8 floats */
  CHECK (dp_f32_available (f) == 4);

  float _Complex *v   = dp_f32_peek_view (f, 4);
  float          *raw = dp_f32_peek (f, 4);
  CHECK (v != NULL && (void *)v == (void *)raw); /* one memory, two types */
  CHECK (v[2] == 3.0f && raw[2 * 2] == 3.0f && raw[2 * 2 + 1] == 0.0f);
  dp_f32_consume (f, 4);
  dp_f32_destroy (f);

  /* ── 16-bit I/Q: an ADC buffer goes in with a cast ────────────────── */
  dp_i16_t *q = dp_i16_create (1024);
  CHECK (q != NULL);

  int16_t adc[6] = { 10, 11, 20, 21, 30, 31 }; /* I, Q, I, Q, I, Q */
  CHECK (dp_i16_write_some_view (q, (const dp_iq16_t *)adc, 3) == 3);

  dp_iq16_t *s = dp_i16_wait_view (q, 3); /* already there: returns at once */
  CHECK (s != NULL);
  CHECK (s[0].i == 10 && s[0].q == 11);
  CHECK (s[2].i == 30 && s[2].q == 31);
  dp_i16_consume (q, 3);
  dp_i16_destroy (q);

  printf ("element view: f32 as float _Complex, i16 as {i, q} -- "
          "same memory as the scalar face, counts in samples\n");
  return 0;
}
