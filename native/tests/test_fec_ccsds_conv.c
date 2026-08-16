/*
 * test_fec_ccsds_conv.c — the rate-1/2 K=7 convolutional code, held to its
 * connection vectors rather than to itself.
 *
 * The encoder is the clearest case in this whole slice of a transform that a
 * round trip cannot check. CCSDS inverts the G2 output path (131.0-B-3
 * 3.3.1(5)); a matched Viterbi decoder inverts whatever it was handed, so an
 * encoder that omits the inversion decodes its own output flawlessly and
 * interoperates with nothing. The same is true of a mirrored register or a
 * swapped G1/G2 — all self-consistent, all wrong on the air.
 *
 * So every check here is against a value the standard states:
 *
 *   - the IMPULSE RESPONSE is the connection vectors, by definition: feed a
 *     one followed by zeros and C1 traces G1 while C2 traces the COMPLEMENT
 *     of G2. This pins the tap alignment and the inversion together.
 *   - the ALL-ZERO input isolates the inversion on its own: C1 must be all
 *     zeros and C2 must be all ones. Nothing else produces that.
 *   - CONTINUITY, because 3.3.2 specifies an unbroken symbol sequence and a
 *     caller that chunks a long record must not restart the register.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "fec/fec_ccsds.h"

#include <string.h>

/* 131.0-B-3 3.3.1(4): G1 = 1111001, G2 = 1011011, written left-to-right with
 * the newest input stage first — the order the impulse response emits. */
static const uint8_t g1_bits[7] = { 1, 1, 1, 1, 0, 0, 1 };
static const uint8_t g2_bits[7] = { 1, 0, 1, 1, 0, 1, 1 };

int
main (void)
{
  /* ── impulse response: C1 is G1, C2 is ~G2 ──────────────────────────── */
  {
    uint8_t    in[7] = { 1, 0, 0, 0, 0, 0, 0 };
    uint8_t    out[14];
    fec_conv_t s;
    fec_conv_init (&s);
    fec_conv_encode (&s, in, 7, out);

    int c1_ok = 1, c2_ok = 1;
    for (size_t i = 0; i < 7; i++)
      {
        if (out[2 * i] != g1_bits[i])
          c1_ok = 0;
        if (out[2 * i + 1] != (uint8_t)(g2_bits[i] ^ 1u))
          c2_ok = 0;
      }
    DP_CHECK_MSG (c1_ok, "C1 impulse response must trace G1 = 1111001");
    DP_CHECK_MSG (c2_ok,
                  "C2 impulse response must trace ~G2, G2 = 1011011 (3.3.1)");
  }

  /* ── the inversion, isolated ────────────────────────────────────────── */
  {
    uint8_t    in[32] = { 0 };
    uint8_t    out[64];
    fec_conv_t s;
    fec_conv_init (&s);
    fec_conv_encode (&s, in, 32, out);

    int c1_zero = 1, c2_one = 1;
    for (size_t i = 0; i < 32; i++)
      {
        if (out[2 * i] != 0)
          c1_zero = 0;
        if (out[2 * i + 1] != 1)
          c2_one = 0;
      }
    DP_CHECK_MSG (c1_zero, "an all-zero input must give C1 all zeros");
    DP_CHECK_MSG (c2_one, "an all-zero input must give C2 all ONES — 3.3.1(5) "
                          "symbol inversion on the G2 path");
  }

  /* ── 3.3.2: the symbol sequence is continuous across calls ──────────── */
  {
    uint8_t in[20];
    for (size_t i = 0; i < sizeof in; i++)
      in[i] = (uint8_t)((i * 5u + 1u) & 1u);

    uint8_t    whole[40], split[40];
    fec_conv_t a, b;
    fec_conv_init (&a);
    fec_conv_encode (&a, in, 20, whole);

    fec_conv_init (&b);
    fec_conv_encode (&b, in, 8, split);
    fec_conv_encode (&b, in + 8, 12, split + 16);

    DP_CHECK_MSG (memcmp (whole, split, sizeof whole) == 0,
                  "encoding in chunks must equal encoding in one call");
  }

  /* ── the rate is 1/2, and max_out says so ───────────────────────────── */
  {
    uint8_t    in[9] = { 1, 0, 1, 1, 0, 0, 1, 0, 1 };
    uint8_t    out[18];
    fec_conv_t s;
    fec_conv_init (&s);
    const size_t got = fec_conv_encode (&s, in, 9, out);
    DP_CHECK_MSG (got == 18, "9 bits in must be 18 symbols out");
    DP_CHECK_MSG (got == fec_conv_max_out (9),
                  "max_out must agree with what encode actually wrote");
  }

  DP_TEST_END ("fec_ccsds_conv");
}
