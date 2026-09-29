/*
 * test_frame_core.c — the frame descriptor as an object.
 *
 * This component computes NO layout: every offset, the CRC's position and its
 * bit order come from wfm_frame.c, which the DSSS assembler and the generator
 * already read. So what is worth testing here is not the arithmetic — that is
 * pinned in test_wfm_frame.c — but the four things this object adds, each of
 * which fails silently if it is wrong:
 *
 *   - it OWNS its bit arrays, so a descriptor outlives the buffers it was
 *     built from (a Python array is released the moment the constructor
 *     returns; borrowing would read freed memory on the first bits() call);
 *   - it takes BITS, and refuses an element that is not one rather than
 *     masking it (a digit string passed where bits belong reads as 101);
 *   - it agrees with dp_wfm_frame_fixed() + dp_wfm_frame_assemble() BIT FOR
 *     BIT, because the whole reason for it is that a receiver and a
 *     generator hold the same description;
 *   - it REFUSES what cannot be materialised, at construction, rather than
 *     handing back an object that produces a frame with a hole in it;
 *   - a repeat is bit-identical: bits() tiles the one materialised frame,
 *     so a capture compared against it frame by frame scores no phantom
 *     errors.
 *
 * A generated field (PN, dotted, a repeated preamble) reaches this object as
 * bits, through dp_wfm_field_bits -- the same door every text face uses.
 */
#include "doppler/frame/frame_core.h"
#include "doppler/wfm/wfm_frame.h"
#include "dp_test.h"

#include "doppler/ccsds_tm/ccsds_tm_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Barker-13 — the sync word the named RX_FRAME_BURST uses. */
static const uint8_t SYNC[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
static const uint8_t PAY[16]
    = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1 };
static const uint8_t PRE[4] = { 1, 0, 1, 0 };

/* An empty description: `FrameDesc()`, every field omitted. `FrameDesc`'s
   flavor is that it stops before materialising, so "nothing yet" is a legal
   starting point here and a refusal in the other constructor. */
static dp_frame_state_t *
empty_desc (void)
{
  return dp_frame_create_desc (NULL, 0, NULL, 0, NULL, 0, 0);
}

int
main (void)
{
  /* ── the description IS the common frame's, bit for bit ────────────────
   *
   * Built independently here from the same fields: if this object ever grew
   * its own layout arithmetic, the two would part company and the receiver
   * would score a capture against a frame the generator never sent. */
  {
    /* The preamble repeated three times IN ITS BITS -- the only way a
       repetition reaches this object now -- against a reference that states
       it as three repetitions of the field. The two spellings must be one
       frame. */
    uint8_t pre3[12];
    DP_REQUIRE (dp_wfm_field_bits ("1010*3", pre3, sizeof pre3, NULL) == 12);
    dp_frame_state_t *f = dp_frame_create (pre3, 12, SYNC, 13, PAY, 16, 1);
    DP_REQUIRE_MSG (f, "a literal frame builds");

    const wfm_seq_t pre = { .kind = WFM_SEQ_LITERAL, .bits = PRE, .len = 4 };
    const wfm_seq_t syn = { .kind = WFM_SEQ_LITERAL, .bits = SYNC, .len = 13 };
    const wfm_seq_t pay = { .kind = WFM_SEQ_LITERAL, .bits = PAY, .len = 16 };
    wfm_frame_desc_t        w;
    wfm_frame_desc_layout_t r;
    DP_REQUIRE (dp_wfm_frame_fixed (&w, &pre, 3, &syn, &pay, 1) == 0);
    DP_REQUIRE (dp_wfm_frame_desc_layout (&w, &r) == 0);

    size_t nb = r.frame_bits;
    DP_REQUIRE_MSG (nb == 4 * 3 + 13 + 16 + 16, "12 + 13 + 16 + 16");
    DP_REQUIRE_MSG (f->nbits == nb, "the object reports the same length");

    uint8_t *want = malloc (nb);
    uint8_t *got  = malloc (nb);
    DP_REQUIRE_MSG (want && got, "alloc");
    DP_REQUIRE_MSG (dp_wfm_frame_assemble (&w, NULL, want, nb) == nb,
                    "reference bits");
    DP_REQUIRE_MSG (dp_frame_bits (f, 1, got, nb) == nb, "object bits");
    DP_REQUIRE_MSG (memcmp (want, got, nb) == 0,
                    "the object's bits ARE the assembly of the same "
                    "description");

    /* The layout is the one the description lays out to, not recomputed. */
    DP_REQUIRE_MSG (memcmp (&f->dl, &r, sizeof r) == 0,
                    "and so is the layout, field for field");
    const int pi = dp_frame_field_index (f, "payload");
    DP_REQUIRE_MSG (pi >= 0 && dp_frame_field_off (f, (size_t)pi) == 12 + 13,
                    "the payload is found by name, after preamble and sync");
    const size_t pay_off = dp_frame_field_off (f, (size_t)pi);

    /* Its own bits pass its own CRC — the truth-free check, on truth. */
    DP_REQUIRE_MSG (dp_frame_crc_ok (f, got, nb) == 1, "a clean frame checks");
    got[pay_off + 3] ^= 1u;
    DP_REQUIRE_MSG (dp_frame_crc_ok (f, got, nb) == 0,
                    "one flipped payload bit fails the CRC");
    got[pay_off + 3] ^= 1u;
    DP_REQUIRE_MSG (dp_frame_crc_ok (f, got, nb - 1) == -1,
                    "and short input is refused, not scored on a partial "
                    "frame");

    free (want);
    free (got);
    dp_frame_destroy (f);
  }

  /* ── the DESCRIPTOR stays valid after construction ─────────────────────
   *
   * The constructor's arrays are borrowed from the caller — in the Python
   * face, from a numpy buffer released the moment the constructor returns —
   * so the state keeps its own copies and `state->d` remains a usable
   * description for as long as the object lives.
   *
   * This has to be asserted THROUGH the descriptor, not through bits(). The
   * frame is materialised at create, so bits() would hand back the cached
   * copy and pass just as happily if the arrays were borrowed and freed —
   * measured, by making the copy borrow: every check below still passed.
   * Re-materialising from `d` is what actually reads the copies. */
  {
    uint8_t *scratch = malloc (13);
    DP_REQUIRE_MSG (scratch, "alloc");
    memcpy (scratch, SYNC, 13);
    dp_frame_state_t *f = dp_frame_create (NULL, 0, scratch, 13, PAY, 16, 0);
    DP_REQUIRE_MSG (f, "builds from a scratch buffer");
    const int si = dp_wfm_frame_field_index (&f->d, "sync");
    DP_REQUIRE_MSG (si >= 0 && f->d.field[si].seq.bits != scratch,
                    "the sync word was copied, not borrowed");
    memset (scratch, 0xAA, 13); /* poison, then free */
    free (scratch);

    uint8_t *got = malloc (f->nbits);
    DP_REQUIRE_MSG (got, "alloc");
    DP_REQUIRE_MSG (dp_wfm_frame_assemble (&f->d, NULL, got, f->nbits)
                        == f->nbits,
                    "the description still materialises on its own");
    DP_REQUIRE_MSG (memcmp (got, SYNC, 13) == 0,
                    "and its sync word outlived the buffer it came from");
    free (got);
    dp_frame_destroy (f);
  }

  /* ── an unbuildable descriptor is REFUSED at construction ──────────────
   *
   * Each of these produces a frame with a hole in it if it is let through,
   * and a hole in a frame is a truth a receiver would score against. */
  {
    DP_REQUIRE_MSG (!dp_frame_create (NULL, 0, NULL, 0, NULL, 0, 0),
                    "an empty geometry is not a frame");
    DP_REQUIRE_MSG (!dp_frame_create (NULL, 4, SYNC, 13, PAY, 16, 1),
                    "a length with no array is refused, not read as absent");
    /* An element that is not a bit. `101` is what the string "0101" becomes
       when a binding reads it as a number (just-makeit#1700), and masking it
       to 1 would make that mistake a valid-looking field. */
    static const uint8_t digit[3] = { 1, 101, 0 };
    DP_REQUIRE_MSG (!dp_frame_create (NULL, 0, digit, 3, PAY, 16, 0),
                    "a byte that is not 0 or 1 is refused, not masked");
    static const uint8_t two[2] = { 1, 2 };
    DP_REQUIRE_MSG (!dp_frame_create (NULL, 0, NULL, 0, two, 2, 0),
                    "...in any field");
    /* A CRC over nothing protects nothing, so it is not a frame either. */
    DP_REQUIRE_MSG (!dp_frame_create (NULL, 0, NULL, 0, NULL, 0, 1),
                    "a crc with no payload is still an empty geometry");
  }

  /* ── a generated field, and repeats that are bit-identical ─────────────
   *
   * A repeat must be the SAME frame: a receiver compares a capture frame by
   * frame, so a second frame that differed would score as errors it did not
   * make. The generated payload arrives as bits from the one text door. */
  {
    uint8_t pn[64];
    DP_REQUIRE (dp_wfm_field_bits ("pn:64:7", pn, sizeof pn, NULL) == 64);
    dp_frame_state_t *f = dp_frame_create (NULL, 0, NULL, 0, pn, 64, 1);
    DP_REQUIRE_MSG (f, "a PN payload with a 7-bit register builds");
    DP_REQUIRE_MSG (f->nbits == 64 + 16, "64 payload bits plus the crc");

    size_t   n2  = 2 * f->nbits;
    uint8_t *two = malloc (n2);
    DP_REQUIRE_MSG (two && dp_frame_bits (f, 2, two, n2) == n2, "two frames");
    DP_REQUIRE_MSG (memcmp (two, two + f->nbits, f->nbits) == 0,
                    "the second frame is the first, bit for bit — the "
                    "generator does not advance between them");

    /* Whole frames only: a partial one would misalign every frame after it. */
    DP_REQUIRE_MSG (dp_frame_bits (f, 2, two, n2 - 1) == f->nbits,
                    "a buffer one bit short of two frames carries one");
    DP_REQUIRE_MSG (dp_frame_bits (f, 2, two, f->nbits - 1) == 0,
                    "and one too small for any whole frame carries none");
    DP_REQUIRE_MSG (dp_frame_bits_max_out (f, 3) == 3 * f->nbits,
                    "max_out counts frames");

    free (two);
    dp_frame_destroy (f);
  }

  /* ── a dotted preamble, repeated, from its text form ─────────────────── */
  {
    uint8_t dot[16];
    DP_REQUIRE (dp_wfm_field_bits ("dotted:8*2", dot, sizeof dot, NULL) == 16);
    dp_frame_state_t *f = dp_frame_create (dot, 16, SYNC, 13, PAY, 16, 0);
    DP_REQUIRE_MSG (f, "a dotted preamble builds");
    DP_REQUIRE_MSG (f->nbits == 8 * 2 + 13 + 16, "16 + 13 + 16, no crc");
    uint8_t *got = malloc (f->nbits);
    DP_REQUIRE_MSG (got && dp_frame_bits (f, 1, got, f->nbits) == f->nbits,
                    "ok");
    DP_REQUIRE_MSG (got[0] == 1 && got[1] == 0 && got[2] == 1,
                    "1010… — it starts high, so a one-bit field is not "
                    "silently zeros");
    DP_REQUIRE_MSG (dp_frame_crc_ok (f, got, f->nbits) == -1,
                    "a frame with no CRC says so, rather than passing");
    free (got);
    dp_frame_destroy (f);
  }

  dp_frame_destroy (NULL); /* NULL is a no-op, as the header says */

  /* ── the builder: the same object, described field by field ──────────
   *
   * dp_frame_create() takes the common frame's three fields. This takes one
   * field at a time, and the two must produce the SAME frame where both can
   * express it -- otherwise there are two descriptors again, which is what
   * the generalization exists to end.
   */
  {
    dp_frame_state_t *b = empty_desc ();
    DP_REQUIRE_MSG (b != NULL, "an empty description allocates");
    DP_CHECK_MSG (dp_frame_n_fields (b) == 0 && dp_frame_n_stages (b) == 0,
                  "...and starts with nothing in it");

    DP_CHECK (dp_frame_add_field (b, "sync", SYNC, 13) == 0);
    DP_CHECK (dp_frame_add_field (b, "payload", PAY, 16) == 1);
    /* The trailer: a field a stage fills. The INDEX form of add_stage wires
       its producer by the same rule the by-name form uses, so a caller
       counting fields and a caller naming them build the same frame. */
    DP_CHECK (dp_frame_add_derived (b, "crc", WFM_FRAME_CRC_BITS) == 2);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_CRC16, 1, 2, 0, 0, 0, 0) == 0);
    DP_CHECK_MSG (b->d.field[2].derived_by == 1u,
                  "the index form wired the trailer to stage 0 (PLUS ONE)");
    DP_REQUIRE_MSG (dp_frame_build (b) == 0, "the description builds");

    DP_CHECK_MSG (b->nbits == 13 + 16 + 16, "13 + 16 + 16");
    DP_CHECK_MSG (dp_frame_field_off (b, 1) == 13
                      && dp_frame_field_bits (b, 1) == 16,
                  "the payload lands behind the sync word");
    DP_CHECK_MSG (dp_frame_stage_first (b, 0) == 13
                      && dp_frame_stage_bits (b, 0) == 32,
                  "the CRC stage covers the payload AND its own trailer");

    /* The configured path, same frame, and the bits must agree. */
    dp_frame_state_t *c = dp_frame_create (NULL, 0, SYNC, 13, PAY, 16, 1);
    DP_REQUIRE (c != NULL && c->nbits == b->nbits);
    uint8_t *bb = malloc (b->nbits);
    uint8_t *cb = malloc (c->nbits);
    DP_REQUIRE (bb && cb);
    DP_REQUIRE (dp_frame_bits (b, 1, bb, b->nbits) == b->nbits);
    DP_REQUIRE (dp_frame_bits (c, 1, cb, c->nbits) == c->nbits);
    DP_CHECK_MSG (memcmp (bb, cb, b->nbits) == 0,
                  "described and configured must be the SAME frame");
    DP_CHECK_MSG (dp_frame_crc_ok (b, bb, b->nbits) == 1,
                  "a described frame is its own truth, like a configured one");

    /* And the SAME description, not merely the same bits: the configured
       path names its fields exactly as the builder above was told to, at the
       same indices, so a receiver reading either by name reads the same
       offsets. There is no second, named view of a frame to disagree. */
    DP_CHECK_MSG (dp_frame_n_fields (c) == dp_frame_n_fields (b)
                      && dp_frame_n_stages (c) == dp_frame_n_stages (b),
                  "the same number of fields and stages");
    static const char *const names[3] = { "sync", "payload", "crc" };
    for (int k = 0; k < 3; k++)
      {
        const int i = dp_frame_field_index (c, names[k]);
        DP_CHECK_MSG (i == k && dp_frame_field_index (b, names[k]) == k
                          && dp_frame_field_off (c, (size_t)i)
                                 == dp_frame_field_off (b, (size_t)i)
                          && dp_frame_field_bits (c, (size_t)i)
                                 == dp_frame_field_bits (b, (size_t)i),
                      "each field is found by the same name at the same "
                      "index and offset");
      }

    /* A description is closed once built. */
    DP_CHECK (dp_frame_add_field (b, "late", PAY, 16) == -1);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_CRC16, 0, 1, 0, 0, 0, 0) == -1);
    DP_CHECK_MSG (dp_frame_build (b) == -1, "and cannot be built twice");

    free (bb);
    free (cb);
    dp_frame_destroy (b);
    dp_frame_destroy (c);

    /* An empty description is not a frame. */
    dp_frame_state_t *e = empty_desc ();
    DP_REQUIRE (e != NULL);
    DP_CHECK_MSG (dp_frame_build (e) == -1,
                  "an empty description cannot build");

    /* add_field takes bits and refuses anything else, and never leaves a
       half-appended field behind. */
    static const uint8_t digit[2] = { 1, 101 };
    DP_CHECK_MSG (dp_frame_add_field (e, "x", NULL, 0) == -1,
                  "no bits is no field");
    DP_CHECK_MSG (dp_frame_add_field (e, "x", SYNC, 0) == -1,
                  "zero bits is no field");
    DP_CHECK_MSG (dp_frame_add_field (e, "x", digit, 2) == -1,
                  "an element that is not a bit is refused");
    DP_CHECK_MSG (dp_frame_n_fields (e) == 0, "and a refusal appends nothing");
    DP_CHECK (dp_frame_add_field (e, "x", SYNC, 13) == 0);
    DP_CHECK_MSG (dp_frame_add_field (e, "x", PAY, 16) == -1,
                  "a name another field carries is refused");
    DP_CHECK_MSG (dp_frame_n_fields (e) == 1, "...and appends nothing");
    dp_frame_destroy (e);
  }

  /* ── a CCSDS CADU, described from Python's side of the ABI ───────────
   *
   * The point of the whole exercise. `ccsds_tm` has no Python binding and is
   * not getting one, so this object is the only place a caller can reach the
   * outer code, the randomiser and the inner code -- and it reaches them by
   * DESCRIBING a frame, not by a CCSDS entry point being added here.
   *
   * Checked against dp_ccsds_tm_frame_encode byte for byte rather than against
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
    const size_t n = dp_ccsds_tm_frame_encode (&cfg, NULL, frame, octets, want,
                                               sizeof want);
    DP_REQUIRE (n != 0);

    uint8_t asm_bits[CCSDS_TM_ASM_BITS];
    dp_ccsds_tm_asm_bits (asm_bits);

    dp_frame_state_t *b = empty_desc ();
    DP_REQUIRE (b != NULL);
    /* [ ASM | Transfer Frame | R-S check symbols ] */
    DP_CHECK (dp_frame_add_field (b, "asm", asm_bits, CCSDS_TM_ASM_BITS) == 0);
    DP_CHECK (dp_frame_add_field (b, "data", fbits, octets * 8u) == 1);
    DP_CHECK (
        dp_frame_add_derived (b, "parity", (size_t)CCSDS_TM_RS_2E * DEPTH * 8u)
        == 2);
    /* The three covers ARE the coverage table: the outer code and the
       randomiser start behind the marker, the inner code does not. */
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_RS, 1, 2, DEPTH, 0, 0, 0) == 0);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_RANDOMISE, 1, 2, 0, 0, 0, 0)
              == 1);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_CONV, 0, 3, 0, 2, 1, 0) == 2);
    DP_REQUIRE_MSG (dp_frame_build (b) == 0,
                    "a CADU builds from a description");

    DP_CHECK_MSG (b->nbits == n, "the CADU is the length the encoder says");
    uint8_t *got = malloc (b->nbits);
    DP_REQUIRE (got && dp_frame_bits (b, 1, got, b->nbits) == b->nbits);
    DP_CHECK_MSG (memcmp (got, want, n) == 0,
                  "...and the SAME bits as dp_ccsds_tm_frame_encode, byte for "
                  "byte");
    free (got);
    dp_frame_destroy (b);
  }

  /* ── the block interleaver as a stage (doppler#1031) ──────────────────
   *
   * The kernels are BUILTIN, so nothing outside this file drives them at the
   * C level, and the coverage gate said so: 62.5% over wfm_frame.c's
   * interleave half. Chasing that number found a REAL bug -- the stage-slot
   * allocation had `s_ilv = ns++` nested under `if (convolutional)` with
   * `s_conv = ns++` left unconditional, so a frame with an interleaver and
   * no inner code allocated the wrong slot. Every test passed before and
   * after the fix; only the uncovered-lines report pointed at it. */
  {
    static const uint8_t sync[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
    uint8_t              payload[64];
    for (unsigned i = 0; i < 64; i++)
      payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

    /* [ sync | payload ], interleaving the PAYLOAD only. A sync word is what
       a receiver correlates to find the frame, so permuting it would destroy
       the thing that makes the frame findable. */
    dp_frame_state_t *b = empty_desc ();
    DP_REQUIRE (b != NULL);
    DP_CHECK (dp_frame_add_field (b, "sync", sync, 13) == 0);
    DP_CHECK (dp_frame_add_field (b, "payload", payload, 64) == 1);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_INTERLEAVE, 1, 1, 8, 0, 0, 0)
              == 0);
    DP_REQUIRE (dp_frame_build (b) == 0);

    /* Length-preserving, and the sync word is untouched. */
    DP_CHECK_MSG (b->nbits == 13 + 64, "a permutation adds no bits");
    uint8_t *tx = malloc (b->nbits);
    DP_REQUIRE (tx && dp_frame_bits (b, 1, tx, b->nbits) == b->nbits);
    DP_CHECK_MSG (memcmp (tx, sync, 13) == 0,
                  "the sync word is outside the stage's cover");
    DP_CHECK_MSG (memcmp (tx + 13, payload, 64) != 0,
                  "...and the payload actually moved");

    /* deframe() reverses it, which is the whole receive path. */
    uint8_t *rx = malloc (b->nbits);
    DP_REQUIRE (rx);
    DP_REQUIRE (dp_frame_deframe (b, tx, b->nbits, rx, b->nbits) == b->nbits);
    DP_CHECK_MSG (memcmp (rx + 13, payload, 64) == 0,
                  "de-interleaving recovers the payload exactly");
    DP_CHECK (b->rx_checked == 1);
    free (tx);
    free (rx);
    dp_frame_destroy (b);
  }

  /* A span the geometry cannot tile is REFUSED, not padded and not
     truncated: the column count follows from the span, so a remainder has
     nowhere to go. 64 bits with depth 8 and unit 8 needs a multiple of 64 and
     64 is one -- so this uses depth 5, where 64 % 40 != 0. */
  {
    uint8_t           payload[64] = { 0 };
    dp_frame_state_t *b           = empty_desc ();
    DP_REQUIRE (b != NULL);
    DP_CHECK (dp_frame_add_field (b, "payload", payload, 64) == 0);
    DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_INTERLEAVE, 0, 1, 5, 0, 0, 8)
              == 0);
    /* At BUILD, not at bits(): dp_frame_build materialises the frame, so a
       stage that cannot run refuses "at the point the caller can still do
       something about it" -- its own comment. Earlier than this test first
       assumed, and better. */
    DP_CHECK_MSG (dp_frame_build (b) != 0,
                  "a span that does not tile is refused, not padded");
    dp_frame_destroy (b);
  }

  /* unit_bits is carried on the stage and CHANGES the permutation -- depth 8
     over octets and depth 8 over bits are different orderings of the same
     span, which is why it is a field of its own rather than folded into
     depth. */
  {
    uint8_t payload[64];
    for (unsigned i = 0; i < 64; i++)
      payload[i] = (uint8_t)((i * 5u + 1u) & 1u);
    uint8_t bit_out[64], oct_out[64];
    for (unsigned pass = 0; pass < 2; pass++)
      {
        dp_frame_state_t *b = empty_desc ();
        DP_REQUIRE (b != NULL);
        DP_CHECK (dp_frame_add_field (b, "payload", payload, 64) == 0);
        DP_CHECK (dp_frame_add_stage (b, WFM_STAGE_INTERLEAVE, 0, 1, 8, 0, 0,
                                      pass ? 8u : 1u)
                  == 0);
        DP_REQUIRE (dp_frame_build (b) == 0);
        DP_REQUIRE (dp_frame_bits (b, 1, pass ? oct_out : bit_out, 64) == 64);
        dp_frame_destroy (b);
      }
    DP_CHECK_MSG (memcmp (bit_out, oct_out, 64) != 0,
                  "unit_bits selects a different permutation");
  }

  /* ── the by-name builder, fed from the text form ──────────────────────
   *
   * A marker written as text and the same marker expanded by hand must be
   * the SAME bits: if they disagreed, every marker built the text way would
   * sync to nothing. The text door is dp_wfm_field_bits, the one reader of
   * the grammar; this object only takes what it returns.
   */
  {
    /* 0x1ACFFC1D expanded by hand, MSB first -- the published ASM. */
    static const uint8_t asm_bits[32]
        = { 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1 };
    uint8_t text[32];
    DP_REQUIRE (dp_wfm_field_bits ("0x1ACFFC1D", text, sizeof text, NULL)
                == 32u);

    dp_frame_state_t *a = empty_desc ();
    dp_frame_state_t *b = empty_desc ();
    DP_REQUIRE (a != NULL && b != NULL);
    DP_CHECK (dp_frame_add_field (a, "asm", text, 32u) == 0);
    DP_CHECK (dp_frame_add_field (b, "asm", asm_bits, 32u) == 0);
    DP_CHECK (dp_frame_build (a) == 0 && dp_frame_build (b) == 0);

    uint8_t ba[64], bb[64];
    DP_CHECK (dp_frame_bits (a, 1u, ba, sizeof ba) == 32u);
    DP_CHECK (dp_frame_bits (b, 1u, bb, sizeof bb) == 32u);
    DP_CHECK_MSG (memcmp (ba, bb, 32u) == 0,
                  "a marker from text and a hand-expanded one are the same "
                  "bits");
    DP_CHECK_MSG (memcmp (ba, asm_bits, 32u) == 0,
                  "...and both are the PUBLISHED expansion, MSB first");
    dp_frame_destroy (a);
    dp_frame_destroy (b);
  }

  {
    dp_frame_state_t *d = empty_desc ();
    DP_REQUIRE (d != NULL);

    /* A whole frame by name: a 12-bit marker, a payload named after the
       fact, a derived CRC, and a stage that covers the pair -- the shape a
       caller actually writes. */
    uint8_t sync[12];
    DP_REQUIRE (dp_wfm_field_bits ("0xABC", sync, sizeof sync, NULL) == 12u);
    DP_CHECK (dp_frame_add_field (d, "sync", sync, 12u) == 0);
    const uint8_t pay[8] = { 0, 1, 1, 0, 1, 0, 0, 1 };
    DP_CHECK (dp_frame_add_field (d, "", pay, 8u) == 1); /* anonymous */
    DP_CHECK (dp_frame_name_field (d, 1u, "payload") == 0);
    DP_CHECK (dp_frame_add_derived (d, "crc", 16u) == 2);
    DP_CHECK (
        dp_frame_add_stage_over (d, 0 /* crc16 */, "payload", "crc", 0, 0)
        == 0);
    DP_CHECK (dp_frame_build (d) == 0);
    DP_CHECK_MSG (d->nbits == 12u + 8u + 16u,
                  "12 + 8 + 16 -- the CRC trailer is a FIELD");

    /* Names resolve, and to the right fields. */
    DP_CHECK (dp_frame_field_index (d, "sync") == 0);
    DP_CHECK (dp_frame_field_index (d, "payload") == 1);
    DP_CHECK (dp_frame_field_index (d, "crc") == 2);
    DP_CHECK (dp_frame_field_index (d, "nope") == -1);
    DP_CHECK_MSG (dp_frame_field_off (d, 1u) == 12u,
                  "the payload starts after the 12-bit sync");

    /* The stage really covers payload..crc, so the CRC it wrote verifies. */
    uint8_t rx[64];
    DP_CHECK (dp_frame_bits (d, 1u, rx, sizeof rx) == 36u);
    DP_CHECK_MSG (dp_frame_crc_ok (d, rx, 36u) == 1,
                  "the frame's own bits pass its own check");
    rx[12] ^= 1u;
    DP_CHECK_MSG (dp_frame_crc_ok (d, rx, 36u) == 0,
                  "...and a flipped payload bit fails it");

    /* Refusals: a duplicate name, and a cover naming nothing. */
    DP_CHECK_MSG (dp_frame_name_field (d, 0u, "payload") == -1,
                  "a rename onto a taken name is refused");
    dp_frame_destroy (d);
  }

  DP_TEST_END ("frame_core");
}
