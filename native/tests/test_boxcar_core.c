/**
 * @file test_boxcar_core.c
 * @brief Unit tests for the boxcar (rectangular) moving-average filter.
 *
 * Tests:
 *   1. Window math: ramp-in over a partial window, then a true mean; gain fold
 *   2. Sliding behaviour across the ring wrap
 *   3. Lifecycle: init==create parity, length clamp/reject, reset, set_gain
 *   4. Serialization: mid-stream split resumes bit-for-bit; envelope reject
 */
#include "boxcar/boxcar_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

  /* ---------------------------------------------------------------- *
   * 1. Window math: ramp-in then true mean; gain fold                 *
   * ---------------------------------------------------------------- */
  {
    /* len=2 over a constant 1: 0.5 (partial), then 1.0, 1.0. */
    boxcar_state_t *b = boxcar_create (2, 1.0);
    CHECK (b != NULL);
    CHECK (fabsf (crealf (boxcar_step (b, 1.0f)) - 0.5f) < 1e-6f);
    CHECK (fabsf (crealf (boxcar_step (b, 1.0f)) - 1.0f) < 1e-6f);
    CHECK (fabsf (crealf (boxcar_step (b, 1.0f)) - 1.0f) < 1e-6f);
    boxcar_destroy (b);

    /* gain=2 folds into the mean: 1.0, 2.0, 2.0. */
    boxcar_state_t *g = boxcar_create (2, 2.0);
    CHECK (fabsf (crealf (boxcar_step (g, 1.0f)) - 1.0f) < 1e-6f);
    CHECK (fabsf (crealf (boxcar_step (g, 1.0f)) - 2.0f) < 1e-6f);
    boxcar_destroy (g);
  }

  /* ---------------------------------------------------------------- *
   * 2. Sliding window across the ring wrap (len=3, ramp input)        *
   * ---------------------------------------------------------------- */
  {
    boxcar_state_t *b = boxcar_create (3, 1.0);
    /* feed 1,2,3,4,5 -> means: 1/3, 3/3, 6/3, 9/3, 12/3 */
    float exp[5] = { 1.0f / 3, 1.0f, 2.0f, 3.0f, 4.0f };
    for (int k = 0; k < 5; k++)
      {
        float y = crealf (boxcar_step (b, (float)(k + 1)));
        CHECK (fabsf (y - exp[k]) < 1e-5f);
      }
    boxcar_destroy (b);
  }

  /* ---------------------------------------------------------------- *
   * 3. Lifecycle: init==create parity, clamp/reject, reset, set_gain  *
   * ---------------------------------------------------------------- */
  {
    boxcar_state_t v;
    boxcar_init (&v, 4, 1.5);
    CHECK (v.len == 4);
    CHECK (fabs (v.gain - 1.5) < 1e-12);
    CHECK (fabsf (v.scale - (float)(1.5 / 4.0)) < 1e-9f);

    boxcar_state_t *c = boxcar_create (4, 1.5);
    CHECK (c != NULL && c->len == v.len && c->scale == v.scale);
    boxcar_destroy (c);

    /* length must fit the fixed ring */
    CHECK (boxcar_create (0, 1.0) == NULL);
    CHECK (boxcar_create (BOXCAR_MAX_LEN + 1, 1.0) == NULL);
    boxcar_state_t *mx = boxcar_create (BOXCAR_MAX_LEN, 1.0);
    CHECK (mx != NULL);
    boxcar_destroy (mx);

    /* set_gain refreshes the cached scale */
    boxcar_set_gain (&v, 8.0);
    CHECK (fabs (boxcar_get_gain (&v) - 8.0) < 1e-12);
    CHECK (fabsf (v.scale - (float)(8.0 / 4.0)) < 1e-9f);

    /* reset clears the window but keeps len/gain */
    boxcar_step (&v, 3.0f + 1.0f * I);
    boxcar_reset (&v);
    CHECK (crealf (v.acc) == 0.0f && cimagf (v.acc) == 0.0f && v.pos == 0);
    CHECK (v.len == 4 && fabs (v.gain - 8.0) < 1e-12);
  }

  /* ---------------------------------------------------------------- *
   * 4. Serialization: mid-stream split resumes bit-for-bit; reject    *
   * ---------------------------------------------------------------- */
  {
    enum
    {
      L   = 200,
      CUT = 83,
      CAP = 200
    };
    float complex *in   = malloc (L * sizeof (*in));
    float complex *outA = malloc (CAP * sizeof (*outA));
    float complex *outB = malloc (CAP * sizeof (*outB));
    for (int i = 0; i < L; i++)
      in[i] = cosf (0.03f * i) + I * sinf (0.017f * i);

    boxcar_state_t *a = boxcar_create (7, 1.25);
    boxcar_steps (a, in, outA, L);
    boxcar_destroy (a);

    boxcar_state_t *r1 = boxcar_create (7, 1.25);
    boxcar_steps (r1, in, outB, CUT);
    size_t sb   = boxcar_state_bytes (r1);
    void  *blob = malloc (sb);
    boxcar_get_state (r1, blob);
    boxcar_destroy (r1);

    boxcar_state_t *r2 = boxcar_create (7, 1.25);
    CHECK (boxcar_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (boxcar_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    boxcar_steps (r2, in + CUT, outB + CUT, L - CUT);
    boxcar_destroy (r2);
    free (blob);

    for (int i = 0; i < L; i++)
      CHECK (crealf (outA[i]) == crealf (outB[i])
             && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }

  if (_fails)
    {
      fprintf (stderr, "test_boxcar_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_boxcar_core PASSED\n");
  return 0;
}
