/* test_dsss_core.c — the dsss module's free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to
 * generate no C test for it — so the one component whose C jm writes end to
 * end was the one with nothing checking it. An object has had this file
 * since the beginning.
 */
#include "dsss/dsss_core.h"

/* dp_test.h, not jm's scaffold harness: `jm_test.h` is gitignored, so a
 * test including it builds on a machine that has run `jm apply` and fails
 * in CI. doppler keeps ONE assertion harness (just-makeit.toml's
 * status_allow says why); `make tests-ssot` is the gate that catches the
 * other one, and it caught this file. */
#include "dp_test.h"

int
main (void)
{
  /* ── bin_to_signed IS numpy.fft.fftfreq(n)*n ──────────────────────────
   *
   * The whole point of the wrapper is that C and Python read an FFT grid
   * the same way, and the convention it follows is the universal one. The
   * table below is numpy's output, written out, so this test states the
   * contract rather than re-deriving it from the implementation it is
   * testing.
   *
   * The even cases carry the index that matters: an even grid's Nyquist
   * bin is NEGATIVE (-n/2). This engine reported +n/2 there until the
   * `burst` certification, which is a defensible reading -- the two are
   * the same frequency and a search on this grid cannot separate them --
   * but it made every formula ported in from numpy disagree at exactly
   * the bin the engine was most careful about. */
  {
    /* n = 8 (even): 0 1 2 3 -4 -3 -2 -1 */
    static const int want8[8] = { 0, 1, 2, 3, -4, -3, -2, -1 };
    for (size_t b = 0; b < 8; b++)
      DP_CHECK (bin_to_signed (b, 8) == want8[b]);

    /* n = 7 (odd): 0 1 2 3 -3 -2 -1  — no ambiguous index exists */
    static const int want7[7] = { 0, 1, 2, 3, -3, -2, -1 };
    for (size_t b = 0; b < 7; b++)
      DP_CHECK (bin_to_signed (b, 7) == want7[b]);

    /* n = 4 and n = 2, the smallest even grids */
    static const int want4[4] = { 0, 1, -2, -1 };
    for (size_t b = 0; b < 4; b++)
      DP_CHECK (bin_to_signed (b, 4) == want4[b]);
    static const int want2[2] = { 0, -1 };
    for (size_t b = 0; b < 2; b++)
      DP_CHECK (bin_to_signed (b, 2) == want2[b]);

    /* n = 1: the only bin is DC. */
    DP_CHECK (bin_to_signed (0, 1) == 0);
  }

  /* ── the wrapper and the inline are the same function ─────────────────
   *
   * bin_to_signed() exists only so Python reaches dp_fftfreq_index(); if
   * they ever diverged, the duplication this whole exercise removed would
   * be back with an extra step. */
  {
    for (size_t n = 1; n <= 33; n++)
      for (size_t b = 0; b < n; b++)
        DP_CHECK ((long)bin_to_signed (b, n) == dp_fftfreq_index (b, n));
  }

  /* ── dp_fftfreq() takes fs, not numpy's sample SPACING ────────────────
   *
   * The one deliberate difference from the numpy signature, and the reason
   * it is worth a test: a caller who assumes `d` would pass 1/fs and be
   * wrong by fs^2. At fs = 1 the two agree by construction, which is
   * exactly why that case cannot be the only one checked. */
  {
    const double fs = 8000.0;
    const size_t n  = 8;
    /* bin 1 of an 8-point grid at 8 kHz is 1 kHz; bin 4 is -4 kHz. */
    DP_CHECK (dp_fftfreq (1, n, fs) == 1000.0);
    DP_CHECK (dp_fftfreq (4, n, fs) == -4000.0);
    DP_CHECK (dp_fftfreq (7, n, fs) == -1000.0);
    DP_CHECK (dp_fftfreq (0, n, fs) == 0.0);
    /* fs = 1 gives normalised cycles/sample, numpy's default. */
    DP_CHECK (dp_fftfreq (1, n, 1.0) == 0.125);
    DP_CHECK (dp_fftfreq (4, n, 1.0) == -0.5);
    /* and it is index * fs / n, so it scales with fs rather than with 1/fs */
    DP_CHECK (dp_fftfreq (1, n, 2.0 * fs) == 2.0 * dp_fftfreq (1, n, fs));
    /* n = 0 is refused rather than dividing by zero. */
    DP_CHECK (dp_fftfreq (0, 0, fs) == 0.0);
  }

  DP_TEST_END ("test_dsss_core");
}
