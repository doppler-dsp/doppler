/*
 * test_ccsds_tm_frame.c — the frame assembler, held to the one thing the
 * four kernel tests cannot see between them.
 *
 * Each stage in ccsds_tm/ is already pinned to a value CCSDS prints. What no
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
 *   - ccsds_tm_rs_codeword_ok, the syndrome check, which needs no decoder and
 *     fails if the marker was ever presented to the R-S encoder;
 *   - and, for the inner code, a CADU rebuilt here from the published marker
 *     and the published sequence, then convolutionally encoded. That last one
 *     composes kernels this file does not own, but every one of them is
 *     independently pinned to a printed value by its own test — the claim
 *     being checked is the COMPOSITION, which is the only thing left.
 *
 * The data is ZEROS wherever the randomiser is in the path, for the reason
 * test_ccsds_tm_rand gives: a random-looking payload hides a randomiser that
 * did not run.
 */
#include "dp_test.h"

#include "ccsds_tm/ccsds_tm_frame.h"
#include "wfm/wfm_frame.h"

#include <string.h>

/* 131.0-B-3 figure 9-1, read left to right from "FIRST TRANSMITTED BIT":
 *   0001 1010 1100 1111 1111 1100 0001 1101   = 1ACFFC1D */
static const uint8_t asm_published[32] = {
  0, 0, 0, 1, 1, 0, 1, 0, /* 1A */
  1, 1, 0, 0, 1, 1, 1, 1, /* CF */
  1, 1, 1, 1, 1, 1, 0, 0, /* FC */
  0, 0, 0, 1, 1, 1, 0, 1  /* 1D */
};

/* 131.0-B-6 10.4.3 note 2, the printed prefix of the DEFAULT pseudo-random
 * sequence -- 10.4.1's 131071-bit generator, not the 255-bit one B-6 keeps
 * for legacy systems. The assembler applies whichever ccsds_tm_randomise
 * applies, so this is also what pins WHICH randomiser the frame path used. */
static const uint8_t rand_published40[40] = {
  0, 0, 0, 1, 1, 1, 0, 0, /* 1C */
  0, 1, 1, 1, 0, 0, 0, 1, /* 71 */
  1, 0, 1, 1, 1, 0, 0, 1, /* B9 */
  0, 0, 0, 1, 1, 0, 1, 1, /* 1B */
  1, 0, 1, 0, 1, 0, 0, 1  /* A9 */
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
  /* ── 1. the coverage table, as spans ──────────────────────────────────── */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    ccsds_tm_frame_layout_t lay;
    const size_t            n
        = ccsds_tm_frame_layout (&cfg, (size_t)CCSDS_TM_RS_K * 5, &lay);

    DP_REQUIRE_MSG (n == (32u + 255u * 5u * 8u) * 2u,
                    "concatenated depth 5: (ASM + codeblock) * 2 symbols");
    DP_CHECK (lay.block_bits == 255u * 5u * 8u);
    DP_CHECK (lay.cadu_bits == 32u + 255u * 5u * 8u);
    DP_CHECK (lay.out_bits == n);

    /* 9.4.1: the marker immediately precedes the codeblock. */
    DP_CHECK (lay.marker.first == 0 && lay.marker.n == CCSDS_TM_ASM_BITS);

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

  /* ── 2. the marker is not randomised, and the sequence starts after it ── */
  {
    /* No outer code, so the randomiser's scope is the Transfer Frame itself
       (10.3.2's third case) and the block is readable straight out. */
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    uint8_t      frame[64] = { 0 };
    uint8_t      out[32 + 64 * 8];
    const size_t n = ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame,
                                            out, sizeof out);

    DP_REQUIRE (n == sizeof out);
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) == 0,
                  "the ASM must appear verbatim — figure 9-1, unrandomised");
    DP_CHECK_MSG (memcmp (out + 32, rand_published40, sizeof rand_published40)
                      == 0,
                  "the sequence must start at the first bit of the BLOCK");

    /* Both halves above fail for the same wrong implementation, in opposite
       ways, which is why both are here: a randomiser given the whole CADU
       emits the marker XORed with the sequence's first 32 bits, and hands
       the block the sequence from bit 32 rather than from bit 0. */
  }

  /* ── 3. 9.5.1: the ASM was never presented to the R-S encoder ─────────── */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 1, .randomise = 0, .attach_asm = 1, .convolutional = 0
    };
    uint8_t frame[CCSDS_TM_RS_K];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 7u + 1u);

    uint8_t      out[32 + CCSDS_TM_RS_N * 8];
    const size_t n = ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame,
                                            out, sizeof out);
    DP_REQUIRE (n == sizeof out);

    /* The information section is systematic and unrandomised here, so it is
       the frame itself, bit for bit, immediately behind the marker. */
    uint8_t info[CCSDS_TM_RS_K];
    pack (out + 32, sizeof info, info);
    DP_CHECK_MSG (memcmp (info, frame, sizeof frame) == 0,
                  "the information section must pass through unchanged");

    /* The codeblock starts one marker behind where a naive assembler would
       put it. Taken from the right place it is a codeword... */
    uint8_t word[CCSDS_TM_RS_N];
    pack (out + 32, sizeof word, word);
    DP_CHECK_MSG (ccsds_tm_rs_codeword_ok (word),
                  "the 255 symbols behind the ASM must form a codeword");

    /* ...and taken from the marker it is not, which is exactly the block an
       assembler that fed the ASM to the outer encoder would have built. */
    uint8_t shifted[CCSDS_TM_RS_N];
    pack (out, sizeof shifted, shifted);
    DP_CHECK_MSG (!ccsds_tm_rs_codeword_ok (shifted),
                  "255 symbols starting AT the ASM must not be a codeword");
  }

  /* ── 4. 3.2.1 / 9.2.1.4: the inner code covers the marker ─────────────── */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    uint8_t      frame[32] = { 0 };
    uint8_t      out[(32 + 32 * 8) * 2];
    const size_t n = ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame,
                                            out, sizeof out);
    DP_REQUIRE (n == sizeof out);

    /* Rebuild the CADU from published pieces: the marker as figure 9-1
       prints it, then the sequence, which is what randomising zeros gives. */
    uint8_t cadu[32 + 32 * 8];
    memcpy (cadu, asm_published, sizeof asm_published);
    ccsds_tm_rand_seq (cadu + 32, 32 * 8);

    uint8_t    want[(32 + 32 * 8) * 2];
    conv_enc_t s;
    conv_enc_init (&s);
    conv_encode (&s, &CCSDS_TM_CONV, cadu, sizeof cadu, want, sizeof want);

    DP_CHECK_MSG (memcmp (out, want, sizeof want) == 0,
                  "the whole CADU, marker included, must be inner-encoded");

    /* The direct falsification of the other order: attach the marker after
       the inner code and it survives verbatim at the head of the symbols. */
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) != 0,
                  "3.2.1: the ASM is inserted BEFORE convolutional encoding");
  }

  /* ── 5. the whole concatenated chain, against the code's own property ─── */
  {
    /* Everything on except the inner code, so the CADU can be read back and
       each interleaved codeword checked without a decoder. */
    const unsigned             depth = 5;
    const ccsds_tm_frame_cfg_t cfg   = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    /* NOT zeros here, unlike the randomiser checks above: the all-zero
       codeword IS a codeword, so a zero frame would satisfy the syndrome
       check below no matter what the interleaver and the encoder did. */
    uint8_t frame[CCSDS_TM_RS_K * 5];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 31u + 11u);
    uint8_t out[32 + CCSDS_TM_RS_N * 5 * 8];

    const size_t n = ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame,
                                            out, sizeof out);
    DP_REQUIRE (n == sizeof out);
    DP_CHECK_MSG (memcmp (out, asm_published, sizeof asm_published) == 0,
                  "a fully concatenated CADU still opens with the marker");

    /* Derandomise the block — and only the block, which is the receiving
       end of 10.3.4: "after locating the ASM, the data immediately
       following the ASM shall be derandomized". */
    ccsds_tm_randomise (out + 32, (size_t)CCSDS_TM_RS_N * depth * 8u);

    uint8_t blk[CCSDS_TM_RS_N * 5];
    pack (out + 32, sizeof blk, blk);

    /* 4.4.1: S1/S2 hand successive symbols to successive encoders, so
       codeword e is every depth-th symbol of each section. */
    int all_ok = 1;
    for (unsigned e = 0; e < depth; e++)
      {
        uint8_t word[CCSDS_TM_RS_N];
        for (unsigned i = 0; i < CCSDS_TM_RS_K; i++)
          word[i] = blk[i * depth + e];
        for (unsigned p = 0; p < CCSDS_TM_RS_2E; p++)
          word[CCSDS_TM_RS_K + p] = blk[CCSDS_TM_RS_K * depth + p * depth + e];
        if (!ccsds_tm_rs_codeword_ok (word))
          all_ok = 0;
      }
    DP_CHECK_MSG (all_ok, "every de-interleaved codeword must have zero "
                          "syndromes after derandomisation");
  }

  /* ── 6. the degenerate config pins the packing convention on its own ──── */
  {
    const ccsds_tm_frame_cfg_t cfg      = { 0 };
    const uint8_t              frame[2] = { 0x1Au, 0xCFu };
    uint8_t                    out[16];
    const size_t               n
        = ccsds_tm_frame_encode (&cfg, NULL, frame, 2, out, sizeof out);

    DP_REQUIRE (n == 16);
    DP_CHECK_MSG (memcmp (out, asm_published, 16) == 0,
                  "octets go out MSB-first: 0x1ACF is the ASM's first 16 "
                  "bits, so an LSB-first unpack cannot pass this");
  }

  /* ── 7. lengths across every allowed depth ────────────────────────────── */
  {
    static const unsigned depths[6] = { 1, 2, 3, 4, 5, 8 };
    int                   sizes_ok  = 1;
    for (size_t i = 0; i < 6; i++)
      {
        const ccsds_tm_frame_cfg_t cfg  = { .rs_depth      = depths[i],
                                            .randomise     = 1,
                                            .attach_asm    = 1,
                                            .convolutional = 1 };
        const size_t               want = (32u + 255u * depths[i] * 8u) * 2u;
        if (ccsds_tm_frame_layout (&cfg, (size_t)CCSDS_TM_RS_K * depths[i],
                                   NULL)
            != want)
          sizes_ok = 0;
      }
    DP_CHECK_MSG (sizes_ok, "(ASM + 255*I octets) * 2 for every allowed I");
  }

  /* ── 8. refusals, rather than a codeblock nobody is configured for ────── */
  {
    uint8_t frame[CCSDS_TM_RS_K * 3] = { 0 };
    uint8_t out[8];
    memset (out, 0xAAu, sizeof out);

    const ccsds_tm_frame_cfg_t d7 = { .rs_depth = 7 };
    DP_CHECK_MSG (ccsds_tm_frame_layout (&d7, (size_t)CCSDS_TM_RS_K * 7, NULL)
                      == 0,
                  "4.3.5.1 allows 1, 2, 3, 4, 5 and 8 — not 7");

    const ccsds_tm_frame_cfg_t d3 = { .rs_depth = 3 };
    DP_CHECK_MSG (ccsds_tm_frame_layout (&d3, (size_t)CCSDS_TM_RS_K * 2, NULL)
                      == 0,
                  "a frame that is not K*I octets must be refused, not "
                  "padded — virtual fill is not implemented");
    DP_CHECK_MSG (ccsds_tm_frame_encode (&d3, NULL, frame,
                                         (size_t)CCSDS_TM_RS_K * 2, out,
                                         sizeof out)
                      == 0,
                  "encode must refuse whatever layout refuses");

    const ccsds_tm_frame_cfg_t none = { 0 };
    DP_CHECK (ccsds_tm_frame_layout (&none, 0, NULL) == 0);
    DP_CHECK (ccsds_tm_frame_encode (&none, NULL, frame, 0, out, sizeof out)
              == 0);

    int untouched = 1;
    for (size_t i = 0; i < sizeof out; i++)
      if (out[i] != 0xAAu)
        untouched = 0;
    DP_CHECK_MSG (untouched, "a refused encode must not write to out");

    /* The CADU is assembled in the TAIL of `out`, so a short buffer is a
       write past the end rather than a truncated answer. Refusing on capacity
       is what makes that unreachable, and it is checked with a buffer one
       symbol short of sufficient rather than a wildly small one. */
    const ccsds_tm_frame_cfg_t ok1 = { .rs_depth = 1, .attach_asm = 1 };
    uint8_t                    room[32 + 255 * 8];
    memset (room, 0xAAu, sizeof room);
    const size_t need = ccsds_tm_frame_layout (&ok1, CCSDS_TM_RS_K, NULL);
    DP_REQUIRE (need == sizeof room);
    DP_CHECK_MSG (ccsds_tm_frame_encode (&ok1, NULL, frame, CCSDS_TM_RS_K,
                                         room, need - 1u)
                      == 0,
                  "a buffer one symbol short must be refused, not written");
    for (size_t i = 0; i < sizeof room; i++)
      if (room[i] != 0xAAu)
        untouched = 0;
    DP_CHECK_MSG (untouched, "and it must not have written anything");
    DP_CHECK (
        ccsds_tm_frame_encode (&ok1, NULL, frame, CCSDS_TM_RS_K, room, need)
        == need);
  }

  /* ── 9. a STREAM of frames: the inner code does not restart ────────────────
   *
   * 3.3.2 fixes the output as one uninterrupted symbol sequence. The kernel
   * test already proves chunked == whole when the register is carried
   * (test_ccsds_tm_conv, "split"); this is the same claim one layer up,
   * where the assembler is the caller that has to carry it.
   *
   * It is asserted as an EQUALITY against the continuous encoding of the two
   * CADUs, and the NULL form is measured beside it rather than merely being
   * different — 6 symbols, all inside the first 7 of frame 2, which is the
   * K-1 = 6 bits of register memory and lands on the ASM. Without the
   * equality this is invisible: every other test in this file encodes one
   * frame, and one frame is exactly the case that cannot see it. */
  {
    const unsigned             depth = 1;
    const size_t               flen  = (size_t)CCSDS_TM_RS_K * depth;
    const ccsds_tm_frame_cfg_t coded = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    const ccsds_tm_frame_cfg_t bare = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };

    uint8_t a[CCSDS_TM_RS_K], b[CCSDS_TM_RS_K];
    for (size_t i = 0; i < flen; i++)
      {
        a[i] = (uint8_t)(i * 7u + 1u);
        b[i] = (uint8_t)(i * 31u + 5u);
      }

    const size_t nsym = ccsds_tm_frame_layout (&coded, flen, NULL);
    const size_t ncad = ccsds_tm_frame_layout (&bare, flen, NULL);
    DP_REQUIRE (nsym == 2u * ncad);

    static uint8_t carried[2 * (32 + 255 * 8) * 2];
    static uint8_t restart[2 * (32 + 255 * 8) * 2];
    static uint8_t cadu[2 * (32 + 255 * 8)];
    static uint8_t cont[2 * (32 + 255 * 8) * 2];

    /* (1) a transmitter's loop, carrying the register */
    conv_enc_t s;
    conv_enc_init (&s);
    ccsds_tm_frame_encode (&coded, &s, a, flen, carried, nsym);
    ccsds_tm_frame_encode (&coded, &s, b, flen, carried + nsym, nsym);

    /* (2) the same two CADUs as ONE continuous encode — the external truth,
           built from the uncoded assembler and the kernel, neither of which
           knows anything about frames */
    ccsds_tm_frame_encode (&bare, NULL, a, flen, cadu, ncad);
    ccsds_tm_frame_encode (&bare, NULL, b, flen, cadu + ncad, ncad);
    conv_enc_t t;
    conv_enc_init (&t);
    conv_encode (&t, &CCSDS_TM_CONV, cadu, 2u * ncad, cont, sizeof cont);

    DP_CHECK_MSG (memcmp (carried, cont, 2u * nsym) == 0,
                  "3.3.2: a stream of frames must equal one continuous "
                  "encode of the same CADUs");

    /* (3) and NULL is the single-frame form, which restarts — measured, so
           the cost of getting this wrong is a number in the record */
    ccsds_tm_frame_encode (&coded, NULL, a, flen, restart, nsym);
    ccsds_tm_frame_encode (&coded, NULL, b, flen, restart + nsym, nsym);
    DP_CHECK_MSG (memcmp (restart, cont, nsym) == 0,
                  "frame 1 is identical either way — only the seam moves");

    /* The BOUND is the physics and the count is not: how many of the symbols
       inside the seam actually differ depends on which register states the
       two encodings hold, i.e. on the payload. So the assertion is the span —
       K-1 = 6 bits of memory at rate 1/2 is 12 symbols — with "at least one"
       to keep it from passing vacuously if the two ever became identical.
       Measured here: 6 differing symbols, the last at offset 7. */
    size_t diff = 0, last = 0;
    for (size_t i = nsym; i < 2u * nsym; i++)
      if (restart[i] != cont[i])
        {
          diff++;
          last = i - nsym;
        }
    DP_CHECK_MSG (diff > 0 && last < 2u * (CCSDS_TM_CONV_K - 1u),
                  "restarting costs the K-1 bits of register memory and no "
                  "more: every difference inside the first 12 symbols");
  }

  /* ── 10. the decoder undoes the encoder, over the SAME spans ───────────────
   *
   * A round trip is weak evidence on its own -- it is what this whole slice
   * refuses to rely on -- so it is not the claim here. The claim is that
   * `ccsds_tm_frame_decode` reads the same span table `ccsds_tm_frame_encode`
   * wrote, and the checks below are chosen to fail if it does not:
   *
   *   - the payload is ZEROS, so a decoder that skipped the randomiser
   *     returns the sequence instead of the frame, loudly (a PN payload
   *     would hide it, per test_ccsds_tm_rand's opening);
   *   - the marker is CLOBBERED before decoding, so a decoder that
   *     derandomised or R-S-checked from bit 0 instead of from behind it
   *     cannot pass -- and one that reads the spans correctly cannot even
   *     notice.
   */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    ccsds_tm_frame_layout_t lay;
    const size_t n = ccsds_tm_frame_layout (&cfg, CCSDS_TM_RS_K * 5, &lay);
    DP_REQUIRE (n != 0 && n == lay.cadu_bits);

    static uint8_t frame[CCSDS_TM_RS_K * 5] = { 0 };
    static uint8_t cadu[(32 + CCSDS_TM_RS_N * 5 * 8)];
    DP_REQUIRE (ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame, cadu,
                                       sizeof cadu)
                == lay.cadu_bits);

    /* Nothing after this point may depend on the marker's contents. */
    for (size_t i = 0; i < lay.marker.n; i++)
      cadu[i] ^= 1u;

    static uint8_t      back[CCSDS_TM_RS_K * 5];
    ccsds_tm_frame_rx_t rx = { 999u, 999u, 999u, 999u, 999u };
    memset (back, 0xAA, sizeof back);
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof back);
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "the decoder must recover the frame from behind a marker "
                  "it does not read");
    DP_CHECK_MSG (rx.rs_codewords == 5u && rx.rs_ok == 5u,
                  "every interleaved codeword must pass its syndrome check");
    DP_CHECK (rx.frame_len == sizeof back);
  }

  /* ── 11. a corrupted symbol is CORRECTED, and a hopeless one is reported ───
   *
   * The outer code corrects up to E = 16 symbols per codeword, so one
   * flipped bit inside codeword 2 must come back repaired -- frame byte for
   * byte, with the repair counted. That is a stronger de-interleave proof
   * than the syndrome check it replaces: a wrong rotation repairs the wrong
   * column, which damages data that was never hit.
   *
   * The payload here is STRUCTURED, and that is load-bearing. Everywhere
   * else in this file it is zeros so a missing randomiser cannot hide; but
   * R-S of an all-zero block is all-zero parity, so every interleaved column
   * is identical and a rotated de-interleave is the IDENTITY. Zeros make one
   * defect visible and another one invisible, so this section pays for the
   * second with data whose columns differ.
   */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    ccsds_tm_frame_layout_t lay;
    ccsds_tm_frame_layout (&cfg, CCSDS_TM_RS_K * 5, &lay);

    static uint8_t frame[CCSDS_TM_RS_K * 5];
    static uint8_t cadu[(32 + CCSDS_TM_RS_N * 5 * 8)];
    static uint8_t back[CCSDS_TM_RS_K * 5];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 37u + 11u);
    ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu);

    /* Clean first: with columns that differ, this is what fails the moment
       the de-interleave rotates. */
    ccsds_tm_frame_rx_t rx = { 0, 0, 0, 0, 0 };
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof back);
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "structured data must round-trip byte for byte");
    DP_CHECK_MSG (rx.rs_ok == 5u && rx.rs_codewords == 5u,
                  "every codeword must check out when the de-interleave "
                  "pairs each column with its own parity");

    /* Symbol index 2 of the block is codeword 2 at depth 5 (S1 hands encoder
       e every 5th symbol starting at e). Its first bit is block bit 16. */
    cadu[lay.randomised.first + 16u] ^= 1u;
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof back);
    DP_CHECK_MSG (rx.rs_codewords == 5u && rx.rs_ok == 5u,
                  "one damaged symbol must be REPAIRED, not just reported");
    DP_CHECK_MSG (rx.rs_corrected == 1u && rx.rs_symbols == 1u,
                  "...in exactly one codeword, at exactly one symbol");
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "...and the frame must come back byte for byte");
    cadu[lay.randomised.first + 16u] ^= 1u;

    /* What the interleaver is FOR, at the frame level: a contiguous burst of
       5*E symbols lands as exactly E in each of the five codewords, which is
       the boundary of what each can repair. One symbol more in any column is
       past it. */
    for (unsigned s = 0; s < 5u * CCSDS_TM_RS_E; s++)
      cadu[lay.randomised.first + (size_t)s * 8u] ^= 1u;
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof back);
    DP_CHECK_MSG (rx.rs_ok == 5u && rx.rs_corrected == 5u
                      && rx.rs_symbols == 5u * CCSDS_TM_RS_E,
                  "a burst of 5*E symbols must be fully repaired at depth 5");
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "...and the frame must be exactly what was sent");
    for (unsigned s = 0; s < 5u * CCSDS_TM_RS_E; s++)
      cadu[lay.randomised.first + (size_t)s * 8u] ^= 1u;

    /* Past the radius in ONE column: E+1 symbols of codeword 2. The decoder
       must refuse that codeword and say so rather than hand back a frame it
       cannot vouch for -- the counts are the caller's protection. */
    for (unsigned c = 0; c <= CCSDS_TM_RS_E; c++)
      cadu[lay.randomised.first + ((size_t)c * 5u + 2u) * 8u] ^= 1u;
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof back);
    DP_CHECK_MSG (rx.rs_codewords == 5u && rx.rs_ok == 4u,
                  "E+1 errors in one column must leave that codeword bad");
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) != 0,
                  "...and the frame it returns must be the wrong one it "
                  "just reported, not a silently repaired copy");
  }

  /* ── 12. with no outer code the frame is the block ────────────────────── */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    ccsds_tm_frame_layout_t lay;
    uint8_t                 frame[64] = { 0 };
    uint8_t                 cadu[32 + 64 * 8];
    uint8_t                 back[64];
    ccsds_tm_frame_rx_t     rx = { 0, 9u, 9u, 9u, 9u };

    ccsds_tm_frame_layout (&cfg, sizeof frame, &lay);
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 7u + 3u);
    DP_REQUIRE (ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame, cadu,
                                       sizeof cadu)
                == lay.cadu_bits);
    DP_REQUIRE (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                       sizeof back, &rx)
                == sizeof frame);
    DP_CHECK (memcmp (back, frame, sizeof frame) == 0);
    DP_CHECK_MSG (rx.rs_codewords == 0u && rx.rs_ok == 0u,
                  "no outer code means no codewords to report");
  }

  /* ── 13. the refusals, each verified by a poisoned buffer ─────────────── */
  {
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    ccsds_tm_frame_layout_t lay;
    ccsds_tm_frame_layout (&cfg, CCSDS_TM_RS_K * 5, &lay);

    static uint8_t frame[CCSDS_TM_RS_K * 5] = { 0 };
    static uint8_t cadu[(32 + CCSDS_TM_RS_N * 5 * 8)];
    static uint8_t back[CCSDS_TM_RS_K * 5];
    ccsds_tm_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu);

    memset (back, 0xAA, sizeof back);
    DP_CHECK_MSG (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits - 8u, back,
                                         sizeof back, NULL)
                      == 0,
                  "a CADU that is not the layout's length must refuse");
    DP_CHECK_MSG (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits - 1u, back,
                                         sizeof back, NULL)
                      == 0,
                  "a block that is not a whole number of octets must refuse");
    DP_CHECK_MSG (ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                         sizeof back - 1u, NULL)
                      == 0,
                  "a short frame buffer must refuse");
    for (size_t i = 0; i < sizeof back; i++)
      DP_CHECK (back[i] == 0xAAu);
  }

  /* ── 14. the coverage table as a wfm_frame_desc_t (docs/design/…) ──────────
   *
   * `ccsds_tm_frame_layout_t` reports a span per stage because a stage ORDER
   * cannot express what covers the marker. `wfm_frame_desc_t` is that same
   * idea with the fields and stages supplied rather than fixed, and this is
   * the check that the general form really does express this one: a
   * description built here must reproduce the shipped layout's four spans and
   * both lengths EXACTLY, for a configuration with every stage on and for one
   * with two of them off.
   *
   * It is the layout half of the generalization's falsification. The other
   * half is byte-for-byte output, which needs a general assembler and is not
   * claimed here -- a right coverage table says nothing about whether the
   * stages were applied to the right bits.
   */
  {
    /* Fields in wire order; the check symbols are a FIELD, derived by the
       outer code, which is what removes any need for a stage to expand what
       it covers. Indices are named because every cover below is a range of
       them and an off-by-one would silently move a span. */
    enum
    {
      F_ASM = 0,
      F_PAYLOAD,
      F_PARITY,
      N_FIELD
    };
    enum
    {
      S_OUTER = 0,
      S_RAND,
      S_INNER,
      N_STAGE
    };

    static const struct
    {
      const char          *what;
      ccsds_tm_frame_cfg_t cfg;
      size_t               frame_octets;
    } CASES[] = {
      { "concatenated, depth 5",
        { .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 1 },
        (size_t)CCSDS_TM_RS_K * 5 },
      { "marker and randomiser only",
        { .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0 },
        64 },
      { "outer code only, no marker",
        { .rs_depth = 2, .randomise = 0, .attach_asm = 0, .convolutional = 0 },
        (size_t)CCSDS_TM_RS_K * 2 },
    };

    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; c++)
      {
        const ccsds_tm_frame_cfg_t *cfg = &CASES[c].cfg;
        ccsds_tm_frame_layout_t     want;
        const size_t                n
            = ccsds_tm_frame_layout (cfg, CASES[c].frame_octets, &want);
        DP_REQUIRE_MSG (n != 0, CASES[c].what);

        wfm_frame_desc_t d;
        memset (&d, 0, sizeof d);
        d.n_fields = N_FIELD;
        d.n_stages = N_STAGE;

        /* The marker is a literal field, and it is field ZERO -- which is the
           whole content of 9.2.1.5 once the covers below are read. */
        d.field[F_ASM].seq.kind     = WFM_SEQ_LITERAL;
        d.field[F_ASM].seq.len      = cfg->attach_asm ? CCSDS_TM_ASM_BITS : 0u;
        d.field[F_PAYLOAD].seq.kind = WFM_SEQ_LITERAL;
        d.field[F_PAYLOAD].seq.len  = CASES[c].frame_octets * 8u;
        d.field[F_PARITY].bits = (size_t)CCSDS_TM_RS_2E * cfg->rs_depth * 8u;
        d.field[F_PARITY].derived_by = S_OUTER + 1u;

        /* 9.5.1: the outer code's data space is the payload and the check
           symbols it derives -- and NOT the marker. */
        d.stage[S_OUTER].kind        = WFM_STAGE_RS;
        d.stage[S_OUTER].depth       = cfg->rs_depth;
        d.stage[S_OUTER].first_field = F_PAYLOAD;
        d.stage[S_OUTER].n_fields    = cfg->rs_depth ? 2u : 0u;

        /* 10.3.4 note 1: the randomiser covers the codeblock, same span. */
        d.stage[S_RAND].kind        = WFM_STAGE_RANDOMISE;
        d.stage[S_RAND].first_field = F_PAYLOAD;
        d.stage[S_RAND].n_fields    = cfg->randomise ? 2u : 0u;

        /* 9.2.1.4: the inner code covers everything, marker included, and it
           is the one stage that emits a different stream. */
        d.stage[S_INNER].kind        = WFM_STAGE_CONV;
        d.stage[S_INNER].first_field = F_ASM;
        d.stage[S_INNER].n_fields    = cfg->convolutional ? N_FIELD : 0u;
        d.stage[S_INNER].emit_num    = 2u;
        d.stage[S_INNER].emit_den    = 1u;

        wfm_frame_desc_layout_t got;
        DP_REQUIRE (wfm_frame_desc_layout (&d, &got) == 0);

        DP_CHECK_MSG (got.frame_bits == want.cadu_bits,
                      "the description's frame is the CADU");
        DP_CHECK_MSG (got.out_bits == want.out_bits,
                      "...and its output is the channel symbols");
        DP_CHECK_MSG (got.field_off[F_ASM] == want.marker.first
                          && got.field_bits[F_ASM] == want.marker.n,
                      "the marker field must land where the ASM does");
        DP_CHECK_MSG (got.stage[S_OUTER].first == want.outer.first
                          && got.stage[S_OUTER].n == want.outer.n,
                      "9.5.1: the outer cover must be the R-S data space");
        DP_CHECK_MSG (got.stage[S_RAND].first == want.randomised.first
                          && got.stage[S_RAND].n == want.randomised.n,
                      "10.3.4: the randomised cover must exclude the ASM");
        DP_CHECK_MSG (got.stage[S_INNER].first == want.inner.first
                          && got.stage[S_INNER].n == want.inner.n,
                      "9.2.1.4: the inner cover must include the ASM");
      }
  }

  /* ── 15. the SAME BITS, from the description ───────────────────────────────
   *
   * The section above proves the general description reproduces this
   * component's coverage table. A right coverage table says nothing about
   * whether the stages were applied to the right bits, so this is the other
   * half and the one that matters: `wfm_frame_assemble` over
   * `ccsds_tm_frame_describe`'s description must equal
   * `ccsds_tm_frame_encode`'s output BYTE FOR BYTE.
   *
   * It is the strongest check available here precisely because it is not a
   * round trip. `ccsds_tm_frame_encode` is already falsified against the
   * values 131.0-B-3 prints -- the marker as figure 9-1 draws it, the
   * randomiser's published prefix, Annex G's generator, the impulse response
   * of the inner code -- so equalling it inherits all of that. A
   * generalization that agreed only with itself would prove nothing, which is
   * this whole slice's opening argument applied one level up.
   *
   * The payload is STRUCTURED, not zeros: with an all-zero frame the R-S
   * parity is all-zero too, every interleaved column is identical, and a
   * description that rotated the codeblock wrongly would still match.
   */
  {
    static const struct
    {
      const char          *what;
      ccsds_tm_frame_cfg_t cfg;
      size_t               octets;
    } CASES[] = {
      { "concatenated, depth 5",
        { .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 1 },
        (size_t)CCSDS_TM_RS_K * 5 },
      { "concatenated, depth 1",
        { .rs_depth = 1, .randomise = 1, .attach_asm = 1, .convolutional = 1 },
        CCSDS_TM_RS_K },
      { "outer code and marker, no randomiser, no inner",
        { .rs_depth = 2, .randomise = 0, .attach_asm = 1, .convolutional = 0 },
        (size_t)CCSDS_TM_RS_K * 2 },
      { "marker and randomiser only",
        { .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0 },
        64 },
      { "inner code over a bare frame, no marker",
        { .rs_depth = 0, .randomise = 0, .attach_asm = 0, .convolutional = 1 },
        40 },
    };

    static uint8_t frame[CCSDS_TM_RS_K * 8];
    static uint8_t fbits[CCSDS_TM_RS_K * 8 * 8];
    static uint8_t want[(32 + CCSDS_TM_RS_N * 8 * 8) * 2];
    static uint8_t got[(32 + CCSDS_TM_RS_N * 8 * 8) * 2];

    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; c++)
      {
        const size_t octets = CASES[c].octets;
        for (size_t i = 0; i < octets; i++)
          frame[i] = (uint8_t)(i * 37u + 11u + c);
        for (size_t i = 0; i < octets; i++)
          for (unsigned b = 0; b < 8u; b++)
            fbits[i * 8u + b] = (uint8_t)((frame[i] >> (7u - b)) & 1u);

        const size_t n = ccsds_tm_frame_encode (&CASES[c].cfg, NULL, frame,
                                                octets, want, sizeof want);
        DP_REQUIRE_MSG (n != 0, CASES[c].what);

        wfm_frame_desc_t d;
        DP_REQUIRE_MSG (
            ccsds_tm_frame_describe (&CASES[c].cfg, octets, fbits, &d) == 0,
            CASES[c].what);
        wfm_frame_ops_t ops;
        ccsds_tm_frame_ops (&ops, NULL);

        memset (got, 0xAAu, sizeof got);
        const size_t m = wfm_frame_assemble (&d, &ops, got, sizeof got);
        DP_CHECK_MSG (m == n, "the description must write as many bits");
        DP_CHECK_MSG (m == n && memcmp (got, want, n) == 0,
                      "...and the SAME bits, byte for byte");
      }

    /* The inner code is continuous across frames (3.3.2), and the register
       is the caller's in both paths. Two frames through one conv_enc_t must
       agree with two frames through the shipped encoder's -- the seam is
       where a generalization that quietly owned its own state would differ,
       and it is exactly the 6 symbols of a CADU's ASM that a receiver is
       correlating against. */
    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 1, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    const size_t octets = CCSDS_TM_RS_K;
    for (size_t i = 0; i < octets; i++)
      frame[i] = (uint8_t)(i * 7u + 1u);
    for (size_t i = 0; i < octets; i++)
      for (unsigned b = 0; b < 8u; b++)
        fbits[i * 8u + b] = (uint8_t)((frame[i] >> (7u - b)) & 1u);

    const size_t nsym = ccsds_tm_frame_layout (&cfg, octets, NULL);
    conv_enc_t   ca, cb;
    conv_enc_init (&ca);
    conv_enc_init (&cb);

    wfm_frame_desc_t d;
    DP_REQUIRE (ccsds_tm_frame_describe (&cfg, octets, fbits, &d) == 0);
    wfm_frame_ops_t ops;
    ccsds_tm_frame_ops (&ops, &cb);

    int seam_ok = 1;
    for (int f = 0; f < 2; f++)
      {
        ccsds_tm_frame_encode (&cfg, &ca, frame, octets, want, nsym);
        if (wfm_frame_assemble (&d, &ops, got, nsym) != nsym
            || memcmp (got, want, nsym) != 0)
          seam_ok = 0;
      }
    DP_CHECK_MSG (seam_ok, "3.3.2: a carried register must give the same "
                           "stream through the description as through the "
                           "assembler, frame 2 included");
  }

  /* ── 16. the scoring path: undo the same spans, and report ─────────────────
   *
   * `wfm_frame_check` reads the description `wfm_frame_assemble` wrote by, so
   * the two cannot disagree about which stage covered what. What is asserted
   * here is what a receiver does with the answer: the outer code's counts,
   * which are a strictly better detector than a CRC because they say how much
   * repair it took rather than one bit of right-or-wrong.
   *
   * Damage is placed at the SYMBOL level and its effect is predicted, not
   * observed: at depth 5 a contiguous burst of 5*E symbols is exactly E in
   * each of the five codewords, the boundary each can repair, and one symbol
   * more is past it in exactly one column.
   */
  {
    enum
    {
      DEPTH = 5
    };
    const ccsds_tm_frame_cfg_t cfg
        = { /* No inner code: this begins where a frame checker begins, after
               the Viterbi and after frame sync. */
            .rs_depth      = DEPTH,
            .randomise     = 1,
            .attach_asm    = 1,
            .convolutional = 0
          };
    const size_t   octets = (size_t)CCSDS_TM_RS_K * DEPTH;
    static uint8_t frame[CCSDS_TM_RS_K * DEPTH];
    static uint8_t fbits[CCSDS_TM_RS_K * DEPTH * 8];
    static uint8_t cadu[32 + CCSDS_TM_RS_N * DEPTH * 8];
    for (size_t i = 0; i < octets; i++)
      frame[i] = (uint8_t)(i * 37u + 11u);
    for (size_t i = 0; i < octets; i++)
      for (unsigned b = 0; b < 8u; b++)
        fbits[i * 8u + b] = (uint8_t)((frame[i] >> (7u - b)) & 1u);

    wfm_frame_desc_t d;
    DP_REQUIRE (ccsds_tm_frame_describe (&cfg, octets, fbits, &d) == 0);
    wfm_frame_ops_t ops;
    ccsds_tm_frame_ops (&ops, NULL);

    wfm_frame_desc_layout_t lay;
    DP_REQUIRE (wfm_frame_desc_layout (&d, &lay) == 0);
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, cadu, sizeof cadu)
                == lay.frame_bits);

    /* Clean: every codeword good, nothing repaired. Asserted because a
       checker that reported damage on a clean frame would be as wrong as one
       that missed damage, and only one of those shows up below. */
    wfm_frame_rx_t rx;
    DP_CHECK_MSG (wfm_frame_check (&d, &ops, cadu, &rx) == 1,
                  "a clean CADU must pass its own check");
    DP_CHECK_MSG (rx.stage[0].units == DEPTH && rx.stage[0].ok == DEPTH
                      && rx.stage[0].corrected == 0
                      && rx.stage[0].symbols == 0,
                  "...with nothing repaired");
    /* The randomiser is involutive, so undoing it left the frame derandomised
       -- re-assemble before damaging it. */
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, cadu, sizeof cadu)
                == lay.frame_bits);

    /* A burst of DEPTH*E symbols: exactly E in each codeword, all repaired,
       and the COUNT is the margin that was spent. */
    const size_t blk = lay.stage[0].first;
    for (unsigned s = 0; s < DEPTH * CCSDS_TM_RS_E; s++)
      cadu[blk + (size_t)s * 8u] ^= 1u;
    DP_CHECK_MSG (wfm_frame_check (&d, &ops, cadu, &rx) == 1,
                  "a burst of DEPTH*E symbols must still pass");
    DP_CHECK_MSG (rx.stage[0].ok == DEPTH && rx.stage[0].corrected == DEPTH
                      && rx.stage[0].symbols == DEPTH * CCSDS_TM_RS_E,
                  "...having repaired exactly E in each of the five");
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, cadu, sizeof cadu)
                == lay.frame_bits);

    /* E+1 in ONE column is past the radius: that codeword is refused, the
       frame fails, and the caller is told which -- the whole reason this
       reports counts rather than a verdict. */
    for (unsigned c = 0; c <= CCSDS_TM_RS_E; c++)
      cadu[blk + ((size_t)c * DEPTH + 2u) * 8u] ^= 1u;
    DP_CHECK_MSG (wfm_frame_check (&d, &ops, cadu, &rx) == 0,
                  "E+1 errors in one column must FAIL the frame");
    DP_CHECK_MSG (rx.stage[0].units == DEPTH && rx.stage[0].ok == DEPTH - 1u,
                  "...as exactly one bad codeword out of five");

    /* The inner code is reported as NOT CHECKED rather than as passed: a
       frame checker never sees channel symbols, and "we did not look" is a
       different answer from "it was fine". */
    const ccsds_tm_frame_cfg_t coded = {
      .rs_depth = DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    wfm_frame_desc_t dc;
    DP_REQUIRE (ccsds_tm_frame_describe (&coded, octets, fbits, &dc) == 0);

    /* Re-assembled through the UNCODED description, because the coded one
       emits twice as many symbols as this buffer holds -- and because that is
       the input a frame checker really gets: the inner code is already
       undone by the time anything looks for a frame. The check below is about
       which stages get reversed, not about the bits. */
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, cadu, sizeof cadu)
                == lay.frame_bits);
    wfm_frame_rx_t rc;
    DP_CHECK_MSG (wfm_frame_check (&dc, &ops, cadu, &rc) == 1,
                  "the coded description checks the frame it is handed");
    DP_CHECK_MSG (!rc.stage[2].checked,
                  "the inner code is not reversed by a frame checker");
    DP_CHECK_MSG (rc.checked == 2u,
                  "...so two of the three stages are, not three");
  }

  /* ── 17. the randomiser stage carries WHICH generator ──────────────────────
   *
   * 131.0-B-6 specifies two (10.4.1's 131071-bit sequence and 10.4.2's
   * legacy 255-bit one) and only the matching receiver derandomises a given
   * waveform, so the choice cannot be the kernel's to make. It rides on the
   * stage, and this is what stops a kernel quietly picking for itself.
   *
   * Found by sabotage: making `rand_choice` ignore the stage passed every
   * other test in the tree and every case in the wfmgen flag matrix, because
   * the matrix pins the record and the byte COUNT rather than the bytes.
   */
  {
    enum
    {
      NB = 64
    };
    static uint8_t fbits[NB * 8];
    for (size_t i = 0; i < sizeof fbits; i++)
      fbits[i] = 0; /* zeros: the sequence itself becomes the output */

    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 0, .convolutional = 0
    };
    wfm_frame_desc_t d;
    DP_REQUIRE (ccsds_tm_frame_describe (&cfg, NB, fbits, &d) == 0);
    wfm_frame_ops_t ops;
    ccsds_tm_frame_ops (&ops, NULL);

    /* The default: depth unset selects 10.4.1's. */
    static uint8_t dflt[NB * 8];
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, dflt, sizeof dflt)
                == sizeof dflt);
    uint8_t want[40];
    ccsds_tm_rand_seq_with (&CCSDS_TM_RAND, want, sizeof want);
    DP_CHECK_MSG (memcmp (dflt, want, sizeof want) == 0,
                  "an unset choice must apply 10.4.1's sequence");

    /* depth = 2 selects the legacy generator, and the two must DIFFER --
       otherwise the choice is decoration and a caller asking for legacy
       gets a waveform no legacy receiver can read. */
    d.stage[1].depth = 2u;
    static uint8_t legacy[NB * 8];
    DP_REQUIRE (wfm_frame_assemble (&d, &ops, legacy, sizeof legacy)
                == sizeof legacy);
    ccsds_tm_rand_seq_with (&CCSDS_TM_RAND_LEGACY, want, sizeof want);
    DP_CHECK_MSG (memcmp (legacy, want, sizeof want) == 0,
                  "depth = 2 must apply 10.4.2's legacy sequence");
    DP_CHECK_MSG (memcmp (dflt, legacy, sizeof dflt) != 0,
                  "...and the two must not be the same waveform");

    /* And the receive side reverses whichever was APPLIED, reading the same
       stage -- so a frame coded one way and checked the other cannot happen.
       wfm_frame_check derandomises in place, and the payload was zeros, so
       what comes back is zeros. */
    wfm_frame_check (&d, &ops, legacy, NULL);
    int back = 1;
    for (size_t i = 0; i < sizeof legacy; i++)
      {
        if (legacy[i] != 0u)
          back = 0;
      }
    DP_CHECK_MSG (back, "the legacy-coded frame derandomises back to zeros, "
                        "because the check reads the same stage the assemble "
                        "wrote");
  }

  DP_TEST_END ("ccsds_tm_frame");
}
