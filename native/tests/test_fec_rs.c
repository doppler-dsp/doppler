/*
 * test_fec_rs.c — CCSDS Reed-Solomon (255,223), held to Annex G and 4.3.9.3.
 *
 * Reed-Solomon is the worst offender in this slice for tests that cannot
 * fail. Encode-then-decode inverts correctly over ANY field polynomial, ANY
 * set of generator roots and ANY basis, so long as both ends agree — and the
 * whole point of a standard is that both ends are built by different people
 * who never compare notes.
 *
 * So nothing here round-trips against a decoder. Every check is against a
 * value CCSDS 131.0-B-3 prints:
 *
 *   - ANNEX G publishes all 33 coefficients of g(x) for E=16. Reproducing
 *     them exercises the field polynomial (4.3.3) and the a^11 root stride
 *     (4.3.4) together — get either wrong and the coefficients move.
 *   - 4.3.9.3 gives the two basis matrices. Requiring them to invert each
 *     other across all 256 symbols catches a single mis-transcribed bit,
 *     which reading them twice does not.
 *   - the code is SYSTEMATIC (4.3.4 note 2), so the information symbols must
 *     survive encoding unchanged.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "fec/fec_rs.h"

#include <string.h>

/* CCSDS 131.0-B-3 Annex G, "For E = 16", read off the POLYNOMIAL IN a
 * columns (a^7 ... a^0, so the leftmost column is bit 7):
 *
 *   G0 = G32 = a^0   = 0000 0001     G9  = G23 = a^30  = 1010 0101
 *   G1 = G31 = a^249 = 0101 1011     G10 = G22 = a^3   = 0000 1000
 *   G2 = G30 = a^59  = 0111 1111     G11 = G21 = a^213 = 0010 1010
 *   G3 = G29 = a^66  = 0101 0110     G12 = G20 = a^50  = 0011 0110
 *   G4 = G28 = a^4   = 0001 0000     G13 = G19 = a^66  = 0101 0110
 *   G5 = G27 = a^43  = 0001 1110     G14 = G18 = a^170 = 1010 1011
 *   G6 = G26 = a^126 = 0000 1101     G15 = G17 = a^5   = 0010 0000
 *   G7 = G25 = a^251 = 1110 1011     G16       = a^24  = 0111 0001
 *   G8 = G24 = a^97  = 0110 0001
 */
static const uint8_t annex_g[FEC_RS_2E + 1]
    = { 0x01, 0x5B, 0x7F, 0x56, 0x10, 0x1E, 0x0D, 0xEB, 0x61, 0xA5, 0x08,
        0x2A, 0x36, 0x56, 0xAB, 0x20, 0x71, 0x20, 0xAB, 0x56, 0x36, 0x2A,
        0x08, 0xA5, 0x61, 0xEB, 0x0D, 0x1E, 0x10, 0x56, 0x7F, 0x5B, 0x01 };

int
main (void)
{
  /* ── Annex G: the generator polynomial, coefficient by coefficient ──── */
  {
    const uint8_t *g    = fec_rs_generator ();
    int            same = 1;
    for (size_t i = 0; i < sizeof annex_g; i++)
      {
        if (g[i] != annex_g[i])
          same = 0;
      }
    DP_CHECK_MSG (same, "g(x) must match CCSDS 131.0-B-3 Annex G (E=16)");

    /* Annex G's own structural note, worth asserting because it is a
       property of the root set rather than of any one coefficient. */
    int palindromic = 1;
    for (size_t i = 0; i <= FEC_RS_2E; i++)
      {
        if (g[i] != g[FEC_RS_2E - i])
          palindromic = 0;
      }
    DP_CHECK_MSG (palindromic, "g(x) must be palindromic");
    DP_CHECK_MSG (g[3] == g[29] && g[3] == g[13] && g[3] == g[19],
                  "Annex G NOTE: G3 = G29 = G13 = G19");
  }

  /* ── 4.3.9.3: the two matrices must be exact inverses ───────────────── */
  {
    int round = 1, nontrivial = 0;
    for (int v = 0; v < 256; v++)
      {
        const uint8_t u = (uint8_t)v;
        if (fec_rs_dual_to_conv (fec_rs_conv_to_dual (u)) != u)
          round = 0;
        if (fec_rs_conv_to_dual (u) != u)
          nontrivial = 1;
      }
    DP_CHECK_MSG (round, "the basis transforms must invert across all 256");
    /* ...and must not be the identity, which a matrix of zeros would also
       satisfy the check above with. */
    DP_CHECK_MSG (nontrivial, "the dual basis must not be the identity map");
  }

  /* ── the code is systematic, and parity is not trivial ──────────────── */
  {
    uint8_t info[FEC_RS_K], parity[FEC_RS_2E];
    for (size_t i = 0; i < sizeof info; i++)
      info[i] = (uint8_t)(i * 7u + 1u);

    uint8_t before[FEC_RS_K];
    memcpy (before, info, sizeof info);
    fec_rs_encode (info, parity);

    DP_CHECK_MSG (memcmp (info, before, sizeof info) == 0,
                  "encoding must not disturb the information symbols");

    int any = 0;
    for (size_t i = 0; i < sizeof parity; i++)
      {
        if (parity[i] != 0)
          any = 1;
      }
    DP_CHECK_MSG (any, "parity over non-trivial data must not be all zeros");
  }

  /* ── an all-zero codeword has zero parity, in EITHER basis ──────────── */
  {
    uint8_t info[FEC_RS_K] = { 0 }, parity[FEC_RS_2E];
    fec_rs_encode (info, parity);
    int zero = 1;
    for (size_t i = 0; i < sizeof parity; i++)
      {
        if (parity[i] != 0)
          zero = 0;
      }
    /* This one is deliberately weak on its own — it passes for almost any
       linear code — but it is the check that catches a basis transform with
       a stray constant term, which the round-trip above cannot see. */
    DP_CHECK_MSG (zero,
                  "the all-zero information block must have zero parity");
  }

  /* ── one changed symbol must change the parity ──────────────────────── */
  {
    uint8_t a[FEC_RS_K] = { 0 }, b[FEC_RS_K] = { 0 };
    uint8_t pa[FEC_RS_2E], pb[FEC_RS_2E];
    b[100] = 0x01;
    fec_rs_encode (a, pa);
    fec_rs_encode (b, pb);
    DP_CHECK_MSG (memcmp (pa, pb, sizeof pa) != 0,
                  "a single changed information symbol must move the parity");
  }

  DP_TEST_END ("fec_rs");
}
