/*
 * test_fec_ccsds_asm.c — the Attached Sync Marker, and finding it.
 *
 * Its own file for the reason ccsds_asm.c is its own translation unit: a
 * receiver that only correlates against the marker should not have to link an
 * R-S encoder, and a test that only exercises the marker should not have to
 * assemble a frame to do it.
 *
 * The search is the part that can be subtly wrong, and it can be wrong in two
 * opposite directions. Too strict and a marker the channel touched is missed,
 * so the frame behind it is lost even though the inner code would have
 * carried it. Too loose and random data produces a hit, so the receiver locks
 * to noise and decodes a frame that was never sent. Both are exercised below
 * rather than one, because a threshold defended from only one side is a
 * threshold that has been tuned rather than chosen.
 */
#define _GNU_SOURCE
#include "dp_rng_test.h"
#include "dp_test.h"

#include "fec/fec_ccsds.h"

#include <string.h>

int
main (void)
{
  uint8_t marker[FEC_CCSDS_ASM_BITS];
  fec_ccsds_asm_bits (marker);

  /* ── the marker is the published constant, MSB-first ────────────────── */
  {
    /* 0x1ACFFC1D, figure 9-1: the first transmitted bit is the top of 0x1A.
       Spelled out rather than shifted, so this is a transcription of the
       standard and not a second copy of the code under test. */
    static const uint8_t published[FEC_CCSDS_ASM_BITS] = {
      0, 0, 0, 1, 1, 0, 1, 0, /* 1A */
      1, 1, 0, 0, 1, 1, 1, 1, /* CF */
      1, 1, 1, 1, 1, 1, 0, 0, /* FC */
      0, 0, 0, 1, 1, 1, 0, 1  /* 1D */
    };
    DP_CHECK_MSG (memcmp (marker, published, sizeof published) == 0,
                  "the marker must be 0x1ACFFC1D, first bit at the top");
  }

  /* ── it is found where it was put, at any offset ─────────────────────── */
  {
    enum
    {
      N = 400
    };
    for (size_t at = 0; at + FEC_CCSDS_ASM_BITS <= N; at += 37)
      {
        /* Zeros around it: the marker is the only thing to find, so a hit
           anywhere else is the search inventing one. */
        uint8_t       bits[N] = { 0 };
        fec_asm_hit_t hit     = { 999u, -1, 999u };
        memcpy (bits + at, marker, sizeof marker);
        DP_REQUIRE (fec_ccsds_asm_find (bits, N, 0u, &hit));
        DP_CHECK_MSG (hit.offset == at,
                      "the marker must be found where it is");
        DP_CHECK (hit.errors == 0u && hit.inverted == 0);
      }

    /* The LAST offset a marker can occupy, explicitly. The stride above
       never lands on it, and a search whose loop bound is `<` rather than
       `<=` passes every case above while losing exactly this one -- a frame
       flush against the end of a capture. */
    uint8_t       bits[N] = { 0 };
    fec_asm_hit_t hit     = { 999u, -1, 999u };
    memcpy (bits + (N - FEC_CCSDS_ASM_BITS), marker, sizeof marker);
    DP_REQUIRE_MSG (fec_ccsds_asm_find (bits, N, 0u, &hit),
                    "a marker at the last possible offset must be found");
    DP_CHECK (hit.offset == N - FEC_CCSDS_ASM_BITS);
  }

  /* ── a complemented stream is found, and SAID to be complemented ─────
   *
   * The 180-degree ambiguity of a BPSK carrier delivers the whole stream
   * inverted. The marker is the only part of a CADU that can report it --
   * the randomiser deliberately does not cover it -- so a search that found
   * the marker without saying which polarity it was in would hand the frame
   * decoder bits it will silently misread.
   */
  {
    enum
    {
      N = 200
    };
    uint8_t       bits[N];
    fec_asm_hit_t hit = { 999u, -1, 999u };
    for (size_t i = 0; i < N; i++)
      bits[i] = 1u; /* the complement of the zero background */
    for (unsigned i = 0; i < FEC_CCSDS_ASM_BITS; i++)
      bits[64 + i] = (uint8_t)(marker[i] ^ 1u);
    DP_REQUIRE (fec_ccsds_asm_find (bits, N, 0u, &hit));
    DP_CHECK (hit.offset == 64u && hit.errors == 0u);
    DP_CHECK_MSG (hit.inverted, "an inverted marker must report its polarity");
  }

  /* ── max_errors is honoured from BOTH sides ──────────────────────────
   *
   * Corrupt exactly three bits of the marker: a tolerance of 3 must find it
   * and a tolerance of 2 must not. One of those alone proves nothing -- a
   * search that always succeeds passes the first, and one that always fails
   * passes the second.
   */
  {
    enum
    {
      N = 128
    };
    uint8_t bits[N] = { 0 };
    memcpy (bits + 32, marker, sizeof marker);
    bits[32 + 1] ^= 1u;
    bits[32 + 11] ^= 1u;
    bits[32 + 29] ^= 1u;

    fec_asm_hit_t hit = { 999u, -1, 999u };
    DP_CHECK_MSG (!fec_ccsds_asm_find (bits, N, 2u, &hit),
                  "three errors must not be found at a tolerance of two");
    DP_REQUIRE (fec_ccsds_asm_find (bits, N, 3u, &hit));
    DP_CHECK (hit.offset == 32u && hit.errors == 3u && hit.inverted == 0);
  }

  /* ── it does not invent a marker in random data ─────────────────────
   *
   * The false-hit side of the threshold, and the arithmetic is the point
   * rather than the assertion. A random 32-bit window lands within `t` of the
   * marker in one polarity or the other with probability
   * `2 * sum_{i<=t} C(32,i) / 2^32`, and a search over `n` offsets gets `n`
   * tries at it:
   *
   *   t = 1    66 / 2^32       ~ 1 in 10^3 over 65536 offsets
   *   t = 2  1058 / 2^32       ~ 0.2%     over  8192 offsets
   *   t = 3 10978 / 2^32       ~ 17%      over 65536 offsets
   *
   * That last row is why this checks t = 2 over 8192 rather than the t = 3 a
   * noisy link wants: at t = 3 a long search is MORE LIKELY THAN NOT to false
   * hit within a few frames, so a passing test at that setting would be luck
   * and a green CI would be recording it as a property. A receiver wanting
   * t = 3 has to bound its search window instead, which is what the link
   * demo does.
   */
  {
    enum
    {
      N = 8192
    };
    static uint8_t bits[N];
    uint32_t       st = 424242u;
    for (size_t i = 0; i < N; i++)
      bits[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    fec_asm_hit_t hit = { 999u, -1, 999u };
    DP_CHECK_MSG (!fec_ccsds_asm_find (bits, N, 2u, &hit),
                  "random data must not produce a sync hit");
  }

  /* ── the refusals ───────────────────────────────────────────────────── */
  {
    uint8_t       bits[FEC_CCSDS_ASM_BITS - 1] = { 0 };
    fec_asm_hit_t hit                          = { 999u, -1, 999u };
    DP_CHECK_MSG (!fec_ccsds_asm_find (bits, sizeof bits, 32u, &hit),
                  "a run shorter than the marker cannot contain one");
    DP_CHECK_MSG (hit.offset == 999u,
                  "a miss must leave the caller's hit untouched");
  }

  DP_TEST_END ("fec_ccsds_asm");
}
