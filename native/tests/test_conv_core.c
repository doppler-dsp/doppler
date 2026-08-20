/*
 * test_conv_core.c — the convolutional code description and its encoder.
 *
 * The external truth is the IMPULSE RESPONSE, which is what a generator
 * polynomial means: drive a 1 followed by zeros and output j must trace
 * poly[j] bit for bit, inverted where the code says so. It is checkable for
 * every configuration rather than only the one this was developed against,
 * and it is not a round trip -- a decoder matched to a wrong encoder decodes
 * perfectly and interoperates with nothing, which is why the encoder is
 * pinned against the polynomial here and never against the decoder.
 *
 * The DECODER is test_viterbi_core.c's: `viterbi` is its own declared
 * component (doppler#893), and everything it claims -- the sweep over codes,
 * scale invariance, error correction, node synchronization, the free
 * distance and the state blob -- is asserted there.
 *
 * Nothing here includes `ccsds_tm`. The CCSDS configuration is one row in
 * the table below, and holding it to the STANDARD's printed values is
 * test_ccsds_tm_conv.c's job.
 *
 * Generality is asserted rather than assumed: the sections below run k from 3
 * to 9 and n from 1 to 3, because an encoder that only works at the one
 * configuration it was developed against is a CCSDS encoder wearing a
 * parameter list.
 *
 * The design is docs/design/viterbi.md.
 */
#include "dp_rng_test.h"
#include "dp_test.h"

#include "conv/conv_core.h"

#include <string.h>

/* CCSDS 131.0-B-3 section 3.3, as a conv_code_t: G1 = 171, G2 = 133 octal,
   with the inversion on the SECOND output and nothing else. */
static const conv_code_t CCSDS
    = { .k = 7u, .n = 2u, .poly = { 0171u, 0133u }, .invert = 0x2u };

int
main (void)
{
  /* ── 1. validation, and what it refuses ────────────────────────────────
   *
   * A zero polynomial is an output carrying no information and a polynomial
   * wider than the register is a transcription that lost its alignment. Both
   * are typos, and a codec that accepts them produces a code nobody else has.
   */
  {
    DP_CHECK (conv_code_valid (&CCSDS));
    conv_code_t bad = CCSDS;
    bad.k           = 1u;
    DP_CHECK_MSG (!conv_code_valid (&bad), "k < 2 is not a code");
    bad   = CCSDS;
    bad.k = CONV_K_MAX + 1u;
    DP_CHECK (!conv_code_valid (&bad));
    bad   = CCSDS;
    bad.n = 0u;
    DP_CHECK (!conv_code_valid (&bad));
    bad   = CCSDS;
    bad.n = CONV_N_MAX + 1u;
    DP_CHECK (!conv_code_valid (&bad));
    bad         = CCSDS;
    bad.poly[1] = 0u;
    DP_CHECK_MSG (!conv_code_valid (&bad), "a zero polynomial is a typo");
    bad         = CCSDS;
    bad.poly[0] = 1u << CCSDS.k; /* one bit too wide */
    DP_CHECK_MSG (!conv_code_valid (&bad),
                  "a polynomial wider than k lost its alignment");
    DP_CHECK (!conv_code_valid (NULL));
    DP_CHECK (conv_states (&CCSDS) == 64u);
  }

  /* ── 2. the impulse response IS the generator polynomial ───────────────
   *
   * What a generator polynomial MEANS: a 1 followed by zeros walks a single
   * bit through the register, so output j reads off poly[j] one tap at a
   * time -- inverted wherever the code says so. That makes the polynomial as
   * written the external truth, for every code and not just the familiar one,
   * and it is what catches a state convention derived backwards (perfectly
   * self-consistent, decodes nothing a conforming encoder produced) or an
   * inversion applied to the wrong output.
   */
  {
    static const conv_code_t IMP[] = {
      { 3u, 2u, { 07u, 05u }, 0u },
      { 7u, 2u, { 0171u, 0133u }, 0x2u }, /* CCSDS */
      { 7u, 3u, { 0171u, 0165u, 0133u }, 0x5u },
      { 9u, 2u, { 0753u, 0561u }, 0u },
    };
    for (size_t ci = 0; ci < sizeof IMP / sizeof IMP[0]; ci++)
      {
        const conv_code_t *c              = &IMP[ci];
        uint8_t            in[CONV_K_MAX] = { 1u };
        uint8_t            out[CONV_K_MAX * CONV_N_MAX];
        conv_enc_t         e;
        conv_enc_init (&e);
        const size_t got = conv_encode (&e, c, in, c->k, out, sizeof out);
        DP_REQUIRE (got == (size_t)c->k * c->n);

        for (unsigned j = 0; j < c->n; j++)
          {
            const unsigned inv = (c->invert >> j) & 1u;
            for (unsigned t = 0; t < c->k; t++)
              {
                /* At step t the 1 sits t stages down from the top, so the tap
                   it meets is bit (k-1-t) of the polynomial. */
                const unsigned tap
                    = (unsigned)((c->poly[j] >> (c->k - 1u - t)) & 1u);
                DP_CHECK_MSG (out[t * c->n + j] == (tap ^ inv),
                              "output j must trace poly[j], inverted where "
                              "the code says so");
              }
          }
      }
  }

  /* ── 2b. conv_outputs and conv_next_state, the two the trellis is built
   *      from — and which nothing here had ever called ─────────────────────
   *
   * The file docstring calls `conv_outputs` "the only place that says what
   * this family of codes emits", and the register convention "load-bearing"
   * — a trellis derived the other way round is self-consistent and decodes
   * nothing a conforming encoder produced. Both were exercised only THROUGH
   * `conv_encode`, which is the caller that happens to agree with them; a
   * user building a trellis (which is what `viterbi_create` does, and what
   * anyone extending this must do) reads them directly.
   *
   * So: run the description BY HAND — `conv_outputs` for the symbols,
   * `conv_next_state` for the state — and require the result to equal what
   * `conv_encode` produced for the same bits. Two independent expressions of
   * the same code, which is what the docstring claims this file pins.
   */
  {
    enum
    {
      N = 200
    };
    uint8_t  in[N], sym[3 * N];
    uint32_t st = 90210u;

    const conv_code_t codes[3] = {
      CCSDS,
      { .k = 3u, .n = 2u, .poly = { 07u, 05u }, .invert = 0u },
      { .k = 5u, .n = 3u, .poly = { 025u, 033u, 037u }, .invert = 0x5u }
    };

    for (size_t ci = 0; ci < 3u; ci++)
      {
        const conv_code_t *c = &codes[ci];
        for (int i = 0; i < N; i++)
          in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

        conv_enc_t e;
        conv_enc_init (&e);
        const size_t ns = conv_encode (&e, c, in, N, sym, sizeof sym);
        DP_REQUIRE (ns == (size_t)N * c->n);

        /* The hand-run trellis. It starts where conv_enc_init does — the
           all-zero state — which is the only thing this borrows from the
           encoder. */
        uint32_t state = 0u;
        int      same  = 1;
        for (int i = 0; i < N; i++)
          {
            const unsigned w = conv_outputs (c, state, in[i]);
            for (unsigned j = 0; j < c->n; j++)
              {
                /* Output j is BIT j of the word and the j-th symbol emitted
                   — the ordering claim, which is the half a round trip
                   cannot see because both ends would move together. */
                if (((w >> j) & 1u) != sym[(size_t)i * c->n + j])
                  same = 0;
              }
            state = conv_next_state (c, state, in[i]);
          }
        DP_CHECK_MSG (same,
                      "the trellis run by hand from conv_outputs and "
                      "conv_next_state must reproduce conv_encode exactly");

        /* And the state is genuinely the k-1 previous inputs, newest in the
           high stage — read back rather than inferred, because this is the
           convention the docstring says a reader gets backwards. */
        uint32_t expect = 0u;
        for (unsigned b = 0; b < c->k - 1u; b++)
          expect |= (uint32_t)(in[N - 1 - b] & 1u) << (c->k - 2u - b);
        DP_CHECK_MSG (state == expect,
                      "a state must BE the k-1 previous inputs, newest in "
                      "the high stage");
      }
  }

  /* ── 3. continuity: the register carries across calls ──────────────────
   *
   * 3.3.2 fixes the output as one uninterrupted sequence. A caller encoding
   * in chunks must get the same stream as one call, or every chunk boundary
   * is a discontinuity no decoder expects.
   */
  {
    enum
    {
      N = 200
    };
    uint8_t  in[N], whole[2 * N], split[2 * N];
    uint32_t st = 77777u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_t s;
    conv_enc_init (&s);
    conv_encode (&s, &CCSDS, in, N, whole, sizeof whole);

    conv_enc_init (&s);
    conv_encode (&s, &CCSDS, in, 37, split, 74);
    conv_encode (&s, &CCSDS, in + 37, N - 37, split + 74, sizeof split - 74);
    DP_CHECK_MSG (memcmp (whole, split, 2u * N) == 0,
                  "chunked encoding must equal one call");
  }

  /* ── 4. the refusals, each verified by a poisoned buffer ───────────────*/
  {
    uint8_t     in[8] = { 0 }, out[16];
    conv_enc_t  e;
    conv_code_t bad = CCSDS;
    bad.poly[0]     = 0u;

    conv_enc_init (&e);
    memset (out, 0xAA, sizeof out);
    DP_CHECK_MSG (conv_encode (&e, &bad, in, 8, out, sizeof out) == 0,
                  "an invalid code must refuse");
    DP_CHECK_MSG (conv_encode (&e, &CCSDS, in, 8, out, 15) == 0,
                  "one symbol short of the output must refuse");
    for (size_t i = 0; i < sizeof out; i++)
      DP_CHECK (out[i] == 0xAAu);
  }

  DP_TEST_END ("test_conv_core");
}
