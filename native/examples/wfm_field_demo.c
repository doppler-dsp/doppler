/**
 * wfm_field_demo.c — bits, written as text: the Field grammar from C.
 *
 * Every place wfmgen takes a run of bits — `--bits`, `--sync`, `--acq-code`,
 * `--data-code`, a scene's "spec", Python's field_bits() — reads it with ONE
 * parser, dp_wfm_field_parse(). This is that parser's C face, and the twin of
 * src/doppler/examples/wfm_field_demo.py.
 *
 * What this demonstrates, in order:
 *
 *   1. A Field is text in, bits out: size with a NULL buffer, then render.
 *   2. Two spellings of one Field are the same Field, and it has ONE
 *      canonical text, which dp_wfm_field_format() writes and which parses
 *      back to the same field.
 *   3. A repetition is the same bits again, never a fresh draw.
 *   4. A Field outside the grammar is REFUSED with a sentence naming the
 *      rule — a stray character, a seed wider than its register, a length
 *      past the Field bound — and nothing is built.
 *
 * Every check is explicit and returns non-zero on failure; `assert()` is not
 * used, because examples build Release and NDEBUG would compile the checks
 * out.
 *
 * Build:
 *   make build && ./build/native/examples/wfm_field_demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doppler/wfm/wfm_frame.h"

/* A Field's bits: the size, then the render. The caller frees. */
static uint8_t *
bits_of (const char *spec, size_t *n)
{
  const char *why = NULL;
  *n              = dp_wfm_field_bits (spec, NULL, 0, &why);
  if (*n == 0)
    {
      fprintf (stderr, "  %s: %s\n", spec, why);
      return NULL;
    }
  uint8_t *b = malloc (*n);
  if (!b || dp_wfm_field_bits (spec, b, *n, NULL) != *n)
    {
      free (b);
      return NULL;
    }
  return b;
}

int
main (void)
{
  /* 1. Text in, bits out. */
  size_t   n;
  uint8_t *b = bits_of ("0xA5", &n);
  if (!b || n != 8)
    return 1;
  printf ("0xA5 ->");
  for (size_t i = 0; i < n; i++)
    printf (" %u", b[i]);
  printf ("\n"); /* 0xA5 -> 1 0 1 0 0 1 0 1, most significant first */
  free (b);

  /* 2. One canonical text: the defaults and the radix are spelling. */
  wfm_field_t f;
  uint8_t    *owned = NULL; /* a literal's bits; NULL for a generator */
  if (dp_wfm_field_parse ("pn:0x1f:5:0:0:galois", &f, &owned, NULL) != 0)
    return 1;
  char text[64];
  dp_wfm_field_format (&f, text, sizeof text);
  printf ("pn:0x1f:5:0:0:galois is written %s\n", text);
  if (strcmp (text, "pn:31:5") != 0)
    return 1;

  /* 3. *REPS repeats the SAME bits: one period, four times. */
  size_t   n1, n4;
  uint8_t *one  = bits_of ("pn:31:5", &n1);
  uint8_t *four = bits_of ("pn:31:5*4", &n4);
  if (!one || !four || n4 != 4 * n1)
    return 1;
  for (size_t r = 0; r < 4; r++)
    if (memcmp (four + r * n1, one, n1) != 0)
      return 1;
  printf ("pn:31:5*4 is pn:31:5 four times over (%zu bits)\n", n4);
  free (one);
  free (four);

  /* 4. Refused, never repaired: each names its rule. */
  static const char *const bad[] = { "01a1", "pn:31:5:32", "pn:4000000000:5" };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    {
      const char *why = NULL;
      if (dp_wfm_field_parse (bad[i], &f, &owned, &why) == 0 || !why)
        return 1;
      printf ("%-16s refused: %s\n", bad[i], why);
    }
  return 0;
}
