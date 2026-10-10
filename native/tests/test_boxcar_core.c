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
#include "doppler/boxcar/boxcar_core.h"
#include "doppler/dp_complex.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (void)
{

  /* ---------------------------------------------------------------- *
   * 1. Window math: ramp-in then true mean; gain fold                 *
   * ---------------------------------------------------------------- */
  {
    /* len=2 over a constant 1: 0.5 (partial), then 1.0, 1.0. */
    dp_boxcar_state_t *b = dp_boxcar_create (2, 1.0);
    DP_CHECK (b != NULL);
    DP_CHECK (fabsf (crealf (dp_boxcar_step (b, 1.0f)) - 0.5f) < 1e-6f);
    DP_CHECK (fabsf (crealf (dp_boxcar_step (b, 1.0f)) - 1.0f) < 1e-6f);
    DP_CHECK (fabsf (crealf (dp_boxcar_step (b, 1.0f)) - 1.0f) < 1e-6f);
    dp_boxcar_destroy (b);

    /* gain=2 folds into the mean: 1.0, 2.0, 2.0. */
    dp_boxcar_state_t *g = dp_boxcar_create (2, 2.0);
    DP_CHECK (fabsf (crealf (dp_boxcar_step (g, 1.0f)) - 1.0f) < 1e-6f);
    DP_CHECK (fabsf (crealf (dp_boxcar_step (g, 1.0f)) - 2.0f) < 1e-6f);
    dp_boxcar_destroy (g);
  }

  /* ---------------------------------------------------------------- *
   * 2. Sliding window across the ring wrap (len=3, ramp input)        *
   * ---------------------------------------------------------------- */
  {
    dp_boxcar_state_t *b = dp_boxcar_create (3, 1.0);
    /* feed 1,2,3,4,5 -> means: 1/3, 3/3, 6/3, 9/3, 12/3 */
    float exp[5] = { 1.0f / 3, 1.0f, 2.0f, 3.0f, 4.0f };
    for (int k = 0; k < 5; k++)
      {
        float y = crealf (dp_boxcar_step (b, (float)(k + 1)));
        DP_CHECK (fabsf (y - exp[k]) < 1e-5f);
      }
    dp_boxcar_destroy (b);
  }

  /* ---------------------------------------------------------------- *
   * 3. Lifecycle: init==create parity, clamp/reject, reset, set_gain  *
   * ---------------------------------------------------------------- */
  {
    dp_boxcar_state_t v;
    dp_boxcar_init (&v, 4, 1.5);
    DP_CHECK (v.len == 4);
    DP_CHECK (fabs (v.gain - 1.5) < 1e-12);
    DP_CHECK (fabsf (v.scale - (float)(1.5 / 4.0)) < 1e-9f);

    dp_boxcar_state_t *c = dp_boxcar_create (4, 1.5);
    DP_CHECK (c != NULL && c->len == v.len && c->scale == v.scale);
    dp_boxcar_destroy (c);

    /* length must fit the fixed ring */
    DP_CHECK (dp_boxcar_create (0, 1.0) == NULL);
    DP_CHECK (dp_boxcar_create (BOXCAR_MAX_LEN + 1, 1.0) == NULL);
    dp_boxcar_state_t *mx = dp_boxcar_create (BOXCAR_MAX_LEN, 1.0);
    DP_CHECK (mx != NULL);
    dp_boxcar_destroy (mx);

    /* set_gain refreshes the cached scale */
    dp_boxcar_set_gain (&v, 8.0);
    DP_CHECK (fabs (dp_boxcar_get_gain (&v) - 8.0) < 1e-12);
    DP_CHECK (fabsf (v.scale - (float)(8.0 / 4.0)) < 1e-9f);

    /* reset clears the window but keeps len/gain */
    dp_boxcar_step (&v, 3.0f + 1.0f * I);
    dp_boxcar_reset (&v);
    DP_CHECK (crealf (v.acc) == 0.0f && cimagf (v.acc) == 0.0f && v.pos == 0);
    DP_CHECK (v.len == 4 && fabs (v.gain - 8.0) < 1e-12);
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
    float _Complex *in   = malloc (L * sizeof (*in));
    float _Complex *outA = malloc (CAP * sizeof (*outA));
    float _Complex *outB = malloc (CAP * sizeof (*outB));
    for (int i = 0; i < L; i++)
      in[i] = cosf (0.03f * i) + I * sinf (0.017f * i);

    dp_boxcar_state_t *a = dp_boxcar_create (7, 1.25);
    dp_boxcar_steps (a, in, outA, L);
    dp_boxcar_destroy (a);

    dp_boxcar_state_t *r1 = dp_boxcar_create (7, 1.25);
    dp_boxcar_steps (r1, in, outB, CUT);
    size_t sb   = dp_boxcar_state_bytes (r1);
    void  *blob = malloc (sb);
    dp_boxcar_get_state (r1, blob);
    dp_boxcar_destroy (r1);

    dp_boxcar_state_t *r2 = dp_boxcar_create (7, 1.25);
    DP_CHECK (dp_boxcar_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (dp_boxcar_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    dp_boxcar_steps (r2, in + CUT, outB + CUT, L - CUT);
    dp_boxcar_destroy (r2);
    free (blob);

    for (int i = 0; i < L; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }

  /* ---------------------------------------------------------------- *
   * #2142: a forged index is refused, the object untouched          *
   * ---------------------------------------------------------------- */
  {
    /* dp_boxcar_step writes ring[pos] BEFORE it wraps pos, so a pos at or
       past BOXCAR_MAX_LEN wrote past the struct, and a len above it let pos
       walk there. A pos inside the ring but past len is no state either. */
    dp_boxcar_state_t *b = dp_boxcar_create (4, 1.0);
    DP_CHECK (b != NULL);
    for (int k = 0; k < 6; k++)
      (void)dp_boxcar_step (b, (float)k); /* pos = 2, mid-window */
    const size_t pos_off
        = sizeof (dp_state_hdr_t) + offsetof (dp_boxcar_state_t, pos);
    const size_t len_off
        = sizeof (dp_state_hdr_t) + offsetof (dp_boxcar_state_t, len);
    const size_t bad_pos[] = { BOXCAR_MAX_LEN, 4, SIZE_MAX };
    const size_t bad_len[] = { 0, BOXCAR_MAX_LEN + 1 };
    for (size_t i = 0; i < sizeof bad_pos / sizeof *bad_pos; i++)
      DP_STATE_FORGE_TEST (dp_boxcar, b, BOXCAR_STATE_MAGIC,
                           BOXCAR_STATE_VERSION, pos_off, &bad_pos[i],
                           sizeof bad_pos[i]);
    for (size_t i = 0; i < sizeof bad_len / sizeof *bad_len; i++)
      DP_STATE_FORGE_TEST (dp_boxcar, b, BOXCAR_STATE_MAGIC,
                           BOXCAR_STATE_VERSION, len_off, &bad_len[i],
                           sizeof bad_len[i]);

    /* And the write itself: step after the refused forgery. Were the blob
       taken, this would store ring[64], one element past the struct, which
       `make test-asan` reports as a heap overflow. */
    unsigned char *blob = malloc (dp_boxcar_state_bytes (b));
    DP_CHECK (blob != NULL);
    if (blob)
      {
        dp_boxcar_get_state (b, blob);
        const size_t past = BOXCAR_MAX_LEN;
        memcpy (blob + pos_off, &past, sizeof past);
        DP_CHECK (dp_boxcar_set_state (b, blob) == DP_ERR_INVALID);
        (void)dp_boxcar_step (b, 1.0f);
        free (blob);
      }
    /* The all-zero state is a state: an embedding holds it until its init
       runs. It must round-trip, and stepping it writes ring[0] only. */
    dp_boxcar_state_t zero;
    memset (&zero, 0, sizeof zero);
    unsigned char *zb = malloc (dp_boxcar_state_bytes (&zero));
    DP_CHECK (zb != NULL);
    if (zb)
      {
        dp_boxcar_get_state (&zero, zb);
        DP_CHECK (dp_boxcar_set_state (b, zb) == DP_OK);
        (void)dp_boxcar_step (b, 1.0f);
        DP_CHECK (b->pos == 0);
        free (zb);
      }
    dp_boxcar_destroy (b);
  }

  DP_TEST_END ("test_boxcar_core");
}
