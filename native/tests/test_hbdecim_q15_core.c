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
#include <stddef.h>
#include <stdint.h>
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

  /* #2148 r3: the AVX2 kernel reads a[N-16-k], which is inside the fold
   * only when the fold is at least K_pad long. Tap counts under 16 take the
   * scalar path. The ring is heap-allocated, so ASan catches a read outside
   * it; the in/out buffers here are stack arrays and are not what ASan
   * checks for this. The native -march=native ASan build is the proof. */
  for (size_t taps = 2; taps <= 15; taps++)
    {
      float h[15];
      for (size_t i = 0; i < taps; i++)
        h[i] = 0.05f * (float)(i + 1);
      dp_hbdecim_q15_state_t *q = dp_hbdecim_q15_create (taps, h);
      DP_CHECK (q != NULL);
      if (q)
        {
          int16_t in[2 * 24], out[2 * 24];
          for (int i = 0; i < 2 * 24; i++)
            in[i] = (int16_t)(50 * i - 300);
          size_t n = dp_hbdecim_q15_execute (q, in, 24, out, 24);
          DP_CHECK (n <= 24);
          dp_hbdecim_q15_destroy (q);
        }
    }

  /* ---------------------------------------------------------------- *
   * #2142: a forged head or flag is refused, the object untouched          *
   * ---------------------------------------------------------------- */
  {
    /* Blob: [hdr][even_head u64][odd_head u64][has_pending u32]... */
    dp_hbdecim_q15_state_t *s = dp_hbdecim_q15_create (25, H25_stub);
    DP_CHECK (s != NULL);
    if (s)
      {
        const size_t   eh  = sizeof (dp_state_hdr_t);
        const size_t   oh  = eh + sizeof (uint64_t);
        const size_t   hp  = oh + sizeof (uint64_t);
        const uint64_t cap = (uint64_t)s->cap;
        const uint32_t two = 2;
        DP_STATE_FORGE_TEST (dp_hbdecim_q15, s, HBDECIM_Q15_STATE_MAGIC,
                             HBDECIM_Q15_STATE_VERSION, eh, &cap, sizeof cap);
        DP_STATE_FORGE_TEST (dp_hbdecim_q15, s, HBDECIM_Q15_STATE_MAGIC,
                             HBDECIM_Q15_STATE_VERSION, oh, &cap, sizeof cap);
        DP_STATE_FORGE_TEST (dp_hbdecim_q15, s, HBDECIM_Q15_STATE_MAGIC,
                             HBDECIM_Q15_STATE_VERSION, hp, &two, sizeof two);
        dp_hbdecim_q15_destroy (s);
      }
  }

  DP_TEST_END ("test_hbdecim_q15_core");
}
