/**
 * @file test_hbdecim_q15_core.c
 * @brief Unit tests for the Q15 halfband decimator (structural / C-level).
 *
 * Frequency-domain and SNR tests live in the Python suite where scipy
 * can generate valid halfband prototypes.  These tests cover lifecycle,
 * zero-input passthrough, decimation ratio, odd-block buffering, and
 * reset behaviour — all verifiable without a filter-design library.
 */
#include "doppler/hbdecim_q15/hbdecim_q15_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Flat impulse response (all ones, num_taps=1): FIR branch is empty
 * (K=0), only the delay center-tap path executes.  Useful for testing
 * the pure-delay branch and the 2:1 output ratio in isolation.        */
static const float H1[1] = { 1.0f };

/* Larger coefficient array for structural tests (25 taps).
 * Values don't form an optimal halfband — only zero-input is tested.  */
static const float H25_stub[25]
    = { 0.01f,  0.0f,   -0.02f, 0.0f,  0.03f,  0.0f,   -0.04f, 0.0f,  0.08f,
        0.0f,   -0.20f, 0.0f,   0.50f, 0.0f,   -0.20f, 0.0f,   0.08f, 0.0f,
        -0.04f, 0.0f,   0.03f,  0.0f,  -0.02f, 0.0f,   0.01f };

int
main (void)
{

  /* ── NULL / bad-arg guards ───────────────────────────────────── */
  DP_CHECK (dp_hbdecim_q15_create (0, H1) == NULL);
  DP_CHECK (dp_hbdecim_q15_create (1, NULL) == NULL);

  /* ── Lifecycle (num_taps=1) ──────────────────────────────────── */
  dp_hbdecim_q15_state_t *r = dp_hbdecim_q15_create (1, H1);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;
  DP_CHECK (dp_hbdecim_q15_get_num_taps (r) == 1);
  DP_CHECK (fabs (dp_hbdecim_q15_get_rate (r) - 0.5) < 1e-9);
  dp_hbdecim_q15_reset (r);
  dp_hbdecim_q15_destroy (r);
  r = NULL;

  /* ── Zero input → zero output ────────────────────────────────── */
  r = dp_hbdecim_q15_create (25, H25_stub);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;

  static const int16_t zeros[512] = { 0 };
  int16_t              out[256];
  size_t               n;

  n = dp_hbdecim_q15_execute (r, zeros, 256, out, 128);
  DP_CHECK (n == 128);
  for (int i = 0; i < 256; i++)
    DP_CHECK (out[i] == 0);

  /* ── 2:1 decimation ratio ────────────────────────────────────── */
  int16_t ramp[2048];
  int16_t ramp_out[1024];
  for (int i = 0; i < 2048; i++)
    ramp[i] = (int16_t)(i & 0x7fff);
  n = dp_hbdecim_q15_execute (r, ramp, 1024, ramp_out, 512);
  DP_CHECK (n == 512);

  /* ── Odd block: trailing even pair buffered, consumed next call ─ */
  dp_hbdecim_q15_reset (r);
  n = dp_hbdecim_q15_execute (r, zeros, 3, out, 64);
  DP_CHECK (n == 1); /* floor(3/2) = 1 complete pair processed */
  n = dp_hbdecim_q15_execute (r, zeros, 1, out, 64);
  DP_CHECK (n == 1); /* buffered pair + 1 new odd = 1 more output   */

  /* ── Execute with empty input ────────────────────────────────── */
  n = dp_hbdecim_q15_execute (r, zeros, 0, out, 64);
  DP_CHECK (n == 0);

  /* ── max_out=0 produces no output ───────────────────────────── */
  n = dp_hbdecim_q15_execute (r, zeros, 128, out, 0);
  DP_CHECK (n == 0);

  /* ── reset clears delay lines (zero after reset + zero input) ── */
  dp_hbdecim_q15_reset (r);
  n = dp_hbdecim_q15_execute (r, zeros, 256, out, 128);
  DP_CHECK (n == 128);
  for (int i = 0; i < 256; i++)
    DP_CHECK (out[i] == 0);

  dp_hbdecim_q15_destroy (r);

  /* ── execute_max_out always returns 0 (lazy-alloc signal) ────── */
  r = dp_hbdecim_q15_create (1, H1);
  DP_CHECK (r != NULL);
  if (!r)
    return 1;
  DP_CHECK (dp_hbdecim_q15_execute_max_out (r) == 0);
  dp_hbdecim_q15_destroy (r);

  /* serializable state — four dual-write rings + heads round-trip + reject. */
  {
    const float             htaps[4] = { 0.1f, -0.2f, 0.3f, 0.0f };
    dp_hbdecim_q15_state_t *a        = dp_hbdecim_q15_create (7, htaps);
    dp_hbdecim_q15_state_t *b        = dp_hbdecim_q15_create (7, htaps);
    DP_CHECK (a != NULL && b != NULL);
    /* `n_in` and `max_out` count COMPLEX samples; the buffers are
       interleaved int16 IQ, so each needs 2x that many elements (the
       header says so on execute()). Sized 32 with n_in=32, this read two
       bytes off the end of `in` -- caught by ASAN, invisible to the
       assertions below, which only ever look at the state. */
    int16_t in[2 * 32], out[2 * 32];
    for (int i = 0; i < 2 * 32; i++)
      in[i] = (int16_t)(100 * i);
    (void)dp_hbdecim_q15_execute (a, in, 32, out, 32);
    DP_STATE_ROUNDTRIP_TEST (dp_hbdecim_q15, a, b);
    DP_CHECK (b->even_head == a->even_head && b->odd_head == a->odd_head);
    DP_CHECK (b->has_pending == a->has_pending);
    const size_t rb = 2 * a->cap * sizeof (int16_t);
    DP_CHECK (memcmp (b->even_I, a->even_I, rb) == 0);
    DP_CHECK (memcmp (b->odd_Q, a->odd_Q, rb) == 0);
    dp_hbdecim_q15_destroy (a);
    dp_hbdecim_q15_destroy (b);
  }

  /* Two instances in the same logical state give the same blob
     (doppler#2642): one held an even sample across a call and consumed it,
     the other never held one; one reset after a held sample, the other is
     fresh. A consumed or reset pending sample is zeroed, as hbdecim does. */
  {
    static const int16_t in[16]
        = { 100, -200, 300,  400,   -500, 600,  700,   -800,
            900, 1000, 1100, -1200, 1300, 1400, -1500, 1600 };
    int16_t                 o[16];
    dp_hbdecim_q15_state_t *a = dp_hbdecim_q15_create (25, H25_stub);
    dp_hbdecim_q15_state_t *b = dp_hbdecim_q15_create (25, H25_stub);
    (void)dp_hbdecim_q15_execute (a, in, 3, o, 8); /* holds pair 2 */
    (void)dp_hbdecim_q15_execute (a, in + 6, 5, o, 8);
    (void)dp_hbdecim_q15_execute (b, in, 8, o, 8);
    size_t         sb = dp_hbdecim_q15_state_bytes (a);
    unsigned char *ba = malloc (sb), *bb = malloc (sb);
    dp_hbdecim_q15_get_state (a, ba);
    dp_hbdecim_q15_get_state (b, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);

    (void)dp_hbdecim_q15_execute (a, in, 3, o, 8); /* holds pair 2 */
    dp_hbdecim_q15_reset (a);
    dp_hbdecim_q15_state_t *c = dp_hbdecim_q15_create (25, H25_stub);
    dp_hbdecim_q15_get_state (a, ba);
    dp_hbdecim_q15_get_state (c, bb);
    DP_CHECK (memcmp (ba, bb, sb) == 0);
    free (ba);
    free (bb);
    dp_hbdecim_q15_destroy (a);
    dp_hbdecim_q15_destroy (b);
    dp_hbdecim_q15_destroy (c);
  }

  /* A short out drops nothing (doppler#2643): every call stops at a pair
     boundary, consumed() says how much it took, and re-offering the rest
     gives output identical to one whole call. Capacities cycle through 0,
     1, 2 and 3 over an odd-length input. */
  {
    enum
    {
      L = 101
    };
    int16_t in[2 * L], ref[2 * L], got[2 * L];
    for (size_t i = 0; i < 2 * L; i++)
      in[i] = (int16_t)(8000.0 * cos (0.37 * (double)i));
    dp_hbdecim_q15_state_t *a    = dp_hbdecim_q15_create (25, H25_stub);
    size_t                  nref = dp_hbdecim_q15_execute (a, in, L, ref, L);
    DP_CHECK (dp_hbdecim_q15_consumed (a) == L);
    dp_hbdecim_q15_state_t *b   = dp_hbdecim_q15_create (25, H25_stub);
    size_t                  off = 0, ngot = 0, k = 0, calls = 0;
    while (off < L && calls++ < 4 * L)
      {
        size_t cap = (k++) % 4;
        ngot += dp_hbdecim_q15_execute (b, in + 2 * off, L - off,
                                        got + 2 * ngot, cap);
        off += dp_hbdecim_q15_consumed (b);
      }
    DP_CHECK (off == L);
    DP_CHECK (ngot == nref);
    DP_CHECK (memcmp (ref, got, 2 * nref * sizeof *ref) == 0);
    dp_hbdecim_q15_destroy (a);
    dp_hbdecim_q15_destroy (b);
  }

  DP_TEST_END ("test_hbdecim_q15_core");
}
