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
 * Nothing here includes `ccsds_tm`. The CCSDS configuration is one row in
 * the
 * table below, and holding it to the STANDARD's printed values is
 * test_ccsds_tm_conv.c's job.
 *
 * Generality is asserted rather than assumed: the sections below run k from 3
 * to 9 and n from 1 to 3, because a decoder that only works at the one
 * configuration it was developed against is a CCSDS decoder wearing a
 * parameter list.
 *
 * The design is docs/design/viterbi.md.
 */
#include "dp_rng_test.h"
#include "dp_state_test.h"
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

  /* ── 5b. the LLR sign convention, against something outside this file ──
   *
   * `viterbi_decode`'s header fixes the convention — "positive means symbol
   * 0" — and claims the decoder "agrees with `mpsk_demap` on hard decisions
   * by construction". Nothing here could see that: every section above feeds
   * LLRs through this file's own `to_llr`, which shares the convention, so a
   * decoder and a helper that flipped TOGETHER would pass all of them. That
   * is the same shared-convention blind spot the encoder's impulse-response
   * test exists to avoid on the other side.
   *
   * The IDENTITY code closes it without importing anything: `k = 2, n = 1,
   * poly = {0b10}` taps only the newest input, so the encoder is the identity
   * and a maximum-likelihood decode of it is precisely a hard slicer. The
   * decoded bits must therefore equal `llr < 0` element for element — a
   * statement about the LLR definition alone, which is what `mpsk_demap`
   * also implements and why the two agree by construction rather than by
   * coincidence.
   */
  {
    enum
    {
      N     = 500,
      DEPTH = 8
    };
    const conv_code_t ident
        = { .k = 2u, .n = 1u, .poly = { 2u }, .invert = 0u };
    DP_REQUIRE (conv_code_valid (&ident));

    float    llr[N];
    uint8_t  dec[N];
    uint32_t st = 31337u;
    for (int i = 0; i < N; i++)
      {
        /* Magnitudes vary so the test is about the SIGN and not about a
           decoder that happens to threshold at some level. */
        const double u = dp_uni (&st);
        llr[i]         = (float)((u - 0.5) * (0.2 + 4.0 * dp_uni (&st)));
        if (llr[i] == 0.0f)
          llr[i] = 0.25f;
      }

    viterbi_state_t *v = viterbi_create (&ident, DEPTH);
    DP_REQUIRE (v != NULL);
    const size_t got = viterbi_decode (v, llr, N, dec, sizeof dec);
    viterbi_destroy (v);

    /* The latency, against a LITERAL rather than against the sizing
       function. §4 already pins `viterbi_decode == viterbi_decode_max_out`,
       so the two agree with each other by construction and neither could
       see that the header's prose said `depth` where the traceback walks
       `depth - 1` — measured here as 493 bits from 500 symbols at depth 8,
       and the prose is what moved. */
    DP_CHECK_MSG (got == (size_t)N - (DEPTH - 1),
                  "a decision needs depth-1 branches behind it, and the "
                  "header says so");
    int agree = 1, both = 0;
    for (size_t i = 0; i < got; i++)
      {
        const uint8_t hard = llr[i] < 0.0f ? 1u : 0u;
        if (dec[i] != hard)
          agree = 0;
        both |= (dec[i] ? 2 : 1);
      }
    DP_CHECK_MSG (agree,
                  "positive LLR means symbol 0: the identity code's decode "
                  "must equal the hard slice, bit for bit");
    /* ...and both symbols must occur, or a decoder stuck at 0 would pass a
       stream that happened to be all zeros. */
    DP_CHECK_MSG (both == 3, "the check must have seen both symbols");
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

  /* ── 6b. node synchronization: the re-encoding metric ──────────────────
   *
   * The claims here are the ones a metric cannot satisfy by agreeing with
   * itself, because none of them mentions the transmitted bits:
   *
   *   - the RIGHT alignment scores the channel's errors and nothing else, so
   *     on a clean stream it scores ZERO;
   *   - a wrong alignment scores a large fraction of the symbols, because
   *     the decoder is searching a trellis its input does not lie on. NOT a
   *     half: the decoder is a maximum LIKELIHOOD search and finds whatever
   *     codeword agrees best with the misaligned stream, so measured it is
   *     24 % for CCSDS, 23 % uninverted and 18 % at rate 1/3. The floor
   *     below is a tenth, which every code clears with room and which no
   *     broken metric would;
   *   - the metric is BLIND TO POLARITY, exactly and not approximately: a
   *     transparent code decodes an inverted stream to the complement, which
   *     re-encodes to the inverted symbols. The counts must be equal, and if
   *     they are not the metric is reading something other than alignment;
   *   - it works at every rate, so the scan runs over n hypotheses and not
   *     over two.
   */
  {
    enum
    {
      N = 800
    };
    static uint8_t in[N], sym[3 * N];
    static float   llr[3 * N + 4];

    /* Three codes: CCSDS, an uninverted rate 1/2, and a rate 1/3 -- the last
       is what makes "n hypotheses" a test rather than a spelling. */
    const conv_code_t codes[3]
        = { CCSDS,
            { .k = 7u, .n = 2u, .poly = { 0171u, 0133u }, .invert = 0u },
            { .k = 5u, .n = 3u, .poly = { 025u, 033u, 037u }, .invert = 0u } };

    for (size_t ci = 0; ci < 3u; ci++)
      {
        const conv_code_t *c  = &codes[ci];
        uint32_t           st = 24601u + (uint32_t)ci;
        for (int i = 0; i < N; i++)
          in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

        conv_enc_t e;
        conv_enc_init (&e);
        const size_t ns = conv_encode (&e, c, in, N, sym, sizeof sym);
        to_llr (sym, ns, llr, 4.0f);

        viterbi_state_t *v = viterbi_create (c, 60u);
        DP_REQUIRE (v != NULL);

        /* On a clean stream the aligned score is the channel's error count,
           which is none. */
        DP_CHECK_MSG (node_sync_score (v, llr, ns) == 0,
                      "in sync on a clean stream, the re-encode must agree "
                      "with the received symbols exactly");

        /* Every OTHER alignment must be far away — see the note above on
           why the floor is a tenth and not a half. */
        node_sync_t ns_res;
        DP_REQUIRE (node_sync_scan (v, llr, ns, &ns_res));
        DP_CHECK_MSG (ns_res.phase == 0u,
                      "the scan must pick the alignment the stream is on");
        DP_CHECK_MSG (ns_res.errors == 0u, "...at zero errors");
        DP_CHECK_MSG (ns_res.next > ns_res.symbols / 10u,
                      "...and every other hypothesis must be far off");
        DP_CHECK_MSG (ns_res.margin == ns_res.next,
                      "the margin is what separates them");

        /* Shift the stream by one symbol and the answer must move by one,
           for every rate -- this is the slip case, and it is why the scan
           takes its window rather than holding state. */
        node_sync_t shifted;
        DP_REQUIRE (node_sync_scan (v, llr + 1, ns - 1u, &shifted));
        DP_CHECK_MSG (shifted.phase == c->n - 1u,
                      "a one-symbol slip must move the winning phase by one");
        DP_CHECK_MSG (shifted.errors == 0u, "...and still score zero");

        /* Polarity: invert every symbol. The code is transparent only when
           every generator has odd weight, which is a property of the code
           rather than of the metric -- so this asserts EQUALITY where that
           holds and only that the phase still wins where it does not. */
        for (size_t i = 0; i < ns; i++)
          llr[i] = -llr[i];
        const size_t inv         = node_sync_score (v, llr, ns);
        int          transparent = 1;
        for (unsigned j = 0; j < c->n; j++)
          {
            unsigned w = 0, poly = c->poly[j];
            while (poly)
              {
                w += poly & 1u;
                poly >>= 1;
              }
            if ((w & 1u) == 0u)
              transparent = 0;
          }
        if (transparent)
          DP_CHECK_MSG (inv == 0u,
                        "a transparent code's metric must be blind to "
                        "polarity -- exactly, not approximately");
        for (size_t i = 0; i < ns; i++)
          llr[i] = -llr[i];

        viterbi_destroy (v);
      }
  }

  /* ── 6c. in sync, the metric IS the channel symbol error rate ──────────
   *
   * The claim the design rests on (`docs/design/viterbi.md` §9): a caller
   * reads the aligned count as a channel statistic, not merely as a
   * comparator. So put a KNOWN number of symbol errors in and require the
   * count back. It cannot be exact -- the decoder corrects, and a corrected
   * error still disagrees with the received symbol while a MIScorrection
   * adds disagreements the channel did not put there -- so the assertion is
   * that it tracks within a fifth, which is far tighter than the half-window
   * separation the decision rests on.
   */
  {
    enum
    {
      N     = 2000,
      EVERY = 40
    };
    static uint8_t in[N], sym[2 * N];
    static float   llr[2 * N];
    uint32_t       st = 777u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);

    conv_enc_t e;
    conv_enc_init (&e);
    conv_encode (&e, &CCSDS, in, N, sym, sizeof sym);
    to_llr (sym, 2u * N, llr, 4.0f);

    size_t put = 0;
    for (size_t i = 7; i < 2u * N; i += EVERY)
      {
        llr[i] = -llr[i];
        put++;
      }

    viterbi_state_t *v = viterbi_create (&CCSDS, 60u);
    DP_REQUIRE (v != NULL);
    const size_t got = node_sync_score (v, llr, 2u * N);
    viterbi_destroy (v);

    DP_CHECK_MSG (got * 5u >= put * 4u && got * 4u <= put * 5u,
                  "the aligned count must track the channel's symbol errors");
  }

  /* ── 6d. the free distance, which is the code's own published number ───
   *
   * `docs/design/viterbi.md` §8 lists this as an UNKNOWN: "whether
   * `d_free = 10` is exhibited... a property of the code, so it is checkable
   * against the implementation rather than against another implementation —
   * the strongest kind of assertion available here". This is that check.
   *
   * `d_free` is the minimum Hamming weight of a nonzero codeword, which for a
   * convolutional code is the lightest path that leaves the all-zero state
   * and returns to it. Computed here by relaxation over the trellis the
   * DESCRIPTION defines — `conv_outputs` and `conv_next_state`, the same two
   * a decoder builds from — and compared against values the literature
   * publishes for these codes. Nothing in doppler can choose them.
   *
   * The inversion is removed first, and that is not a convenience: a
   * constant XOR on every branch cancels in the DIFFERENCE between two
   * codewords, so it cannot move a distance. Leaving it in would measure the
   * weight of the all-zero path instead, which is `n` per branch and is not
   * a distance at all.
   */
  {
    const struct
    {
      conv_code_t code;
      unsigned    d_free;
      const char *who;
    } known[3] = {
      /* CCSDS 131.0-B-3 §3.3's inner code: the (171, 133) K=7, and 10 is the
         number 130.1-G quotes when it prints the curves. */
      { CCSDS, 10u, "CCSDS K=7 (171,133) must have d_free = 10" },
      /* The textbook K=3 (7,5), d_free = 5. */
      { { .k = 3u, .n = 2u, .poly = { 07u, 05u }, .invert = 0u },
        5u,
        "K=3 (7,5) must have d_free = 5" },
      /* K=4 (15,17), d_free = 6. */
      { { .k = 4u, .n = 2u, .poly = { 015u, 017u }, .invert = 0u },
        6u,
        "K=4 (15,17) must have d_free = 6" },
    };

    for (size_t ki = 0; ki < 3u; ki++)
      {
        const conv_code_t *c = &known[ki].code;
        const uint32_t     S = conv_states (c);
        unsigned           w[1u << (CONV_K_MAX - 1)];

        /* Weight of a branch, with the inversion taken back out. */
#define BRANCH_W(st, b)                                                       \
  (unsigned)__builtin_popcount ((conv_outputs (c, (st), (b)) ^ c->invert)     \
                                & ((1u << c->n) - 1u))

        const unsigned INF = 0xFFFFu;
        for (uint32_t st = 0; st < S; st++)
          w[st] = INF;

        /* Leave the all-zero state on a 1 — every nonzero codeword starts
           that way — and relax until nothing moves. */
        w[conv_next_state (c, 0u, 1u)] = BRANCH_W (0u, 1u);
        for (uint32_t pass = 0; pass < S + 2u; pass++)
          {
            int moved = 0;
            for (uint32_t st = 0; st < S; st++)
              {
                if (w[st] == INF)
                  continue;
                for (unsigned b = 0; b < 2u; b++)
                  {
                    const uint32_t ns = conv_next_state (c, st, b);
                    if (ns == 0u && b == 0u)
                      continue; /* the merge itself is scored below */
                    const unsigned cand = w[st] + BRANCH_W (st, b);
                    if (cand < w[ns])
                      {
                        w[ns] = cand;
                        moved = 1;
                      }
                  }
              }
            if (!moved)
              break;
          }

        /* The merge back to zero, on a 0, from wherever is cheapest. */
        unsigned d = INF;
        for (uint32_t st = 0; st < S; st++)
          {
            if (w[st] == INF || conv_next_state (c, st, 0u) != 0u)
              continue;
            const unsigned cand = w[st] + BRANCH_W (st, 0u);
            if (cand < d)
              d = cand;
          }
#undef BRANCH_W

        DP_CHECK_MSG (d == known[ki].d_free, known[ki].who);
      }
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

  /* ── 8. the state bytes interface: resume is bit-exact ─────────────────
   *
   * The claim is that a decode split anywhere and resumed from a blob into a
   * FRESH decoder produces the same bits as one uninterrupted decode. A
   * decoder is a link in a chain -- behind the receiver, in front of the R-S
   * decoder -- and a chain is only checkpointable if every link is.
   *
   * Two cuts, because they catch different omissions. The steady-state cut
   * exercises the path metrics and the ring; the cut inside the first
   * `depth` bits is the only one that can see `fill` being dropped, since a
   * decoder resumed there still owes its traceback and must emit NOTHING for
   * the bits it has not yet earned.
   */
  {
    enum
    {
      N = 600
    };
    const size_t DEPTH = 60u;
    uint8_t      in[N], sym[2 * N], ref[N], got[N];
    float        llr[2 * N];
    uint32_t     st = 90210u;
    for (int i = 0; i < N; i++)
      in[i] = (uint8_t)(dp_xs32 (&st) & 1u);
    conv_enc_t e;
    conv_enc_init (&e);
    conv_encode (&e, &CCSDS, in, N, sym, sizeof sym);
    to_llr (sym, 2u * N, llr, 3.0f);

    viterbi_state_t *a = viterbi_create (&CCSDS, DEPTH);
    DP_REQUIRE (a != NULL);
    const size_t n_ref = viterbi_decode (a, llr, 2u * N, ref, sizeof ref);
    DP_REQUIRE (n_ref == (size_t)N - (DEPTH - 1u));

    /* In SYMBOLS: 200 steps (steady state) and 20 steps (fill < depth). */
    const size_t cuts[] = { 400u, 40u };
    for (size_t ci = 0; ci < sizeof cuts / sizeof *cuts; ci++)
      {
        const size_t     cut = cuts[ci];
        viterbi_state_t *b   = viterbi_create (&CCSDS, DEPTH);
        DP_REQUIRE (b != NULL);
        const size_t n1 = viterbi_decode (b, llr, cut, got, sizeof got);

        void *blob = malloc (viterbi_state_bytes (b));
        DP_REQUIRE (blob != NULL);
        viterbi_get_state (b, blob);
        viterbi_destroy (b); /* the sender is GONE: only the blob carries it */

        viterbi_state_t *c = viterbi_create (&CCSDS, DEPTH);
        DP_REQUIRE (c != NULL);
        DP_CHECK (viterbi_set_state (c, blob) == DP_OK);

        const size_t owed = viterbi_decode_max_out (c, 2u * N - cut);
        const size_t n2 = viterbi_decode (c, llr + cut, 2u * N - cut, got + n1,
                                          sizeof got - n1);
        DP_CHECK_MSG (n2 == owed,
                      "a resumed decoder must owe exactly what its fill says");
        DP_CHECK_MSG (n1 + n2 == n_ref,
                      "a split stream must emit as many bits as one decode");
        DP_CHECK_MSG (memcmp (got, ref, n_ref) == 0,
                      "resume from a blob must be bit-exact");
        viterbi_destroy (c);
        free (blob);
      }

    /* The shared round-trip: fidelity (b re-serializes to a's bytes) plus
       the envelope reject. */
    viterbi_state_t *r2 = viterbi_create (&CCSDS, DEPTH);
    DP_REQUIRE (r2 != NULL);
    DP_STATE_ROUNDTRIP_TEST (viterbi, a, r2);

    /* Config identity, which the envelope's SIZE check cannot supply. Each
       of these decoders is built for a different code, and the first two
       produce a blob of exactly the same length as a's. */
    void *cb = malloc (viterbi_state_bytes (a));
    DP_REQUIRE (cb != NULL);
    viterbi_get_state (a, cb);

    conv_code_t uninverted = CCSDS;
    uninverted.invert      = 0u;
    conv_code_t otherpoly  = CCSDS;
    otherpoly.poly[0]      = 0155u; /* still k bits, still non-zero */
    const conv_code_t K9   = { 9u, 2u, { 0753u, 0561u }, 0u };

    const struct
    {
      const conv_code_t *code;
      size_t             depth;
      const char        *why;
    } rejects[] = {
      { &uninverted, 60u, "a blob from a code differing only in `invert`" },
      { &otherpoly, 60u, "a blob from a code differing only in a polynomial" },
      { &CCSDS, 61u, "a blob from a decoder of another depth" },
      { &K9, 60u, "a K=7 blob restored into a K=9 decoder" },
    };
    for (size_t i = 0; i < sizeof rejects / sizeof *rejects; i++)
      {
        viterbi_state_t *w
            = viterbi_create (rejects[i].code, rejects[i].depth);
        DP_REQUIRE (w != NULL);
        if (i < 2u)
          DP_REQUIRE_MSG (viterbi_state_bytes (w) == viterbi_state_bytes (a),
                          "this reject must be the SAME size, or it proves "
                          "only that the envelope checks length");
        DP_CHECK_MSG (viterbi_set_state (w, cb) == DP_ERR_INVALID,
                      rejects[i].why);
        viterbi_destroy (w);
      }

    /* A cursor out of range would be traced back through memory the ring
       does not own, and the envelope cannot see it -- the blob is the right
       object, version, endianness and size. The payload opens with
       `depth, head, fill` as three u64s, which the first assertion PROVES
       rather than assumes: if the layout ever moves, this reads a number
       that is not the depth and fails here, instead of quietly testing
       nothing. */
    {
      uint64_t     field[3];
      const size_t base = sizeof (dp_state_hdr_t);
      memcpy (field, (const uint8_t *)cb + base, sizeof field);
      DP_REQUIRE_MSG (field[0] == (uint64_t)DEPTH,
                      "the payload no longer opens with the depth -- this "
                      "test's offsets are stale");

      const uint64_t bad_cursor[2] = { (uint64_t)DEPTH, (uint64_t)DEPTH + 1u };
      const char    *why[2]
          = { "head must be inside the ring", "fill must not exceed depth" };
      for (size_t i = 0; i < 2u; i++)
        {
          void *poison = malloc (viterbi_state_bytes (a));
          DP_REQUIRE (poison != NULL);
          memcpy (poison, cb, viterbi_state_bytes (a));
          memcpy ((uint8_t *)poison + base + (i + 1u) * sizeof (uint64_t),
                  &bad_cursor[i], sizeof (uint64_t));
          DP_CHECK_MSG (viterbi_set_state (r2, poison) == DP_ERR_INVALID,
                        why[i]);
          free (poison);
        }
    }

    /* And the guard is not simply refusing everything: the same blob still
       restores into the decoder it came from. */
    DP_CHECK (viterbi_set_state (r2, cb) == DP_OK);

    free (cb);
    viterbi_destroy (r2);
    viterbi_destroy (a);
  }

  DP_TEST_END ("test_conv_core");
}
