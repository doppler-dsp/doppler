/*
 * test_frame_core.c — the frame descriptor as an object.
 *
 * This component computes NO layout: every offset, the CRC's position and its
 * bit order come from wfm_frame.c, which the DSSS assembler and the generator
 * already read. So what is worth testing here is not the arithmetic — that is
 * pinned in test_wfm_frame.c — but the four things this object adds, each of
 * which fails silently if it is wrong:
 *
 *   - it OWNS its literal arrays, so a descriptor outlives the buffers it was
 *     built from (a Python array is released the moment the constructor
 *     returns; borrowing would read freed memory on the first bits() call);
 *   - it agrees with wfm_frame_bits() BIT FOR BIT, because the whole reason
 *     for it is that a receiver and a generator hold the same descriptor;
 *   - it REFUSES what cannot be materialised, at construction, rather than
 *     handing back an object that produces a frame with a hole in it;
 *   - a repeat is bit-identical, which is not free: a generated field
 *     re-generated per frame would advance its register and every repeat
 *     would differ, so a capture compared against it would score as errors.
 */
#define _GNU_SOURCE
#include "dp_test.h"
#include "frame/frame_core.h"
#include "wfm/wfm_frame.h"

#include "ccsds_tm/ccsds_tm_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Barker-13 — the sync word the named RX_FRAME_BURST uses. */
static const uint8_t SYNC[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
static const uint8_t PAY[16]
    = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1 };
static const uint8_t PRE[4] = { 1, 0, 1, 0 };

/* frame_create() with every generator argument zeroed — the literal case,
   which is what a caller with real data has. Keeps the 37-argument call out
   of each test's way without hiding which fields are set. */
static frame_state_t *
lit_frame (const uint8_t *pre, size_t n_pre, size_t reps, const uint8_t *sync,
           size_t n_sync, const uint8_t *pay, size_t n_pay, int crc)
{
  return frame_create (0, pre, n_pre, 0, reps, 0, 0, 0, 0, 0, 0, 0, 0, 0, sync,
                       n_sync, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, pay, n_pay, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, crc);
}

/* An empty description: the same thirty-eight arguments `Frame` takes, with
   every field left empty. `FrameDesc`'s flavor is that it stops before
   materialising, so "nothing yet" is a legal starting point here and a
   refusal in the other constructor. */
static frame_state_t *
empty_desc (void)
{
  return frame_create_desc (0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL,
                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0,
                            0, 0, 0, 0, 0, 0, 0);
}

int
main (void)
{
  /* ── the descriptor IS wfm_frame's, bit for bit ────────────────────────
   *
   * Built independently here from the same fields: if this object ever grew
   * its own layout arithmetic, the two would part company and the receiver
   * would score a capture against a frame the generator never sent. */
  {
    frame_state_t *f = lit_frame (PRE, 4, 3, SYNC, 13, PAY, 16, 1);
    DP_REQUIRE_MSG (f, "a literal frame builds");

    wfm_frame_t w   = { 0 };
    w.preamble.kind = WFM_SEQ_LITERAL;
    w.preamble.bits = PRE;
    w.preamble.len  = 4;
    w.preamble_reps = 3;
    w.sync.kind     = WFM_SEQ_LITERAL;
    w.sync.bits     = SYNC;
    w.sync.len      = 13;
    w.payload.kind  = WFM_SEQ_LITERAL;
    w.payload.bits  = PAY;
    w.payload.len   = 16;
    w.crc           = 1;

    size_t nb = wfm_frame_nbits (&w);
    DP_REQUIRE_MSG (nb == 4 * 3 + 13 + 16 + 16, "12 + 13 + 16 + 16");
    DP_REQUIRE_MSG (f->nbits == nb, "the object reports the same length");

    uint8_t *want = malloc (nb);
    uint8_t *got  = malloc (nb);
    DP_REQUIRE_MSG (want && got, "alloc");
    DP_REQUIRE_MSG (wfm_frame_bits (&w, want, nb) == nb, "reference bits");
    DP_REQUIRE_MSG (frame_bits (f, 1, got, nb) == nb, "object bits");
    DP_REQUIRE_MSG (memcmp (want, got, nb) == 0,
                    "the object's bits ARE wfm_frame_bits of the same "
                    "descriptor");

    /* The layout is handed back, not recomputed. */
    wfm_frame_layout_t l = frame_layout (f);
    wfm_frame_layout_t r;
    wfm_frame_layout (&w, &r);
    DP_REQUIRE_MSG (memcmp (&l, &r, sizeof l) == 0,
                    "and so is the layout, field for field");

    /* Its own bits pass its own CRC — the truth-free check, on truth. */
    DP_REQUIRE_MSG (frame_crc_ok (f, got, nb) == 1, "a clean frame checks");
    got[l.payload_off + 3] ^= 1u;
    DP_REQUIRE_MSG (frame_crc_ok (f, got, nb) == 0,
                    "one flipped payload bit fails the CRC");
    got[l.payload_off + 3] ^= 1u;
    DP_REQUIRE_MSG (frame_crc_ok (f, got, nb - 1) == -1,
                    "and short input is refused, not scored on a partial "
                    "frame");

    free (want);
    free (got);
    frame_destroy (f);
  }

  /* ── the DESCRIPTOR stays valid after construction ─────────────────────
   *
   * The constructor's arrays are borrowed from the caller — in the Python
   * face, from a numpy buffer released the moment the constructor returns —
   * so the state keeps its own copies and `state->f` remains a usable
   * `wfm_frame_t` for as long as the object lives.
   *
   * This has to be asserted THROUGH the descriptor, not through bits(). The
   * frame is materialised at create, so bits() would hand back the cached
   * copy and pass just as happily if the arrays were borrowed and freed —
   * measured, by making seq_fill() borrow: every check below still passed.
   * Re-materialising from `f` is what actually reads the copies. */
  {
    uint8_t *scratch = malloc (13);
    DP_REQUIRE_MSG (scratch, "alloc");
    memcpy (scratch, SYNC, 13);
    frame_state_t *f = lit_frame (NULL, 0, 0, scratch, 13, PAY, 16, 0);
    DP_REQUIRE_MSG (f, "builds from a scratch buffer");
    DP_REQUIRE_MSG (f->f.sync.bits != scratch,
                    "the sync word was copied, not borrowed");
    memset (scratch, 0xAA, 13); /* poison, then free */
    free (scratch);

    uint8_t *got = malloc (f->nbits);
    DP_REQUIRE_MSG (got, "alloc");
    DP_REQUIRE_MSG (wfm_frame_bits (&f->f, got, f->nbits) == f->nbits,
                    "the descriptor still materialises on its own");
    DP_REQUIRE_MSG (memcmp (got, SYNC, 13) == 0,
                    "and its sync word outlived the buffer it came from");
    free (got);
    frame_destroy (f);
  }

  /* ── an unbuildable descriptor is REFUSED at construction ──────────────
   *
   * Each of these produces a frame with a hole in it if it is let through,
   * and a hole in a frame is a truth a receiver would score against. */
  {
    DP_REQUIRE_MSG (!lit_frame (NULL, 0, 0, NULL, 0, NULL, 0, 0),
                    "an empty geometry is not a frame");
    DP_REQUIRE_MSG (!lit_frame (NULL, 4, 3, SYNC, 13, PAY, 16, 1),
                    "a literal preamble with a length but no array");
    /* A PN field with no register width: pn_create() cannot be given one. */
    DP_REQUIRE_MSG (!frame_create (0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, NULL,
                                   0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 1),
                    "a PN payload with no register width");
    /* A CRC over nothing protects nothing, so it is not a frame either. */
    DP_REQUIRE_MSG (!lit_frame (NULL, 0, 0, NULL, 0, NULL, 0, 1),
                    "a crc with no payload is still an empty geometry");
  }

  /* ── a generated field, and repeats that are bit-identical ─────────────
   *
   * A repeat must be the SAME frame: a receiver compares a capture frame by
   * frame, so a second frame that differed would score as errors it did not
   * make. Two things could break it — this component tiling a re-generated
   * frame, or wfm_frame.c's generators carrying their register across calls —
   * and the second is already closed there (seq_bits creates and destroys its
   * LFSR per call), so what this pins is the first. */
  {
    frame_state_t *f = frame_create (0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                                     NULL, 0, 64, 0, 0, 7, 0, 0, 0, 0, 0, 1);
    DP_REQUIRE_MSG (f, "a PN payload with a 7-bit register builds");
    DP_REQUIRE_MSG (f->nbits == 64 + 16, "64 payload bits plus the crc");

    size_t   n2  = 2 * f->nbits;
    uint8_t *two = malloc (n2);
    DP_REQUIRE_MSG (two && frame_bits (f, 2, two, n2) == n2, "two frames");
    DP_REQUIRE_MSG (memcmp (two, two + f->nbits, f->nbits) == 0,
                    "the second frame is the first, bit for bit — the "
                    "generator does not advance between them");

    /* Whole frames only: a partial one would misalign every frame after it. */
    DP_REQUIRE_MSG (frame_bits (f, 2, two, n2 - 1) == f->nbits,
                    "a buffer one bit short of two frames carries one");
    DP_REQUIRE_MSG (frame_bits (f, 2, two, f->nbits - 1) == 0,
                    "and one too small for any whole frame carries none");
    DP_REQUIRE_MSG (frame_bits_max_out (f, 3) == 3 * f->nbits,
                    "max_out counts frames");

    free (two);
    frame_destroy (f);
  }

  /* ── a dotted preamble: the kind that needs no array at all ────────────*/
  {
    frame_state_t *f = frame_create (3, NULL, 0, 8, 2, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, SYNC, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     PAY, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    DP_REQUIRE_MSG (f, "a dotted preamble builds with no array");
    DP_REQUIRE_MSG (f->nbits == 8 * 2 + 13 + 16, "16 + 13 + 16, no crc");
    uint8_t *got = malloc (f->nbits);
    DP_REQUIRE_MSG (got && frame_bits (f, 1, got, f->nbits) == f->nbits, "ok");
    DP_REQUIRE_MSG (got[0] == 1 && got[1] == 0 && got[2] == 1,
                    "1010… — it starts high, so a one-bit field is not "
                    "silently zeros");
    DP_REQUIRE_MSG (frame_crc_ok (f, got, f->nbits) == -1,
                    "a frame with no CRC says so, rather than passing");
    free (got);
    frame_destroy (f);
  }

  frame_destroy (NULL); /* NULL is a no-op, as the header says */

  /* ── the builder: the same object, described field by field ──────────
   *
   * frame_create() takes the four fields wfm_frame_t names. This takes one
   * field at a time, and the two must produce the SAME frame where both can
   * express it -- otherwise there are two descriptors again, which is what
   * the generalization exists to end.
   */
  {
    frame_state_t *b = empty_desc ();
    DP_REQUIRE_MSG (b != NULL, "an empty description allocates");
    DP_CHECK_MSG (frame_n_fields (b) == 0 && frame_n_stages (b) == 0,
                  "...and starts with nothing in it");

    DP_CHECK (
        frame_add_field (b, SYNC, 13, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        == 0);
    DP_CHECK (
        frame_add_field (b, PAY, 16, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        == 1);
    /* The trailer: derived by stage 0, hence `derived_by = 0 + 1`. */
    DP_CHECK (frame_add_field (b, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                               WFM_FRAME_CRC_BITS)
              == 2);
    DP_CHECK (frame_add_stage (b, WFM_STAGE_CRC16, 1, 2, 0, 0, 0) == 0);
    DP_REQUIRE_MSG (frame_build (b) == 0, "the description builds");

    DP_CHECK_MSG (b->nbits == 13 + 16 + 16, "13 + 16 + 16");
    DP_CHECK_MSG (frame_field_off (b, 1) == 13
                      && frame_field_bits (b, 1) == 16,
                  "the payload lands behind the sync word");
    DP_CHECK_MSG (frame_stage_first (b, 0) == 13
                      && frame_stage_bits (b, 0) == 32,
                  "the CRC stage covers the payload AND its own trailer");

    /* The configured path, same frame, and the bits must agree. */
    frame_state_t *c = frame_create (0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, SYNC, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     PAY, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
    DP_REQUIRE (c != NULL && c->nbits == b->nbits);
    uint8_t *bb = malloc (b->nbits);
    uint8_t *cb = malloc (c->nbits);
    DP_REQUIRE (bb && cb);
    DP_REQUIRE (frame_bits (b, 1, bb, b->nbits) == b->nbits);
    DP_REQUIRE (frame_bits (c, 1, cb, c->nbits) == c->nbits);
    DP_CHECK_MSG (memcmp (bb, cb, b->nbits) == 0,
                  "described and configured must be the SAME frame");
    DP_CHECK_MSG (frame_crc_ok (b, bb, b->nbits) == 1,
                  "a described frame is its own truth, like a configured one");

    /* The named view belongs to the configured path alone: a described frame
       has no field called "payload", and saying so beats inventing offsets
       for fields that do not exist. */
    DP_CHECK_MSG (frame_layout (b).total_bits == 0,
                  "layout()'s NAMED view is empty for a described frame");
    DP_CHECK_MSG (frame_layout (c).total_bits == c->nbits,
                  "...and populated for a configured one");

    /* A description is closed once built. */
    DP_CHECK (
        frame_add_field (b, PAY, 16, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        == -1);
    DP_CHECK (frame_add_stage (b, WFM_STAGE_CRC16, 0, 1, 0, 0, 0) == -1);
    DP_CHECK_MSG (frame_build (b) == -1, "and cannot be built twice");

    free (bb);
    free (cb);
    frame_destroy (b);
    frame_destroy (c);

    /* An empty description is not a frame. */
    frame_state_t *e = empty_desc ();
    DP_REQUIRE (e != NULL);
    DP_CHECK_MSG (frame_build (e) == -1, "an empty description cannot build");
    frame_destroy (e);
  }

  /* ── a CCSDS CADU, described from Python's side of the ABI ───────────
   *
   * The point of the whole exercise. `ccsds_tm` has no Python binding and is
   * not getting one, so this object is the only place a caller can reach the
   * outer code, the randomiser and the inner code -- and it reaches them by
   * DESCRIBING a frame, not by a CCSDS entry point being added here.
   *
   * Checked against ccsds_tm_frame_encode byte for byte rather than against
   * itself, which is this slice's rule: the shipped encoder is already
   * falsified against the values 131.0-B-3 prints, so equalling it inherits
   * all of that, and agreeing only with itself would prove nothing.
   */
  {
    enum
    {
      DEPTH = 2
    };
    const size_t   octets = (size_t)CCSDS_TM_RS_K * DEPTH;
    static uint8_t frame[CCSDS_TM_RS_K * DEPTH];
    static uint8_t fbits[CCSDS_TM_RS_K * DEPTH * 8];
    static uint8_t want[(32 + CCSDS_TM_RS_N * DEPTH * 8) * 2];
    for (size_t i = 0; i < octets; i++)
      frame[i] = (uint8_t)(i * 29u + 5u);
    for (size_t i = 0; i < octets; i++)
      for (unsigned k = 0; k < 8u; k++)
        fbits[i * 8u + k] = (uint8_t)((frame[i] >> (7u - k)) & 1u);

    const ccsds_tm_frame_cfg_t cfg = {
      .rs_depth = DEPTH, .randomise = 1, .attach_asm = 1, .convolutional = 1
    };
    const size_t n
        = ccsds_tm_frame_encode (&cfg, NULL, frame, octets, want, sizeof want);
    DP_REQUIRE (n != 0);

    uint8_t asm_bits[CCSDS_TM_ASM_BITS];
    ccsds_tm_asm_bits (asm_bits);

    frame_state_t *b = empty_desc ();
    DP_REQUIRE (b != NULL);
    /* [ ASM | Transfer Frame | R-S check symbols ] */
    DP_CHECK (frame_add_field (b, asm_bits, CCSDS_TM_ASM_BITS, 0, 0, 1, 0, 0,
                               0, 0, 0, 0, 0, 0, 0, 0)
              == 0);
    DP_CHECK (frame_add_field (b, fbits, octets * 8u, 0, 0, 1, 0, 0, 0, 0, 0,
                               0, 0, 0, 0, 0)
              == 1);
    DP_CHECK (frame_add_field (b, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                               (size_t)CCSDS_TM_RS_2E * DEPTH * 8u)
              == 2);
    /* The three covers ARE the coverage table: the outer code and the
       randomiser start behind the marker, the inner code does not. */
    DP_CHECK (frame_add_stage (b, WFM_STAGE_RS, 1, 2, DEPTH, 0, 0) == 0);
    DP_CHECK (frame_add_stage (b, WFM_STAGE_RANDOMISE, 1, 2, 0, 0, 0) == 1);
    DP_CHECK (frame_add_stage (b, WFM_STAGE_CONV, 0, 3, 0, 2, 1) == 2);
    DP_REQUIRE_MSG (frame_build (b) == 0, "a CADU builds from a description");

    DP_CHECK_MSG (b->nbits == n, "the CADU is the length the encoder says");
    uint8_t *got = malloc (b->nbits);
    DP_REQUIRE (got && frame_bits (b, 1, got, b->nbits) == b->nbits);
    DP_CHECK_MSG (memcmp (got, want, n) == 0,
                  "...and the SAME bits as ccsds_tm_frame_encode, byte for "
                  "byte");
    free (got);
    frame_destroy (b);
  }

  DP_TEST_END ("frame_core");
}
