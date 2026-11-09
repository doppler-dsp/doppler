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

#include "doppler/hbdecim/hbdecim_r2c_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <stddef.h>
#include <stdint.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 4-tap symmetric halfband FIR branch. */
static const float H4[4] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

int
main (void)
{
  /* ── lifecycle ──────────────────────────────────────────────────────── */
  hbdecim_r2c_state_t *obj = dp_hbdecim_r2c_create (4, H4);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;
  DP_CHECK (dp_hbdecim_r2c_create (4, NULL) == NULL); /* NULL taps rejected */
  /* A tap count too large to size is refused: 2^(w-2) float taps is 2^w
     bytes, which used to wrap to a 0-byte copy create() then wrote every
     tap into. obj above is the positive control: the same taps at 4. */
  DP_CHECK (dp_hbdecim_r2c_create (SIZE_MAX / sizeof (float) + 1u, H4)
            == NULL);
  dp_hbdecim_r2c_destroy (obj);

  /* ── serializable state round-trip + reject ─────────────────────────── */
  {
    const size_t    L = 512, cut = 157, CAP = 512;
    float          *in   = malloc (L * sizeof (float));
    float _Complex *outA = malloc (CAP * sizeof (float _Complex));
    float _Complex *outB = malloc (CAP * sizeof (float _Complex));
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.05 * (double)i);

    hbdecim_r2c_state_t *ra = dp_hbdecim_r2c_create (4, H4);
    size_t               nA = dp_hbdecim_r2c_execute (ra, in, L, outA, CAP);
    dp_hbdecim_r2c_destroy (ra);

    hbdecim_r2c_state_t *r1 = dp_hbdecim_r2c_create (4, H4);
    size_t               nB = dp_hbdecim_r2c_execute (r1, in, cut, outB, CAP);
    size_t               sb = dp_hbdecim_r2c_state_bytes (r1);
    void                *blob = malloc (sb);
    dp_hbdecim_r2c_get_state (r1, blob);
    dp_hbdecim_r2c_destroy (r1);

    hbdecim_r2c_state_t *r2 = dp_hbdecim_r2c_create (4, H4);
    DP_CHECK (dp_hbdecim_r2c_set_state (r2, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF; /* clobber envelope -> reject */
    DP_CHECK (dp_hbdecim_r2c_set_state (r2, blob) == DP_ERR_INVALID);
    ((char *)blob)[0] ^= (char)0xFF;
    nB += dp_hbdecim_r2c_execute (r2, in + cut, L - cut, outB + nB, CAP - nB);
    dp_hbdecim_r2c_destroy (r2);
    free (blob);

    DP_CHECK (nA == nB);
    for (size_t i = 0; i < nA && i < nB; i++)
      DP_CHECK (crealf (outA[i]) == crealf (outB[i])
                && cimagf (outA[i]) == cimagf (outB[i]));
    free (in);
    free (outA);
    free (outB);
  }

  /* Two instances in the same logical state give the same blob
     (doppler#2642): one held an even sample across a call and consumed it,
     the other never held one; one reset after a held sample, the other is
     fresh. A consumed or reset pending sample is zeroed, as hbdecim does. */
  {
    float in[8] = { 0.5f, -0.25f, 0.75f, 1.0f, -0.5f, 0.125f, 0.3f, -0.7f };
    float _Complex out[8];
    hbdecim_r2c_state_t *a = dp_hbdecim_r2c_create (4, H4);
    hbdecim_r2c_state_t *b = dp_hbdecim_r2c_create (4, H4);
    (void)dp_hbdecim_r2c_execute (a, in, 3, out, 8); /* holds in[2] */
    (void)dp_hbdecim_r2c_execute (a, in + 3, 5, out, 8);
    (void)dp_hbdecim_r2c_execute (b, in, 8, out, 8);
    size_t         sb = dp_hbdecim_r2c_state_bytes (a);
    unsigned char *ba = malloc (sb), *bb = malloc (sb);
    dp_hbdecim_r2c_get_state (a, ba);
    dp_hbdecim_r2c_get_state (b, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);

    (void)dp_hbdecim_r2c_execute (a, in, 3, out, 8); /* holds in[2] */
    dp_hbdecim_r2c_reset (a);
    hbdecim_r2c_state_t *c = dp_hbdecim_r2c_create (4, H4);
    dp_hbdecim_r2c_get_state (a, ba);
    dp_hbdecim_r2c_get_state (c, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);
    free (ba);
    free (bb);
    dp_hbdecim_r2c_destroy (a);
    dp_hbdecim_r2c_destroy (b);
    dp_hbdecim_r2c_destroy (c);
  }

  /* A short out drops nothing (doppler#2643): every call stops at a pair
     boundary, consumed() says how much it took, and re-offering the rest
     gives output identical to one whole call, the fs/4 parity included.
     Capacities cycle through 0, 1, 2 and 3 over an odd-length input. */
  {
    enum
    {
      L = 101
    };
    float in[L];
    float _Complex ref[L], got[L];
    for (size_t i = 0; i < L; i++)
      in[i] = (float)cos (0.37 * (double)i);
    hbdecim_r2c_state_t *a    = dp_hbdecim_r2c_create (4, H4);
    size_t               nref = dp_hbdecim_r2c_execute (a, in, L, ref, L);
    DP_CHECK (dp_hbdecim_r2c_consumed (a) == L);
    hbdecim_r2c_state_t *b   = dp_hbdecim_r2c_create (4, H4);
    size_t               off = 0, ngot = 0, k = 0, calls = 0;
    while (off < L && calls++ < 4 * L)
      {
        size_t cap = (k++) % 4;
        ngot += dp_hbdecim_r2c_execute (b, in + off, L - off, got + ngot, cap);
        off += dp_hbdecim_r2c_consumed (b);
      }
    DP_CHECK (off == L);
    DP_CHECK (ngot == nref);
    DP_CHECK (memcmp (ref, got, nref * sizeof *ref) == 0);
    dp_hbdecim_r2c_destroy (a);
    dp_hbdecim_r2c_destroy (b);
  }

  /* ---------------------------------------------------------------- *
   * #2142: a forged head, flag or parity is refused, the object untouched *
   * ---------------------------------------------------------------- */
  {
    /* Blob: [hdr][even_head][odd_head][has_pending int][pending f32]
       [parity int]... parity is toggled with ^= 1, so a 2 would never
       alternate and the fs/4 flip would stop. The struct is private, so
       the head is forged past any capacity. */
    hbdecim_r2c_state_t *r = dp_hbdecim_r2c_create (4, H4);
    DP_CHECK (r != NULL);
    const size_t eh  = sizeof (dp_state_hdr_t);
    const size_t oh  = eh + sizeof (size_t);
    const size_t hp  = oh + sizeof (size_t);
    const size_t par = hp + sizeof (int) + sizeof (float);
    const size_t far = SIZE_MAX;
    const int    two = 2;
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, eh, &far, sizeof far);
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, oh, &far, sizeof far);
    /* head == cap is the first index past the ring, not only SIZE_MAX. cap
       is the smallest power of two >= num_taps, so 4 for the 4-tap H4. */
    const size_t capv = 4;
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, eh, &capv, sizeof capv);
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, oh, &capv, sizeof capv);
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, hp, &two, sizeof two);
    DP_STATE_FORGE_TEST (dp_hbdecim_r2c, r, HBDECIM_R2C_STATE_MAGIC,
                         HBDECIM_R2C_STATE_VERSION, par, &two, sizeof two);
    /* ...and the last index inside the ring is a state. */
    const size_t last = capv - 1;
    DP_STATE_ACCEPT_TEST (dp_hbdecim_r2c, r, eh, &last, sizeof last);
    DP_STATE_ACCEPT_TEST (dp_hbdecim_r2c, r, oh, &last, sizeof last);
    dp_hbdecim_r2c_destroy (r);
  }

  DP_TEST_END ("test_hbdecim_r2c_core");
}
