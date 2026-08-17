/*
 * test_conv_core.c — the convolutional codec: the code description, the
 * encoder, and the decoder that must agree with it.
 *
 * The external truth is the IMPULSE RESPONSE, which is what a generator
 * polynomial means: drive a 1 followed by zeros and output j must trace
 * poly[j] bit for bit, inverted where the code says so. It is checkable for
 * every configuration rather than only the one this was developed against,
 * and it is not a round trip -- a decoder matched to a wrong encoder decodes
 * perfectly and interoperates with nothing, which is the failure this whole
 * slice is built to refuse.
 *
 * Nothing here includes `fec`. The CCSDS configuration is one row in the
 * table below, and holding it to the STANDARD's printed values is
 * test_fec_ccsds_conv.c's job.
 *
 * Generality is asserted rather than assumed: the sections below run k from 3
 * to 9 and n from 1 to 3, because a decoder that only works at the one
 * configuration it was developed against is a CCSDS decoder wearing a
 * parameter list.
 *
 * The design is docs/design/viterbi.md.
 */
#include "dp_rng_test.h"
#include "dp_test.h"

#include "conv/conv_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* CCSDS 131.0-B-3 section 3.3, as a conv_code_t: G1 = 171, G2 = 133 octal,
   with the inversion on the SECOND output and nothing else. */
static const conv_code_t CCSDS
    = { .k = 7u, .n = 2u, .poly = { 0171u, 0133u }, .invert = 0x2u };

/* Soft symbols from hard ones, at a confidence the decoder cannot doubt. */
static void
to_llr (const uint8_t *sym, size_t n, float *llr, float mag)
{
  for (size_t i = 0; i < n; i++)
    llr[i] = (sym[i] & 1u) ? -mag : mag;
}

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

  /* ── 4. the decoder returns what was encoded, for EVERY code tried ─────
   *
   * Noiseless, so any failure is the trellis rather than the channel. Swept
   * over k and n because a decoder that only works at k = 7, n = 2 is a
   * CCSDS decoder with a parameter list -- and the butterfly, the branch
   * metric table and the traceback all have k or n in them.
   */
  {
    static const conv_code_t CODES[] = {
      { 3u, 2u, { 07u, 05u }, 0u },            /* the classic K=3 r=1/2   */
      { 4u, 2u, { 017u, 013u }, 0u },          /* K=4                     */
      { 7u, 2u, { 0171u, 0133u }, 0x2u },      /* CCSDS, inverted output  */
      { 9u, 2u, { 0753u, 0561u }, 0u },        /* K=9, 256 states         */
      { 7u, 3u, { 0171u, 0165u, 0133u }, 0u }, /* rate 1/3                */
      { 5u, 1u, { 023u }, 0u },                /* rate 1/1: still a code  */
      { 7u, 2u, { 0171u, 0133u }, 0x3u },      /* BOTH outputs inverted   */
    };
    enum
    {
      N = 400
    };
    uint8_t in[N];
    uint8_t sym[N * CONV_N_MAX];
    float   llr[N * CONV_N_MAX];
    uint8_t dec[N];

    for (size_t ci = 0; ci < sizeof CODES / sizeof CODES[0]; ci++)
      {
        const conv_code_t *c     = &CODES[ci];
        const size_t       depth = 5u * c->k + 25u;
        uint32_t           st    = 1000u + (uint32_t)ci;
        for (int i = 0; i < N; i++)
          in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

        conv_enc_t e;
        conv_enc_init (&e);
        const size_t ns = conv_encode (&e, c, in, N, sym, sizeof sym);
        DP_REQUIRE (ns == (size_t)N * c->n);
        to_llr (sym, ns, llr, 8.0f);

        viterbi_state_t *v = viterbi_create (c, depth);
        DP_REQUIRE (v != NULL);
        DP_CHECK (viterbi_depth (v) == depth);
        DP_CHECK (viterbi_code (v)->k == c->k);

        const size_t want = viterbi_decode_max_out (v, ns);
        const size_t got  = viterbi_decode (v, llr, ns, dec, sizeof dec);
        DP_CHECK_MSG (got == want,
                      "decode must emit exactly what max_out predicted");

        /* Streaming delays by depth-1, so bit i of the output is input bit
           i. What is missing is the TAIL, not the head. */
        int bad = 0;
        for (size_t i = 0; i < got; i++)
          if (dec[i] != in[i])
            bad++;
        DP_CHECK_MSG (bad == 0, "noiseless decode must be exact, every code");
        DP_CHECK_MSG (got + depth - 1u == (size_t)N,
                      "and the only bits owed are the traceback's");
        viterbi_destroy (v);
      }
  }

  /* ── 5. the decoder is not fooled by its own scale ─────────────────────
   *
   * A maximum-likelihood path cannot move when every branch metric is
   * multiplied by a positive constant. This is what lets a caller with no
   * SNR estimate pass unscaled LLRs, and it is asserted rather than assumed
   * because a decoder that normalised wrongly would still look right at one
   * scale.
   */
  {
    enum
    {
      N = 300
    };
    uint8_t  in[N], sym[2 * N], d1[N], d2[N];
    float    l1[2 * N], l2[2 * N];
    uint32_t st = 31337u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);
    conv_enc_t e;
    conv_enc_init (&e);
    conv_encode (&e, &CCSDS, in, N, sym, sizeof sym);

    /* noisy, so the decision is not trivially unanimous */
    uint32_t ns = 999u;
    for (size_t i = 0; i < 2u * N; i++)
      {
        const float clean = (sym[i] & 1u) ? -1.0f : 1.0f;
        l1[i]             = clean + 0.9f * (float)dp_gauss (&ns);
        l2[i]             = 137.0f * l1[i];
      }

    viterbi_state_t *v = viterbi_create (&CCSDS, 60u);
    DP_REQUIRE (v != NULL);
    const size_t g1 = viterbi_decode (v, l1, 2u * N, d1, sizeof d1);
    viterbi_reset (v);
    const size_t g2 = viterbi_decode (v, l2, 2u * N, d2, sizeof d2);
    DP_CHECK (g1 == g2);
    DP_CHECK_MSG (memcmp (d1, d2, g1) == 0,
                  "scaling every LLR cannot move the maximum-likelihood path");
    viterbi_destroy (v);
  }

  /* ── 6. it CORRECTS, which is the only reason it exists ────────────────
   *
   * A decoder that merely inverted the encoder would pass every section
   * above. Flip channel symbols and require the output to survive: the
   * CCSDS code has free distance 10, so isolated errors well inside a
   * constraint window are correctable, and this asserts that they are
   * corrected rather than merely tolerated.
   */
  {
    enum
    {
      N = 600
    };
    uint8_t  in[N], sym[2 * N], dec[N];
    float    llr[2 * N];
    uint32_t st = 5150u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);
    conv_enc_t e;
    conv_enc_init (&e);
    conv_encode (&e, &CCSDS, in, N, sym, sizeof sym);
    to_llr (sym, 2u * N, llr, 4.0f);

    /* One symbol flipped every 60 -- sparse enough that no constraint window
       holds more than the code can carry. */
    int flipped = 0;
    for (size_t i = 25; i < 2u * N; i += 60)
      {
        llr[i] = -llr[i];
        flipped++;
      }
    DP_REQUIRE (flipped > 15);

    viterbi_state_t *v = viterbi_create (&CCSDS, 60u);
    DP_REQUIRE (v != NULL);
    const size_t got = viterbi_decode (v, llr, 2u * N, dec, sizeof dec);
    int          bad = 0;
    for (size_t i = 0; i < got; i++)
      if (dec[i] != in[i])
        bad++;
    DP_CHECK_MSG (bad == 0,
                  "isolated channel errors must be CORRECTED, not carried");
    viterbi_destroy (v);
  }

  /* ── 7. the refusals, each verified by a poisoned buffer ───────────────*/
  {
    uint8_t     in[8] = { 0 }, out[16];
    float       llr[16];
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

    DP_CHECK (viterbi_create (&bad, 60u) == NULL);
    DP_CHECK_MSG (viterbi_create (&CCSDS, 0u) == NULL,
                  "depth 0 is not a decoder");
    viterbi_destroy (NULL); /* a no-op, not a crash */

    viterbi_state_t *v = viterbi_create (&CCSDS, 4u);
    DP_REQUIRE (v != NULL);
    for (size_t i = 0; i < 16; i++)
      llr[i] = 1.0f;
    memset (out, 0xAA, sizeof out);
    DP_CHECK_MSG (viterbi_decode (v, llr, 15u, out, sizeof out) == 0,
                  "a symbol count that is not a multiple of n must refuse");
    DP_CHECK_MSG (viterbi_decode (v, llr, 16u, out, 1u) == 0,
                  "a short output buffer must refuse");
    for (size_t i = 0; i < sizeof out; i++)
      DP_CHECK (out[i] == 0xAAu);
    viterbi_destroy (v);
  }

  DP_TEST_END ("test_conv_core");
}
