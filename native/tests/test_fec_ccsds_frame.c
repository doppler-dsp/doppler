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
    const size_t n
        = fec_frame_encode (&cfg, NULL, frame, sizeof frame, out, sizeof out);

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
    const size_t n
        = fec_frame_encode (&cfg, NULL, frame, sizeof frame, out, sizeof out);
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
    const size_t n
        = fec_frame_encode (&cfg, NULL, frame, sizeof frame, out, sizeof out);
    DP_REQUIRE (n == sizeof out);

    /* Rebuild the CADU from published pieces: the marker as figure 9-1
       prints it, then the sequence, which is what randomising zeros gives. */
    uint8_t cadu[32 + 32 * 8];
    memcpy (cadu, asm_published, sizeof asm_published);
    fec_ccsds_rand_seq (cadu + 32, 32 * 8);

    uint8_t    want[(32 + 32 * 8) * 2];
    conv_enc_t s;
    conv_enc_init (&s);
    conv_encode (&s, &FEC_CCSDS_CONV, cadu, sizeof cadu, want, sizeof want);

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

    const size_t n
        = fec_frame_encode (&cfg, NULL, frame, sizeof frame, out, sizeof out);
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
    const size_t n = fec_frame_encode (&cfg, NULL, frame, 2, out, sizeof out);

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
    DP_CHECK_MSG (fec_frame_encode (&d3, NULL, frame, (size_t)FEC_RS_K * 2,
                                    out, sizeof out)
                      == 0,
                  "encode must refuse whatever layout refuses");

    const fec_frame_cfg_t none = { 0 };
    DP_CHECK (fec_frame_layout (&none, 0, NULL) == 0);
    DP_CHECK (fec_frame_encode (&none, NULL, frame, 0, out, sizeof out) == 0);

    int untouched = 1;
    for (size_t i = 0; i < sizeof out; i++)
      if (out[i] != 0xAAu)
        untouched = 0;
    DP_CHECK_MSG (untouched, "a refused encode must not write to out");

    /* The CADU is assembled in the TAIL of `out`, so a short buffer is a
       write past the end rather than a truncated answer. Refusing on capacity
       is what makes that unreachable, and it is checked with a buffer one
       symbol short of sufficient rather than a wildly small one. */
    const fec_frame_cfg_t ok1 = { .rs_depth = 1, .attach_asm = 1 };
    uint8_t               room[32 + 255 * 8];
    memset (room, 0xAAu, sizeof room);
    const size_t need = fec_frame_layout (&ok1, FEC_RS_K, NULL);
    DP_REQUIRE (need == sizeof room);
    DP_CHECK_MSG (
        fec_frame_encode (&ok1, NULL, frame, FEC_RS_K, room, need - 1u) == 0,
        "a buffer one symbol short must be refused, not written");
    for (size_t i = 0; i < sizeof room; i++)
      if (room[i] != 0xAAu)
        untouched = 0;
    DP_CHECK_MSG (untouched, "and it must not have written anything");
    DP_CHECK (fec_frame_encode (&ok1, NULL, frame, FEC_RS_K, room, need)
              == need);
  }

  /* ── a STREAM of frames: the inner code does not restart ────────────────
   *
   * 3.3.2 fixes the output as one uninterrupted symbol sequence. The kernel
   * test already proves chunked == whole when the register is carried
   * (test_fec_ccsds_conv, "split"); this is the same claim one layer up,
   * where the assembler is the caller that has to carry it.
   *
   * It is asserted as an EQUALITY against the continuous encoding of the two
   * CADUs, and the NULL form is measured beside it rather than merely being
   * different — 6 symbols, all inside the first 7 of frame 2, which is the
   * K-1 = 6 bits of register memory and lands on the ASM. Without the
   * equality this is invisible: every other test in this file encodes one
   * frame, and one frame is exactly the case that cannot see it. */
  {
    const unsigned        depth = 1;
    const size_t          flen  = (size_t)FEC_RS_K * depth;
    const fec_frame_cfg_t coded = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    const fec_frame_cfg_t bare = {
      .rs_depth = depth, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };

    uint8_t a[FEC_RS_K], b[FEC_RS_K];
    for (size_t i = 0; i < flen; i++)
      {
        a[i] = (uint8_t)(i * 7u + 1u);
        b[i] = (uint8_t)(i * 31u + 5u);
      }

    const size_t nsym = fec_frame_layout (&coded, flen, NULL);
    const size_t ncad = fec_frame_layout (&bare, flen, NULL);
    DP_REQUIRE (nsym == 2u * ncad);

    static uint8_t carried[2 * (32 + 255 * 8) * 2];
    static uint8_t restart[2 * (32 + 255 * 8) * 2];
    static uint8_t cadu[2 * (32 + 255 * 8)];
    static uint8_t cont[2 * (32 + 255 * 8) * 2];

    /* (1) a transmitter's loop, carrying the register */
    conv_enc_t s;
    conv_enc_init (&s);
    fec_frame_encode (&coded, &s, a, flen, carried, nsym);
    fec_frame_encode (&coded, &s, b, flen, carried + nsym, nsym);

    /* (2) the same two CADUs as ONE continuous encode — the external truth,
           built from the uncoded assembler and the kernel, neither of which
           knows anything about frames */
    fec_frame_encode (&bare, NULL, a, flen, cadu, ncad);
    fec_frame_encode (&bare, NULL, b, flen, cadu + ncad, ncad);
    conv_enc_t t;
    conv_enc_init (&t);
    conv_encode (&t, &FEC_CCSDS_CONV, cadu, 2u * ncad, cont, sizeof cont);

    DP_CHECK_MSG (memcmp (carried, cont, 2u * nsym) == 0,
                  "3.3.2: a stream of frames must equal one continuous "
                  "encode of the same CADUs");

    /* (3) and NULL is the single-frame form, which restarts — measured, so
           the cost of getting this wrong is a number in the record */
    fec_frame_encode (&coded, NULL, a, flen, restart, nsym);
    fec_frame_encode (&coded, NULL, b, flen, restart + nsym, nsym);
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
    DP_CHECK_MSG (diff > 0 && last < 2u * (FEC_CONV_K - 1u),
                  "restarting costs the K-1 bits of register memory and no "
                  "more: every difference inside the first 12 symbols");
  }

  /* ── the decoder undoes the encoder, over the SAME spans ─────────────
   *
   * A round trip is weak evidence on its own -- it is what this whole slice
   * refuses to rely on -- so it is not the claim here. The claim is that
   * `fec_frame_decode` reads the same span table `fec_frame_encode` wrote,
   * and the checks below are chosen to fail if it does not:
   *
   *   - the payload is ZEROS, so a decoder that skipped the randomiser
   *     returns the sequence instead of the frame, loudly (a PN payload
   *     would hide it, per test_fec_ccsds_rand's opening);
   *   - the marker is CLOBBERED before decoding, so a decoder that
   *     derandomised or R-S-checked from bit 0 instead of from behind it
   *     cannot pass -- and one that reads the spans correctly cannot even
   *     notice.
   */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    fec_frame_layout_t lay;
    const size_t       n = fec_frame_layout (&cfg, FEC_RS_K * 5, &lay);
    DP_REQUIRE (n != 0 && n == lay.cadu_bits);

    static uint8_t frame[FEC_RS_K * 5] = { 0 };
    static uint8_t cadu[(32 + FEC_RS_N * 5 * 8)];
    DP_REQUIRE (
        fec_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu)
        == lay.cadu_bits);

    /* Nothing after this point may depend on the marker's contents. */
    for (size_t i = 0; i < lay.marker.n; i++)
      cadu[i] ^= 1u;

    static uint8_t back[FEC_RS_K * 5];
    fec_frame_rx_t rx = { 999u, 999u, 999u, 999u, 999u };
    memset (back, 0xAA, sizeof back);
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
        == sizeof back);
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "the decoder must recover the frame from behind a marker "
                  "it does not read");
    DP_CHECK_MSG (rx.rs_codewords == 5u && rx.rs_ok == 5u,
                  "every interleaved codeword must pass its syndrome check");
    DP_CHECK (rx.frame_len == sizeof back);
  }

  /* ── a corrupted symbol is CORRECTED, and a hopeless one is reported ──
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
    const fec_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    fec_frame_layout_t lay;
    fec_frame_layout (&cfg, FEC_RS_K * 5, &lay);

    static uint8_t frame[FEC_RS_K * 5];
    static uint8_t cadu[(32 + FEC_RS_N * 5 * 8)];
    static uint8_t back[FEC_RS_K * 5];
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 37u + 11u);
    fec_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu);

    /* Clean first: with columns that differ, this is what fails the moment
       the de-interleave rotates. */
    fec_frame_rx_t rx = { 0, 0, 0, 0, 0 };
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
        == sizeof back);
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "structured data must round-trip byte for byte");
    DP_CHECK_MSG (rx.rs_ok == 5u && rx.rs_codewords == 5u,
                  "every codeword must check out when the de-interleave "
                  "pairs each column with its own parity");

    /* Symbol index 2 of the block is codeword 2 at depth 5 (S1 hands encoder
       e every 5th symbol starting at e). Its first bit is block bit 16. */
    cadu[lay.randomised.first + 16u] ^= 1u;
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
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
    for (unsigned s = 0; s < 5u * FEC_RS_E; s++)
      cadu[lay.randomised.first + (size_t)s * 8u] ^= 1u;
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
        == sizeof back);
    DP_CHECK_MSG (rx.rs_ok == 5u && rx.rs_corrected == 5u
                      && rx.rs_symbols == 5u * FEC_RS_E,
                  "a burst of 5*E symbols must be fully repaired at depth 5");
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) == 0,
                  "...and the frame must be exactly what was sent");
    for (unsigned s = 0; s < 5u * FEC_RS_E; s++)
      cadu[lay.randomised.first + (size_t)s * 8u] ^= 1u;

    /* Past the radius in ONE column: E+1 symbols of codeword 2. The decoder
       must refuse that codeword and say so rather than hand back a frame it
       cannot vouch for -- the counts are the caller's protection. */
    for (unsigned c = 0; c <= FEC_RS_E; c++)
      cadu[lay.randomised.first + ((size_t)c * 5u + 2u) * 8u] ^= 1u;
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
        == sizeof back);
    DP_CHECK_MSG (rx.rs_codewords == 5u && rx.rs_ok == 4u,
                  "E+1 errors in one column must leave that codeword bad");
    DP_CHECK_MSG (memcmp (back, frame, sizeof frame) != 0,
                  "...and the frame it returns must be the wrong one it "
                  "just reported, not a silently repaired copy");
  }

  /* ── with no outer code the frame is the block ───────────────────────── */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 0, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    fec_frame_layout_t lay;
    uint8_t            frame[64] = { 0 };
    uint8_t            cadu[32 + 64 * 8];
    uint8_t            back[64];
    fec_frame_rx_t     rx = { 0, 9u, 9u, 9u, 9u };

    fec_frame_layout (&cfg, sizeof frame, &lay);
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i * 7u + 3u);
    DP_REQUIRE (
        fec_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu)
        == lay.cadu_bits);
    DP_REQUIRE (
        fec_frame_decode (&cfg, cadu, lay.cadu_bits, back, sizeof back, &rx)
        == sizeof frame);
    DP_CHECK (memcmp (back, frame, sizeof frame) == 0);
    DP_CHECK_MSG (rx.rs_codewords == 0u && rx.rs_ok == 0u,
                  "no outer code means no codewords to report");
  }

  /* ── the refusals, each verified by a poisoned buffer ───────────────── */
  {
    const fec_frame_cfg_t cfg = {
      .rs_depth = 5, .randomise = 1, .attach_asm = 1, .convolutional = 0
    };
    fec_frame_layout_t lay;
    fec_frame_layout (&cfg, FEC_RS_K * 5, &lay);

    static uint8_t frame[FEC_RS_K * 5] = { 0 };
    static uint8_t cadu[(32 + FEC_RS_N * 5 * 8)];
    static uint8_t back[FEC_RS_K * 5];
    fec_frame_encode (&cfg, NULL, frame, sizeof frame, cadu, sizeof cadu);

    memset (back, 0xAA, sizeof back);
    DP_CHECK_MSG (fec_frame_decode (&cfg, cadu, lay.cadu_bits - 8u, back,
                                    sizeof back, NULL)
                      == 0,
                  "a CADU that is not the layout's length must refuse");
    DP_CHECK_MSG (fec_frame_decode (&cfg, cadu, lay.cadu_bits - 1u, back,
                                    sizeof back, NULL)
                      == 0,
                  "a block that is not a whole number of octets must refuse");
    DP_CHECK_MSG (fec_frame_decode (&cfg, cadu, lay.cadu_bits, back,
                                    sizeof back - 1u, NULL)
                      == 0,
                  "a short frame buffer must refuse");
    for (size_t i = 0; i < sizeof back; i++)
      DP_CHECK (back[i] == 0xAAu);
  }

  DP_TEST_END ("fec_ccsds_frame");
}
