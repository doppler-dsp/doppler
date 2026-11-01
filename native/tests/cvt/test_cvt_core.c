/* test_cvt_core.c — smoke test for the cvt module's
 * free functions.
 *
 * gh-1034: jm generates and owns a function-only module, and used to generate
 * no C test for it — so the one component whose C jm writes end to end was
 * the one with nothing checking it. An object has had this file since the
 * beginning.
 *
 * The bit conversions below are measured against an EXTERNAL truth, not only
 * against each other. A pure round-trip is blind to any defect the two
 * directions share: a pair that agreed on a WRONG bit order would round-trip
 * perfectly. So the published CCSDS ASM expansion pins the order first, and
 * only then is a round-trip worth anything.
 */
#include "doppler/cvt/cvt_core.h"
#include "doppler/mpsk/mpsk_core.h"

#include "dp_test.h"

#include <math.h>
#include <string.h>

int
main (void)
{
  uint8_t b[64], viahex[64], hx[32];

  /* ── the bit order itself, against a published expansion ──────────── */

  /* 0x1A, MSB first. Figure 9-1 of 131.0-B-3 numbers the first transmitted
     bit of the marker as the top bit of 0x1A, which is what BIG means. */
  const uint8_t byte_big[] = { 0, 0, 0, 1, 1, 0, 1, 0 };
  DP_CHECK (dp_int_to_bin (0x1Au, 8u, b, sizeof b, DP_BITORDER_BIG) == 8u);
  DP_CHECK (memcmp (b, byte_big, sizeof byte_big) == 0);

  /* LITTLE reverses within each BYTE, not across the whole literal. The
     distinction is the whole reason the unit width is written once. */
  const uint8_t byte_little[] = { 0, 1, 0, 1, 1, 0, 0, 0 };
  DP_CHECK (dp_int_to_bin (0x1Au, 8u, b, sizeof b, DP_BITORDER_LITTLE) == 8u);
  DP_CHECK (memcmp (b, byte_little, sizeof byte_little) == 0);

  /* ── the two expansions agree ─────────────────────────────────────── */

  /* Same value, two derivations — a shifted integer and a parsed string.
     Neither is defined in terms of the other, so agreeing is evidence.
     What it pins is the bit order, which a transmitter and a receiver must
     not disagree about. */
  for (int bo = 0; bo < 2; bo++)
    {
      DP_CHECK (dp_int_to_bin (0x1ACFFC1DULL, 32u, b, sizeof b, bo) == 32u);
      DP_CHECK (dp_hex_to_bin ("1ACFFC1D", viahex, sizeof viahex, bo) == 32u);
      DP_CHECK (memcmp (b, viahex, 32u) == 0);
    }

  /* ── round-trips, now that the orders themselves are pinned ───────── */

  const uint32_t widths[] = { 1u, 4u, 7u, 8u, 12u, 32u, 63u, 64u };
  for (size_t w = 0; w < sizeof widths / sizeof widths[0]; w++)
    {
      for (int bo = 0; bo < 2; bo++)
        {
          const uint32_t n = widths[w];
          const uint64_t want
              = (n == 64u) ? 0x0123456789ABCDEFULL
                           : (0x0123456789ABCDEFULL & ((1ULL << n) - 1u));
          DP_CHECK (dp_int_to_bin (want, n, b, sizeof b, bo) == n);
          DP_CHECK (dp_bin_to_int (b, n, bo) == want);
        }
    }

  for (int bo = 0; bo < 2; bo++)
    {
      DP_CHECK (dp_hex_to_bin ("1acffc1d", b, sizeof b, bo) == 32u);
      DP_CHECK (dp_bin_to_hex (b, 32u, hx, sizeof hx, bo) == 8u);
      DP_CHECK (strcmp ((const char *)hx, "1acffc1d") == 0);
    }

  /* An ODD digit count is accepted and yields a 4-bit tail. */
  DP_CHECK (dp_hex_to_bin ("abc", b, sizeof b, DP_BITORDER_BIG) == 12u);
  DP_CHECK (dp_bin_to_hex (b, 12u, hx, sizeof hx, DP_BITORDER_BIG) == 3u);
  DP_CHECK (strcmp ((const char *)hx, "abc") == 0);

  /* Any non-zero byte reads as a set bit, so a caller's 0/255 mask
     still renders rather than silently becoming zeros. */
  uint8_t loud[8];
  for (int i = 0; i < 8; i++)
    loud[i] = (uint8_t)(byte_big[i] ? 255u : 0u);
  DP_CHECK (dp_bin_to_hex (loud, 8u, hx, sizeof hx, DP_BITORDER_BIG) == 2u);
  DP_CHECK (strcmp ((const char *)hx, "1a") == 0);

  /* ── refusals ─────────────────────────────────────────────────────── */

  /* A bad digit REFUSES rather than skipping, and writes nothing: a typo
     that silently shortened a marker is the failure this prevents. */
  memset (b, 0xAAu, sizeof b);
  DP_CHECK (dp_hex_to_bin ("12g4", b, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (b[0] == 0xAAu);

  DP_CHECK (dp_hex_to_bin ("", b, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_hex_to_bin (NULL, b, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_hex_to_bin ("1ACF", b, 15u, DP_BITORDER_BIG) == 0);
  /* An unknown bit order is a refusal, not a silent default. */
  DP_CHECK (dp_hex_to_bin ("1ACF", b, sizeof b, 7) == 0);

  DP_CHECK (dp_int_to_bin (1u, 0u, b, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_int_to_bin (1u, 65u, b, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_int_to_bin (1u, 8u, NULL, sizeof b, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_int_to_bin (1u, 8u, b, 7u, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_int_to_bin (1u, 8u, b, sizeof b, 7) == 0);

  DP_CHECK (dp_bin_to_int (b, 0u, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_bin_to_int (b, 65u, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_bin_to_int (NULL, 8u, DP_BITORDER_BIG) == 0);

  DP_CHECK (dp_hex_to_bin ("1ACF", b, sizeof b, DP_BITORDER_BIG) == 16u);
  /* A bit count that is not a whole number of digits. */
  DP_CHECK (dp_bin_to_hex (b, 15u, hx, sizeof hx, DP_BITORDER_BIG) == 0);
  /* 4 digits plus a NUL do not fit in 4 bytes. */
  DP_CHECK (dp_bin_to_hex (b, 16u, hx, 4u, DP_BITORDER_BIG) == 0);
  DP_CHECK (dp_bin_to_hex (b, 16u, hx, 5u, DP_BITORDER_BIG) == 4u);

  /* Only the low n_bits are read, so a caller need not mask first. */
  DP_CHECK (dp_int_to_bin (0xFF00u, 8u, b, sizeof b, DP_BITORDER_BIG) == 8u);
  DP_CHECK (dp_bin_to_int (b, 8u, DP_BITORDER_BIG) == 0u);

  /* ── bin_to_nrz / nrz_to_bin ──────────────────────────────────────── */

  /* The sign convention's HOME is mpsk_core.h: BPSK is M-PSK at m = 2,
     phi0 = 0, so label 0 lands at +1 and label 1 at -1. bin_to_nrz states
     the same thing as `1 - 2*b` because a per-bit loop cannot afford a cos
     and a sin per symbol -- so the two are asserted equal here rather than
     trusted to stay equal. A mapper that disagreed with the receiver's
     would decode every bit INVERTED while looking perfectly locked, which
     is the one failure a round-trip test cannot see.

     mpsk_constellation is JM_FORCEINLINE, so this costs no link edge. */
  {
    uint8_t bit01[2] = { 0u, 1u };
    float   sym[2];
    DP_CHECK (dp_bin_to_nrz (bit01, 2u, sym, 2u) == 2u);
    for (unsigned g = 0; g < 2u; g++)
      DP_CHECK (sym[g] == crealf (mpsk_constellation (g, 2)));
    /* ...and the imaginary part is negligible, which is what makes
       taking the real part lossless in practice. NOT bit-zero: the
       constellation is built from cos/sin, and sin(pi) is ~1.2e-16
       rather than 0, so an exact comparison here fails on a value that
       is entirely correct. */
    for (unsigned g = 0; g < 2u; g++)
      DP_CHECK (fabsf (cimagf (mpsk_constellation (g, 2))) < 1e-6f);
  }

  /* Round trip over a pattern, both directions. */
  {
    uint8_t in[8] = { 0, 1, 1, 0, 1, 0, 0, 1 };
    uint8_t back[8];
    float   sym[8];
    DP_CHECK (dp_bin_to_nrz (in, 8u, sym, sizeof sym / sizeof sym[0]) == 8u);
    DP_CHECK (dp_nrz_to_bin (sym, 8u, back, sizeof back) == 8u);
    DP_CHECK (memcmp (in, back, sizeof in) == 0);

    /* Any non-zero byte is a set bit, matching bin_to_hex. */
    uint8_t loud[2] = { 0u, 255u };
    DP_CHECK (dp_bin_to_nrz (loud, 2u, sym, 2u) == 2u);
    DP_CHECK (sym[0] == 1.0f && sym[1] == -1.0f);

    /* Zero decides to 0, so the mapping is total. */
    float edge[3] = { 0.0f, -0.0f, 0.25f };
    DP_CHECK (dp_nrz_to_bin (edge, 3u, back, sizeof back) == 3u);
    DP_CHECK (back[0] == 0u && back[1] == 0u && back[2] == 0u);

    /* Refusals. */
    DP_CHECK (dp_bin_to_nrz (NULL, 8u, sym, 8u) == 0);
    DP_CHECK (dp_bin_to_nrz (in, 0u, sym, 8u) == 0);
    DP_CHECK (dp_bin_to_nrz (in, 8u, sym, 7u) == 0);
    DP_CHECK (dp_nrz_to_bin (NULL, 8u, back, 8u) == 0);
    DP_CHECK (dp_nrz_to_bin (sym, 0u, back, 8u) == 0);
    DP_CHECK (dp_nrz_to_bin (sym, 8u, back, 7u) == 0);
  }

  /* ── bytes_to_bin: packed octets to bits ───────────────────────────
   * Truth written out by hand. 0x1A 0xCF is the head of the CCSDS marker,
   * so under BIG this must agree with hex_to_bin("1ACF") -- the same octets
   * reached as text and as data cannot unpack two ways. */
  {
    static const uint8_t oct[2] = { 0x1Au, 0xCFu };
    static const uint8_t big[16]
        = { 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1 };
    static const uint8_t little[16]
        = { 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1 };
    uint8_t ob[16], viahex[16];
    DP_CHECK (dp_bytes_to_bin (oct, 2u, ob, sizeof ob, DP_BITORDER_BIG)
              == 16u);
    DP_CHECK (memcmp (ob, big, 16) == 0);
    DP_CHECK (dp_hex_to_bin ("1ACF", viahex, sizeof viahex, DP_BITORDER_BIG)
              == 16u);
    DP_CHECK_MSG (memcmp (ob, viahex, 16) == 0,
                  "octets and their hex spelling unpack identically");
    DP_CHECK (dp_bytes_to_bin (oct, 2u, ob, sizeof ob, DP_BITORDER_LITTLE)
              == 16u);
    DP_CHECK (memcmp (ob, little, 16) == 0);

    /* Refusals leave the output untouched. */
    memset (ob, 7, sizeof ob);
    DP_CHECK (dp_bytes_to_bin (oct, 0u, ob, sizeof ob, DP_BITORDER_BIG) == 0);
    DP_CHECK (dp_bytes_to_bin (NULL, 2u, ob, sizeof ob, DP_BITORDER_BIG) == 0);
    DP_CHECK (dp_bytes_to_bin (oct, 2u, NULL, 16u, DP_BITORDER_BIG) == 0);
    DP_CHECK (dp_bytes_to_bin (oct, 2u, ob, 15u, DP_BITORDER_BIG) == 0);
    DP_CHECK (dp_bytes_to_bin (oct, 2u, ob, sizeof ob, 7) == 0);
    int untouched = 1;
    for (size_t i = 0; i < sizeof ob; i++)
      untouched &= (ob[i] == 7u);
    DP_CHECK_MSG (untouched, "a refusal writes nothing, in any byte");
  }

  /* Every octet value, both orders: 8 bits an octet, each 0 or 1, and the
     two orders each other's reverse within the octet (D13: the header's
     claims held at every input, not at one vector). */
  {
    uint8_t all[256], big[2048], little[2048];
    for (size_t i = 0; i < 256; i++)
      all[i] = (uint8_t)i;
    DP_CHECK_MSG (dp_bytes_to_bin (all, 256u, big, sizeof big, DP_BITORDER_BIG)
                      == 2048u,
                  "each octet gives 8 bits");
    DP_REQUIRE (
        dp_bytes_to_bin (all, 256u, little, sizeof little, DP_BITORDER_LITTLE)
        == 2048u);
    int ok = 1;
    for (size_t i = 0; i < 256; i++)
      for (unsigned b = 0; b < 8u; b++)
        {
          const uint8_t want = (uint8_t)((i >> (7u - b)) & 1u);
          ok &= big[i * 8u + b] == want && little[i * 8u + 7u - b] == want;
        }
    DP_CHECK_MSG (ok, "BIG is MSB first, LITTLE its reverse, every octet");
  }

  DP_TEST_END ("test_cvt_core");
}
