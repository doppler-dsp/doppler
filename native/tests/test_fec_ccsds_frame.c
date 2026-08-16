/*
 * test_fec_ccsds_frame.c — the frame assembler, held to the one thing the
 * four kernel tests cannot see between them.
 *
 * Each stage in fec/ is already pinned to a value CCSDS prints. What no
 * kernel test can be wrong about alone is COVERAGE: the marker enters third
 * and the four stages disagree about whether they reach over it.
 *
 *   Reed-Solomon (outer)   does NOT cover the ASM   9.5.1, 9.2.1.5
 *   pseudo-randomiser      does NOT cover the ASM   10.3.2, 10.3.4 note 1
 *   convolutional (inner)  DOES cover the ASM       3.2.1, 9.2.1.4
 *
 * An assembler that gets any one of the three backwards still encodes, still
 * decodes against a matched receiver of its own construction, and syncs to
 * nothing — the same failure mode the rest of this slice was built around, one
 * level up. So the three rows are asserted directly, and each is asserted
 * against something outside this file:
 *
 *   - the ASM, byte for byte, as figure 9-1 prints it;
 *   - the randomiser's published 40-bit prefix, positioned AFTER the marker,
 *     which fails in both halves if the randomiser reached back over it;
 *   - fec_rs_codeword_ok, the syndrome check, which needs no decoder and
 *     fails if the marker was ever presented to the R-S encoder;
 *   - and, for the inner code, a CADU rebuilt here from the published marker
 *     and the published sequence, then convolutionally encoded. That last one
 *     composes kernels this file does not own, but every one of them is
 *     independently pinned to a printed value by its own test — the claim
 *     being checked is the COMPOSITION, which is the only thing left.
 *
 * The data is ZEROS wherever the randomiser is in the path, for the reason
 * test_fec_ccsds_rand gives: a random-looking payload hides a randomiser that
 * did not run.
 */
#define _GNU_SOURCE
#include "dp_test.h"

#include "fec/fec_frame.h"

#include <string.h>

/* 131.0-B-3 figure 9-1, read left to right from "FIRST TRANSMITTED BIT":
 *   0001 1010 1100 1111 1111 1100 0001 1101   = 1ACFFC1D */
static const uint8_t asm_published[32] = {
  0, 0, 0, 1, 1, 0, 1, 0, /* 1A */
  1, 1, 0, 0, 1, 1, 1, 1, /* CF */
  1, 1, 1, 1, 1, 1, 0, 0, /* FC */
  0, 0, 0, 1, 1, 1, 0, 1  /* 1D */
};

/* 131.0-B-3 10.4.2, the printed prefix of the pseudo-random sequence. */
static const uint8_t rand_published40[40] = {
  1, 1, 1, 1, 1, 1, 1, 1, /* FF */
  0, 1, 0, 0, 1, 0, 0, 0, /* 48 */
  0, 0, 0, 0, 1, 1, 1, 0, /* 0E */
  1, 1, 0, 0, 0, 0, 0, 0, /* C0 */
  1, 0, 0, 1, 1, 0, 1, 0  /* 9A */
};

/* Unpacked bits back to octets, MSB-first — the inverse of what the
 * assembler applies on the way in, so the R-S oracle can be fed symbols. */
static void
pack (const uint8_t *bits, size_t nbytes, uint8_t *bytes)
{
  for (size_t i = 0; i < nbytes; i++)
    {
      uint8_t v = 0;
      for (unsigned b = 0; b < 8u; b++)
        v = (uint8_t)(((unsigned)v << 1u) | (bits[i * 8u + b] & 1u));
      bytes[i] = v;
    }
}

int
main (void)
{
  /* ── the coverage table, as spans ───────────────────────────────────── */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    fec_frame_layout_t lay;
    const size_t       n = fec_frame_layout (&cfg, (size_t)FEC_RS_K * 5, &lay);

    DP_REQUIRE_MSG (n == (32u + 255u * 5u * 8u) * 2u,
                    "concatenated depth 5: (ASM + codeblock) * 2 symbols");
    DP_CHECK (lay.block_bits == 255u * 5u * 8u);
    DP_CHECK (lay.cadu_bits == 32u + 255u * 5u * 8u);
    DP_CHECK (lay.out_bits == n);

    /* 9.4.1: the marker immediately precedes the codeblock. */
    DP_CHECK (lay.marker.first == 0 && lay.marker.n == FEC_CCSDS_ASM_BITS);

    /* 9.5.1 / 9.2.1.5: the outer code's data space excludes the marker. */
    DP_CHECK_MSG (lay.outer.first == lay.marker.n
                      && lay.outer.n == lay.block_bits,
                  "9.5.1: the ASM is not in the R-S encoded data space");

    /* 10.3.2 / 10.3.4 note 1: so does the randomiser's. */
    DP_CHECK_MSG (lay.randomised.first == lay.marker.n
                      && lay.randomised.n == lay.block_bits,
                  "10.3.4: the ASM was not randomized");

    /* 3.2.1 / 9.2.1.4: the inner code covers everything, marker included. */
    DP_CHECK_MSG (lay.inner.first == 0 && lay.inner.n == lay.cadu_bits,
                  "9.2.1.4: the ASM shall be convolutionally encoded");

    /* The three rows together, stated as the difference that makes them
       three fields instead of a stage order. */
    DP_CHECK_MSG (lay.inner.first < lay.randomised.first
                      && lay.randomised.first == lay.outer.first,
                  "only the inner code may start at the first CADU bit");
  }

  /* ── the marker is not randomised, and the sequence starts after it ─── */
  {
    /* No outer code, so the randomiser's scope is the Transfer Frame itself
       (10.3.2's third case) and the block is readable straight out. */
    const fec_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    uint8_t      frame[64] = { 0 };
    uint8_t      out[32 + 64 * 8];
    const size_t n = fec_frame_encode (&cfg, frame, sizeof frame, out);

    DP_REQUIRE (n == sizeof out);
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) == 0,
                  "the ASM must appear verbatim — figure 9-1, unrandomised");
    DP_CHECK_MSG (memcmp (out + 32, rand_published40, sizeof rand_published40)
                      == 0,
                  "the sequence must start at the first bit of the BLOCK");

    /* Both halves above fail for the same wrong implementation, in opposite
       ways, which is why both are here: a randomiser given the whole CADU
       emits the marker XORed with FF 48 0E C0, and hands the block the
       sequence from bit 32 (1001 1010...) rather than from bit 0. */
  }

  /* ── 9.5.1: the ASM was never presented to the R-S encoder ──────────── */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 1, .randomise = 0, .attach_asm = 1, .convolutional = 0
    };
    uint8_t frame[FEC_RS_K];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 7u + 1u);

    uint8_t      out[32 + FEC_RS_N * 8];
    const size_t n = fec_frame_encode (&cfg, frame, sizeof frame, out);
    DP_REQUIRE (n == sizeof out);

    /* The information section is systematic and unrandomised here, so it is
       the frame itself, bit for bit, immediately behind the marker. */
    uint8_t info[FEC_RS_K];
    pack (out + 32, sizeof info, info);
    DP_CHECK_MSG (memcmp (info, frame, sizeof frame) == 0,
                  "the information section must pass through unchanged");

    /* The codeblock starts one marker behind where a naive assembler would
       put it. Taken from the right place it is a codeword... */
    uint8_t word[FEC_RS_N];
    pack (out + 32, sizeof word, word);
    DP_CHECK_MSG (fec_rs_codeword_ok (word),
                  "the 255 symbols behind the ASM must form a codeword");

    /* ...and taken from the marker it is not, which is exactly the block an
       assembler that fed the ASM to the outer encoder would have built. */
    uint8_t shifted[FEC_RS_N];
    pack (out, sizeof shifted, shifted);
    DP_CHECK_MSG (!fec_rs_codeword_ok (shifted),
                  "255 symbols starting AT the ASM must not be a codeword");
  }

  /* ── 3.2.1 / 9.2.1.4: the inner code covers the marker ──────────────── */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    uint8_t      frame[32] = { 0 };
    uint8_t      out[(32 + 32 * 8) * 2];
    const size_t n = fec_frame_encode (&cfg, frame, sizeof frame, out);
    DP_REQUIRE (n == sizeof out);

    /* Rebuild the CADU from published pieces: the marker as figure 9-1
       prints it, then the sequence, which is what randomising zeros gives. */
    uint8_t cadu[32 + 32 * 8];
    memcpy (cadu, asm_published, sizeof asm_published);
    fec_ccsds_rand_seq (cadu + 32, 32 * 8);

    uint8_t    want[(32 + 32 * 8) * 2];
    fec_conv_t s;
    fec_conv_init (&s);
    fec_conv_encode (&s, cadu, sizeof cadu, want);

    DP_CHECK_MSG (memcmp (out, want, sizeof want) == 0,
                  "the whole CADU, marker included, must be inner-encoded");

    /* The direct falsification of the other order: attach the marker after
       the inner code and it survives verbatim at the head of the symbols. */
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) != 0,
                  "3.2.1: the ASM is inserted BEFORE convolutional encoding");
  }

  /* ── the whole concatenated chain, against the code's own property ──── */
  {
    /* Everything on except the inner code, so the CADU can be read back and
       each interleaved codeword checked without a decoder. */
    const unsigned        depth = 5;
    const fec_frame_cfg_t cfg   = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    /* NOT zeros here, unlike the randomiser checks above: the all-zero
       codeword IS a codeword, so a zero frame would satisfy the syndrome
       check below no matter what the interleaver and the encoder did. */
    uint8_t frame[FEC_RS_K * 5];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 31u + 11u);
    uint8_t out[32 + FEC_RS_N * 5 * 8];

    const size_t n = fec_frame_encode (&cfg, frame, sizeof frame, out);
    DP_REQUIRE (n == sizeof out);
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) == 0,
                  "a fully concatenated CADU still opens with the marker");

    /* Derandomise the block — and only the block, which is the receiving
       end of 10.3.4: "after locating the ASM, the data immediately
       following the ASM shall be derandomized". */
    fec_ccsds_randomise (out + 32, (size_t)FEC_RS_N * depth * 8u);

    uint8_t blk[FEC_RS_N * 5];
    pack (out + 32, sizeof blk, blk);

    /* 4.4.1: S1/S2 hand successive symbols to successive encoders, so
       codeword e is every depth-th symbol of each section. */
    int all_ok = 1;
    for (unsigned e = 0; e < depth; e++)
      {
        uint8_t word[FEC_RS_N];
        for (unsigned i = 0; i < FEC_RS_K; i++)
          word[i] = blk[i * depth + e];
        for (unsigned p = 0; p < FEC_RS_2E; p++)
          word[FEC_RS_K + p] = blk[FEC_RS_K * depth + p * depth + e];
        if (!fec_rs_codeword_ok (word))
          all_ok = 0;
      }
    DP_CHECK_MSG (all_ok, "every de-interleaved codeword must have zero "
                          "syndromes after derandomisation");
  }

  /* ── the degenerate config pins the packing convention on its own ───── */
  {
    const fec_frame_cfg_t cfg      = { 0 };
    const uint8_t         frame[2] = { 0x1Au, 0xCFu };
    uint8_t               out[16];
    const size_t          n = fec_frame_encode (&cfg, frame, 2, out);

    DP_REQUIRE (n == 16);
    DP_CHECK_MSG (memcmp (out, asm_published, 16) == 0,
                  "octets go out MSB-first: 0x1ACF is the ASM's first 16 "
                  "bits, so an LSB-first unpack cannot pass this");
  }

  /* ── lengths across every allowed depth ─────────────────────────────── */
  {
    static const unsigned depths[6] = { 1, 2, 3, 4, 5, 8 };
    int                   sizes_ok  = 1;
    for (size_t i = 0; i < 6; i++)
      {
        const fec_frame_cfg_t cfg  = { .rs_depth      = depths[i],
                                       .randomise     = 1,
                                       .attach_asm    = 1,
                                       .convolutional = 1 };
        const size_t          want = (32u + 255u * depths[i] * 8u) * 2u;
        if (fec_frame_layout (&cfg, (size_t)FEC_RS_K * depths[i], NULL)
            != want)
          sizes_ok = 0;
      }
    DP_CHECK_MSG (sizes_ok, "(ASM + 255*I octets) * 2 for every allowed I");
  }

  /* ── refusals, rather than a codeblock nobody is configured for ─────── */
  {
    uint8_t frame[FEC_RS_K * 3] = { 0 };
    uint8_t out[8];
    memset (out, 0xAAu, sizeof out);

    const fec_frame_cfg_t d7 = { .rs_depth = 7 };
    DP_CHECK_MSG (fec_frame_layout (&d7, (size_t)FEC_RS_K * 7, NULL) == 0,
                  "4.3.5.1 allows 1, 2, 3, 4, 5 and 8 — not 7");

    const fec_frame_cfg_t d3 = { .rs_depth = 3 };
    DP_CHECK_MSG (fec_frame_layout (&d3, (size_t)FEC_RS_K * 2, NULL) == 0,
                  "a frame that is not K*I octets must be refused, not "
                  "padded — virtual fill is not implemented");
    DP_CHECK_MSG (fec_frame_encode (&d3, frame, (size_t)FEC_RS_K * 2, out)
                      == 0,
                  "encode must refuse whatever layout refuses");

    const fec_frame_cfg_t none = { 0 };
    DP_CHECK (fec_frame_layout (&none, 0, NULL) == 0);
    DP_CHECK (fec_frame_encode (&none, frame, 0, out) == 0);

    int untouched = 1;
    for (size_t i = 0; i < sizeof out; i++)
      if (out[i] != 0xAAu)
        untouched = 0;
    DP_CHECK_MSG (untouched, "a refused encode must not write to out");
  }

  DP_TEST_END ("fec_ccsds_frame");
}
