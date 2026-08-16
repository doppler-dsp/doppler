/*
 * test_fec_ccsds_rand.c — the CCSDS pseudo-randomiser, held to the sequence
 * the standard prints.
 *
 * This test exists in the shape it does because coding is unusually easy to
 * test vacuously. `randomise` is its own inverse, so a round trip returns the
 * input for ANY self-consistent LFSR — a swapped tap, a Fibonacci/Galois
 * mixup, the wrong preset — and a round-trip test would pass for all of them
 * while producing a frame no receiver on earth could derandomise.
 *
 * The check that bites is the published prefix: CCSDS 131.0-B-3 section
 * 10.4.2 prints the first 40 bits of the sequence, and only one tap
 * arrangement reproduces them.
 *
 * The data used is ZEROS, deliberately. A PN payload is already maximally
 * random, so XORing a randomiser onto one produces something that looks
 * exactly as random whether the randomiser ran, ran wrong, or did not run at
 * all — the absence is invisible. Zeros make the sequence itself the output.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "fec/fec_ccsds.h"

#include <string.h>

/* CCSDS 131.0-B-3, 10.4.2:
 *   1111 1111 0100 1000 0000 1110 1100 0000 1001 1010 . . . .
 * i.e. FF 48 0E C0 9A, MSB-first, the leftmost bit being the one XORed with
 * the first bit of the codeblock. */
static const uint8_t published40[40] = {
  1, 1, 1, 1, 1, 1, 1, 1, /* FF */
  0, 1, 0, 0, 1, 0, 0, 0, /* 48 */
  0, 0, 0, 0, 1, 1, 1, 0, /* 0E */
  1, 1, 0, 0, 0, 0, 0, 0, /* C0 */
  1, 0, 0, 1, 1, 0, 1, 0  /* 9A */
};

int
main (void)
{
  /* ── the sequence itself, against the printed prefix ────────────────── */
  {
    uint8_t seq[40];
    fec_ccsds_rand_seq (seq, 40);
    DP_CHECK_MSG (memcmp (seq, published40, sizeof published40) == 0,
                  "first 40 bits must match CCSDS 131.0-B-3 10.4.2");
  }

  /* ── randomising zeros yields the sequence ──────────────────────────── */
  {
    uint8_t bits[40] = { 0 };
    fec_ccsds_randomise (bits, 40);
    DP_CHECK_MSG (memcmp (bits, published40, sizeof published40) == 0,
                  "randomising zeros must emit the sequence verbatim");
  }

  /* ── 10.4.2: the sequence repeats after 255 bits ────────────────────── */
  {
    uint8_t seq[300];
    fec_ccsds_rand_seq (seq, 300);
    DP_CHECK_MSG (memcmp (seq, seq + 255, 45) == 0,
                  "the sequence must repeat with period 255");

    /* The balance check is here because the two period checks BELOW are not
       enough on their own, and this test learned that the hard way: the first
       implementation drove the register to the all-zero fixed point, and a
       dead sequence repeats with period 255, has no shorter period, and
       satisfies both. Only the published prefix and this caught it.

       A maximal 8-stage generator visits all 255 non-zero states, so its
       period holds exactly 128 ones — a property no degenerate sequence
       has. */
    size_t ones = 0;
    for (size_t i = 0; i < 255; i++)
      ones += seq[i];
    DP_CHECK_MSG (ones == 128,
                  "a maximal-length period must carry exactly 128 ones");

    /* ...and it must not repeat sooner, or the register is too short. */
    int early = 0;
    for (size_t p = 1; p < 255; p++)
      {
        if (memcmp (seq, seq + p, 40) == 0)
          early = 1;
      }
    DP_CHECK_MSG (!early, "no period shorter than 255 may appear");
  }

  /* ── its own inverse, over a run that crosses the period boundary ───── */
  {
    uint8_t data[600], copy[600];
    for (size_t i = 0; i < sizeof data; i++)
      data[i] = (uint8_t)((i * 7u + 3u) & 1u);
    memcpy (copy, data, sizeof data);
    fec_ccsds_randomise (data, sizeof data);
    DP_CHECK_MSG (memcmp (data, copy, sizeof data) != 0,
                  "randomising must actually change the data");
    fec_ccsds_randomise (data, sizeof data);
    DP_CHECK_MSG (memcmp (data, copy, sizeof data) == 0,
                  "randomise twice must be the identity");
  }

  /* ── each call restarts at the all-ones preset (10.4.2) ─────────────── */
  {
    uint8_t a[16] = { 0 }, b[16] = { 0 };
    fec_ccsds_randomise (a, 16);
    fec_ccsds_randomise (b, 16);
    DP_CHECK_MSG (memcmp (a, b, sizeof a) == 0,
                  "the generator must preset per call, not carry state");
  }

  DP_TEST_END ("fec_ccsds_rand");
}
