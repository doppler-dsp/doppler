/**
 * @file test_hbdecim_r2c_core.c
 * @brief Unit tests for hbdecim_r2c_state_t — real-input halfband R2C
 * decimator.
 *
 * Covers lifecycle plus the serializable bytes-interface gate: a mid-stream
 * split, hand the delay-line state to a fresh decimator, and resume
 * bit-for-bit; a magic-clobbered blob is rejected.  (The R2C core is also
 * exercised transitively by ddcr's round-trip; this is its direct, standalone
 * test.)
 */

#include "hbdecim/hbdecim_r2c_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int _fails = 0;

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

/* 4-tap symmetric halfband FIR branch. */
static const float H4[4] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

int
main (void)
{
  /* ── lifecycle ──────────────────────────────────────────────────────── */
  hbdecim_r2c_state_t *obj = hbdecim_r2c_create (4, H4);
  CHECK (obj != NULL);
  if (!obj)
    return 1;
  CHECK (hbdecim_r2c_create (4, NULL) == NULL); /* NULL taps rejected */
  hbdecim_r2c_destroy (obj);

  /* ── serializable state round-trip + reject ─────────────────────────── */
  {
    const size_t    L = 512, cut = 157, CAP = 512;
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.05 * (double)i);

    hbdecim_r2c_state_t *ra = hbdecim_r2c_create (4, H4);
    size_t               nA = hbdecim_r2c_execute (ra, in, L, outA, CAP);
    hbdecim_r2c_destroy (ra);

    hbdecim_r2c_state_t *r1   = hbdecim_r2c_create (4, H4);
    size_t               nB   = hbdecim_r2c_execute (r1, in, cut, outB, CAP);
    size_t               sb   = hbdecim_r2c_state_bytes (r1);
    void                *blob = malloc (sb);
    hbdecim_r2c_get_state (r1, blob);
    hbdecim_r2c_destroy (r1);

    hbdecim_r2c_state_t *r2 = hbdecim_r2c_create (4, H4);
    CHECK (hbdecim_r2c_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    CHECK (hbdecim_r2c_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += hbdecim_r2c_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
    hbdecim_r2c_destroy (r2);
    free (blob);

    CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      CHECK (crealf (outA[i]) == crealf (outB[i])
             && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }

  if (_fails)
    {
      fprintf (stderr, "test_hbdecim_r2c_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_hbdecim_r2c_core PASSED\n");
  return 0;
}
