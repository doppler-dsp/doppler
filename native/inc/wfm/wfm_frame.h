/**
 * @file wfm_frame.h
 * @brief A frame's BIT layout, described once and read from both ends.
 *
 * One struct saying what a frame contains, used by the generator that builds
 * it and by the measurer that scores it. The DSSS assembler already stated the
 * reason it must be shared — it is "assembled in one place so TX and RX can
 * never drift" — and this generalises that from one waveform to all of them:
 * `wfm_frame_dsss_chips()` now builds these bits and spreads them, rather than
 * carrying a second copy of the layout.
 *
 * ## It describes BITS
 *
 * Not chips, not samples, not levels. Spreading, pulse shaping, oversampling,
 * carrier and SNR layer above and stay `wfm_synth`'s job. That boundary is
 * what lets one descriptor serve an unspread BPSK stream and a two-code DSSS
 * burst alike.
 *
 * ## Every field is a sequence, and the generators already exist
 *
 * The preamble, the sync word and the payload are all `wfm_seq_t`, so "a Gold
 * sync" is a configuration rather than a feature, and `pn_create()` /
 * `gold_create()` stay the only implementations of those sequences.
 *
 * **The generated kinds are the ones that matter.** A literal array is what a
 * caller with real data has; a PN or Gold descriptor is a handful of numbers a
 * receiver can REGENERATE, which is what makes a long-record BER practical —
 * truth for a million-symbol run without a million-symbol array, and a capture
 * reproducible from its metadata alone.
 *
 * ## The CRC is the one we already have
 *
 * `dp_crc16_ccitt()`, over the payload only, MSB-first, carried as the same
 * `int crc` flag `wfm_frame_dsss_chips()` already took. A second CRC would be
 * a wire-format decision and nothing is asking for one.
 *
 * @see docs/design/rx-test.md section 7
 */
#ifndef WFM_FRAME_H
#define WFM_FRAME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Bits of CRC-16-CCITT, when a frame carries one. */
#define WFM_FRAME_CRC_BITS 16u

  /** @brief Where a run of bits comes from. */
  typedef enum
  {
    WFM_SEQ_LITERAL = 0, /**< a 0/1 array the caller owns                  */
    WFM_SEQ_PN      = 1, /**< pn_create()   — m-sequence, one LFSR         */
    WFM_SEQ_GOLD    = 2, /**< gold_create() — two LFSRs, a Gold family     */
    WFM_SEQ_DOTTED  = 3  /**< alternating 1010…; a line at Rs/2 to settle on */
  } wfm_seq_kind_t;

  /**
   * @brief A run of bits, however it is produced.
   *
   * @p len is always the OUTPUT length in bits. For the generated kinds it is
   * independent of the register width — `pn_create()`'s `length` argument is
   * the register width (period `2^n - 1`), while `pn_generate(state, n, …)`
   * decides how many bits come out. Conflating the two is easy and costly, so
   * they are named apart here: @p reg_bits against @p len.
   */
  typedef struct
  {
    wfm_seq_kind_t kind;
    size_t         len; /**< output bits; 0 means the field is absent      */

    const uint8_t *bits; /**< LITERAL only; NULL otherwise                 */

    /* PN: pn_create (poly, seed, reg_bits, lfsr) */
    uint64_t poly; /**< 0 selects `pn_mls_poly(reg_bits)` — the same
                        "default" `wfm_synth`'s `--pn-poly` means. A literal
                        0 reaching pn_create() is a register with no
                        feedback: it emits the seed and then zeros, which is
                        a CONSTANT field that still looks like a field.    */
    uint64_t seed;     /**< 0 selects 1; an all-zero register is a fixed point */
    uint32_t reg_bits; /**< register width 1..64; period 2^reg_bits - 1    */
    int      lfsr;     /**< PN_GALOIS (0) or PN_FIBONACCI (1)              */

    /* GOLD: gold_create (taps_a, seed_a, taps_b, seed_b, reg_bits) */
    uint64_t taps_a, seed_a, taps_b, seed_b;
  } wfm_seq_t;

  /** @brief Fields one description may carry. */
#define WFM_FRAME_MAX_FIELDS 8
  /** @brief Stages one description may carry. */
#define WFM_FRAME_MAX_STAGES 6

  /**
   * @brief A run of bits inside the assembled frame, `[first, first + n)`.
   *
   * A stage that did not run reports `n == 0`, and its @p first is then zero
   * as well; @p first is meaningful only for a stage that ran.
   */
  typedef struct
  {
    size_t first; /**< first frame-bit index the stage covers */
    size_t n;     /**< bits covered, or 0 if the stage did not run */
  } wfm_frame_span_t;

  /**
   * @brief One field of a frame — a run of bits that appears on the wire.
   *
   * Either the caller supplies the bits (@p seq, any @ref wfm_seq_kind_t) or
   * a stage produces them (@p derived_by non-zero: a CRC trailer, a block of
   * Reed-Solomon check symbols). Both are fields, because both are on the
   * wire, and making the second one a field is what removes the need for a
   * stage to expand the field it covers.
   */
  typedef struct
  {
    wfm_seq_t seq;  /**< the bits, when the caller supplies them        */
    size_t    reps; /**< repetitions of @p seq, verbatim; 0 means one   */
    size_t    bits; /**< derived only: length in bits, sized by its stage */
    /** 0 when the caller supplies this field; otherwise the index of the
        producing stage, plus one. The `+1` is so a zero-initialised field is
        a caller-supplied one rather than silently the output of stage 0. */
    unsigned derived_by;
  } wfm_field_t;

  /** @brief What a stage does to the fields it covers. */
  typedef enum
  {
    WFM_STAGE_CRC16     = 0, /**< dp_crc16_ccitt over the covered input  */
    WFM_STAGE_RS        = 1, /**< a Reed-Solomon code, interleaved       */
    WFM_STAGE_RANDOMISE = 2, /**< XOR a pseudo-random sequence, in place */
    WFM_STAGE_CONV      = 3  /**< a convolutional code                   */
  } wfm_stage_kind_t;

  /**
   * @brief One transform, and — the whole point — the fields it covers.
   *
   * **@p n_fields is load-bearing, not a refinement.** `ccsds_tm_frame.h`
   * states the failure this prevents: *"any chain of optional transforms
   * applied to 'the frame' is right at three stage boundaries and wrong at
   * the fourth, and wrong in the direction that still encodes, still decodes
   * against itself, and syncs to nothing."* A stage that inherited "whatever
   * ran before me" would be that chain. CCSDS is the case that proves it —
   * the marker is covered by the inner code and by neither the outer code nor
   * the randomiser — and any frame with a sync word has the same shape.
   *
   * The cover is what the stage OCCUPIES on the wire, so for a code it is the
   * information *and* the check symbols it derives. What the stage reads is
   * the cover minus the fields it derives, which is why both are one
   * declaration rather than two that can disagree.
   */
  typedef struct
  {
    wfm_stage_kind_t kind;
    unsigned         first_field; /**< first field covered                */
    unsigned         n_fields;    /**< fields covered; 0 = does not run   */
    unsigned         depth;       /**< RS: interleaving depth             */

    /** A stage that consumes the assembled frame and emits a DIFFERENT
        stream sets these: the output is `n * emit_num / emit_den` bits.
        `emit_num == 0` means the stage stays inside the frame. Only the
        inner code does the former today, and it is exactly why
        @ref wfm_frame_desc_layout_t reports @p frame_bits and @p out_bits as
        two numbers rather than one. */
    unsigned emit_num, emit_den;
  } wfm_stage_t;

  /**
   * @brief A frame as a description: what is on the wire, and what covers it.
   *
   * Two lists, ordered independently, because order and coverage are
   * independent axes: @p field is ordered by POSITION on the wire and
   * @p stage by APPLICATION. In a CCSDS CADU the marker is inserted third and
   * covered by the stage applied fourth, which a single ordered list cannot
   * say.
   *
   * A standard's framing is a CONFIGURATION of this, in the same way
   * `CCSDS_TM_CONV` configures `conv_code_t` and `CCSDS_TM_RS` configures
   * `rs_code_t`. @ref wfm_frame_t is the first such configuration and is
   * built by @ref wfm_frame_describe.
   *
   * @see docs/design/frame-description.md
   */
  typedef struct
  {
    wfm_field_t field[WFM_FRAME_MAX_FIELDS];
    unsigned    n_fields;
    wfm_stage_t stage[WFM_FRAME_MAX_STAGES];
    unsigned    n_stages;
  } wfm_frame_desc_t;

  /** @brief Where every field and every stage landed. */
  typedef struct
  {
    size_t   field_off[WFM_FRAME_MAX_FIELDS];  /**< bit offset per field  */
    size_t   field_bits[WFM_FRAME_MAX_FIELDS]; /**< bits per field        */
    unsigned n_fields;

    wfm_frame_span_t stage[WFM_FRAME_MAX_STAGES]; /**< what each stage covers   */
    unsigned   n_stages;

    size_t frame_bits; /**< the assembled frame, every field end to end   */
    size_t out_bits;   /**< what leaves the last stage that emits a new
                            stream; equals @p frame_bits when none does   */
  } wfm_frame_desc_layout_t;

  /**
   * @brief What undoing one stage found.
   *
   * One shape for every checking stage, because a caller doing frame
   * accounting wants to compare them rather than learn a struct per code. A
   * CRC reports one unit that is either good or not; an interleaved outer
   * code reports one unit per codeword, with the repair work it did.
   *
   * @p corrected and @p symbols are the honest measure of how hard the link
   * is running: `ok == units` with a rising @p symbols is a margin being
   * spent, and it is spent before it is lost.
   */
  typedef struct
  {
    unsigned units;     /**< things checked: codewords, or 1 for a CRC     */
    unsigned ok;        /**< how many are good AFTERWARDS — clean or fixed */
    unsigned corrected; /**< how many needed and received repair           */
    unsigned symbols;   /**< symbol errors repaired across the span        */
    int      checked;   /**< 0 when the receiver does not reverse this
                             stage here; its counts are then meaningless  */
  } wfm_frame_stage_rx_t;

  /**
   * @brief What @ref wfm_frame_check found, stage by stage.
   *
   * Indexed the same as the description's stages, so a caller reads the
   * result beside the declaration that produced it.
   */
  typedef struct
  {
    wfm_frame_stage_rx_t stage[WFM_FRAME_MAX_STAGES];
    unsigned             n_stages;
    unsigned             checked; /**< stages actually reversed here       */
  } wfm_frame_rx_t;

  /**
   * @brief How one kind of stage actually transforms bits.
   *
   * The description is pure data and names a stage by @ref wfm_stage_kind_t;
   * this is where the arithmetic for that kind comes from. The split is a
   * LAYERING requirement, not a taste: `ccsds_tm` must depend on this file to
   * describe a CADU, so this file must not call `ccsds_tm`'s kernels, or the
   * two components form a cycle. The kernels arrive as a table instead, from
   * whichever component owns them.
   *
   * It is also what makes the description open. A caller with a stage doppler
   * has never heard of supplies its own entry rather than waiting for an enum
   * to grow.
   *
   * Exactly one of the two is set. @p in_unit rewrites the stage's span where
   * it lies; @p emit consumes the assembled frame and produces a different
   * stream.
   *
   * **A stage's derived field is the LAST field of its cover**, which is what
   * lets one in-place signature serve a CRC, an outer code and a randomiser
   * alike: the op receives the whole span, reads the information at its head
   * and writes the check symbols into its tail. @ref wfm_frame_desc_layout
   * refuses a description that breaks it.
   */
  typedef struct
  {
    wfm_stage_kind_t kind;

    /** Rewrite @p n bits at @p bits, in place. Returns 0 on success. */
    int (*in_unit) (const wfm_stage_t *st, uint8_t *bits, size_t n,
                    void *user);

    /** Consume @p n bits at @p in and write the new stream to @p out.
        Returns the bits written, or 0 on refusal. @p out may overlap @p in:
        the frame is assembled in the TAIL of the caller's buffer and the
        stream is written from its head, so an implementation must read each
        input bit before writing the output bits that displace it — which is
        the order any expanding code writes in anyway. */
    size_t (*emit) (const wfm_stage_t *st, const uint8_t *in, size_t n,
                    uint8_t *out, size_t max_out, void *user);

    /** Undo the stage over its span on the RECEIVE side, correcting @p bits
        in place and reporting what was found. Returns 0 on success, -1 if
        the span is the wrong shape for this stage.

        A stage with no @p undo is not an error — it is a stage the receiver
        does not reverse HERE. The inner code is the case: it is streaming
        and emits its decisions `depth` bits late, so it is undone before
        frame synchronisation and a frame checker never sees channel symbols.
        @ref wfm_frame_check reports such a stage as not-checked rather than
        as passed, which are different answers. */
    int (*undo) (const wfm_stage_t *st, uint8_t *bits, size_t n,
                 wfm_frame_stage_rx_t *rx, void *user);
  } wfm_stage_op_t;

  /**
   * @brief The kernels an assembly runs, and whatever state they carry.
   *
   * Looked up by kind, and EXTENDS the built-ins rather than replacing them —
   * so a table supplying an outer code does not have to restate the CRC. A
   * stage whose kind is in neither table is a **refusal**, never a silent
   * skip: a stage that quietly did not run produces a frame that still
   * assembles, still decodes against itself, and syncs to nothing.
   */
  typedef struct
  {
    const wfm_stage_op_t *op;   /**< table, looked up by kind         */
    unsigned              n_op; /**< entries in @p op                 */
    void                 *user; /**< handed to every op it calls      */
  } wfm_frame_ops_t;

  /**
   * @brief Materialise a description: run every field, then every stage.
   *
   * The general form of @ref wfm_frame_bits. Fields are written in wire
   * order, then each stage is applied over the span
   * @ref wfm_frame_desc_layout gave it — over that span and no other, which
   * is the whole content of the coverage table a standard's framing turns
   * out to be.
   *
   * @param d        the description.
   * @param ops      kernels for the stage kinds beyond the built-in CRC;
   *                 may be `NULL` when there are none.
   * @param out      receives the unpacked output, one bit per byte.
   * @param max_out  capacity of @p out in bits; must be at least the
   *                 layout's `out_bits`.
   * @return The bits written, or 0 if the description is refused, a stage
   *         has no kernel, a field cannot be built, or @p max_out is too
   *         small — in which case @p out is untouched.
   */
  size_t wfm_frame_assemble (const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, uint8_t *out,
                             size_t max_out);

  /**
   * @brief Derive every field offset, every stage span and both lengths.
   *
   * The one operation both shipped framers already have, widened: this is
   * `wfm_frame_layout()`'s arithmetic and `ccsds_tm_frame_layout()`'s, with
   * the field and stage lists supplied rather than fixed.
   *
   * A derived field whose producing stage covers no caller-supplied bits is
   * dropped to zero length — which is the general form of the rule
   * @ref wfm_frame_layout has always applied, that a CRC over an empty
   * payload protects nothing and is not emitted.
   *
   * @param d    the description.
   * @param out  receives the layout.
   * @return 0, or -1 if @p d or @p out is NULL, or a count or a cover runs
   *         past its array.
   */
  int wfm_frame_desc_layout (const wfm_frame_desc_t  *d,
                             wfm_frame_desc_layout_t *out);

  /**
   * @brief A frame's bit layout: `[preamble × reps | sync | payload | crc]`.
   *
   * The preamble sits OUTSIDE the sync/payload/CRC group, matching the DSSS
   * contract this generalises: it is unmodulated, it is not covered by the
   * CRC, and in the spread case it is not spread. It is the
   * coherent-integration target.
   *
   * This is a **configuration** of @ref wfm_frame_desc_t — four fields and
   * one stage — not a second descriptor. @ref wfm_frame_layout builds it
   * through @ref wfm_frame_describe and reads the general layout back, so
   * there is one implementation of the arithmetic and the two cannot drift.
   */
  typedef struct
  {
    wfm_seq_t preamble;      /**< len 0 = none                             */
    size_t    preamble_reps; /**< repetitions of @p preamble; 0 = none     */
    wfm_seq_t sync;          /**< len 0 = unsynced — BER then needs an
                                  external alignment                       */
    wfm_seq_t payload;
    int       crc; /**< non-zero: CRC-16-CCITT over the payload, MSB-first */
  } wfm_frame_t;

  /** @brief Where each field lands, in bits from the start of the frame. */
  typedef struct
  {
    size_t preamble_off, preamble_bits;
    size_t sync_off, sync_bits;
    size_t payload_off, payload_bits;
    size_t crc_off, crc_bits; /**< 16, or 0 when @p crc is unset or the
                                   payload is empty — a CRC over nothing
                                   protects nothing                        */
    size_t total_bits;
  } wfm_frame_layout_t;

  /** @brief Field indices @ref wfm_frame_describe writes, in wire order. */
  enum
  {
    WFM_FRAME_FIELD_PREAMBLE = 0,
    WFM_FRAME_FIELD_SYNC     = 1,
    WFM_FRAME_FIELD_PAYLOAD  = 2,
    WFM_FRAME_FIELD_CRC      = 3
  };

  /**
   * @brief Express a @ref wfm_frame_t as a @ref wfm_frame_desc_t.
   *
   * The bridge that makes the closed struct a configuration rather than a
   * rival: four fields in wire order, plus one CRC stage covering the payload
   * and the trailer it derives. Exported because it is also the worked
   * example — the shortest complete answer to "what does a description of my
   * frame look like".
   *
   * @param f    the frame.
   * @param out  receives the description.
   * @return 0, or -1 if either argument is NULL.
   */
  int wfm_frame_describe (const wfm_frame_t *f, wfm_frame_desc_t *out);

  /**
   * @brief Total frame bits, or 0 if the geometry is empty.
   *
   * @param f  the frame; must be non-NULL.
   */
  size_t wfm_frame_nbits (const wfm_frame_t *f);

  /**
   * @brief Fill @p out with the field offsets.
   *
   * The arithmetic both directions need, computed once. Today it is inline in
   * `wfm_frame_dsss_nchips()`, and a receiver scoring a frame would have to
   * recompute it — which is exactly how TX and RX drift apart.
   *
   * @return 0, or -1 if @p f or @p out is NULL.
   */
  int wfm_frame_layout (const wfm_frame_t *f, wfm_frame_layout_t *out);

  /**
   * @brief Materialise the frame as one flat 0/1 bit array.
   *
   * Generated fields are produced here, from the descriptor, so a receiver
   * holding the same handful of numbers regenerates the identical bits.
   *
   * @param f        the frame.
   * @param out      output, one bit per byte.
   * @param max_out  capacity of @p out.
   * @return bits written, or 0 if the geometry is empty, a field is
   *         unbuildable (a LITERAL with no array, a PN with no register
   *         width), or @p max_out is too small.
   */
  size_t wfm_frame_bits (const wfm_frame_t *f, uint8_t *out, size_t max_out);

  /**
   * @brief Undo a description's stages over a received frame, and report.
   *
   * The receive mirror of @ref wfm_frame_assemble, reading the same
   * description — so the two cannot disagree about which stage covered what,
   * which is the failure the whole representation exists to prevent. Stages
   * are reversed in the OPPOSITE order to the one they were applied in, each
   * over the span the layout gives it.
   *
   * **This is what makes a truth-free frame error rate possible on a coded
   * link, and it is a strictly better detector than a CRC.** A CRC says one
   * bit: right or wrong. An outer code says *how much repair it took* —
   * `ok == units` with a rising @c symbols is margin being spent, visible
   * before it is lost. A caller wanting only good frames compares @c ok with
   * @c units; one doing accounting reads the rest.
   *
   * It begins AFTER the inner code and after frame synchronisation, for the
   * reason `ccsds_tm_frame.h` gives at length: a Viterbi is streaming and
   * emits its decisions `depth` bits late, so the bits of one frame are not a
   * function of that frame's symbols alone, and the marker that says where a
   * frame starts is only readable once the inner code is undone. A stage with
   * no @c undo kernel is reported as **not checked**, never as passed.
   *
   * @param d        the description the bits are laid out by.
   * @param ops      kernels for the stage kinds beyond the built-in CRC;
   *                 may be `NULL`.
   * @param bits     the layout's `frame_bits` received bits, one per byte,
   *                 CORRECTED IN PLACE by any stage that repairs.
   * @param rx       receives the per-stage outcome; may be `NULL`.
   * @return 1 when every stage that was checked came out good, 0 when one did
   *         not, or -1 if the description is refused. **A description with no
   *         checking stage at all returns -1**, not 1: "carries no check" and
   *         "the check passed" are different answers, and an FER that
   *         conflated them would score every unprotected frame as perfect.
   */
  int wfm_frame_check (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
                       uint8_t *bits, wfm_frame_rx_t *rx);

  /**
   * @brief Check a received frame's CRC against any description that has one.
   *
   * The general form of @ref wfm_frame_crc_ok, and the same truth-free claim:
   * it needs the description and the received bits and no payload truth at
   * all. What the CRC protects is everything its stage covers except the
   * trailer that stage derived — read back from the same rule the assembler
   * writes by, so the two cannot disagree about where the trailer is.
   *
   * @param d        the description the bits are laid out by.
   * @param rx_bits  received bits, the layout's `frame_bits` of them.
   * @return 1 pass, 0 fail, -1 if the description carries no CRC stage (or on
   *         NULL). The three are distinct on purpose: an FER that read
   *         "carries no check" as "the check failed" would count every
   *         unprotected frame as an error.
   */
  int wfm_frame_desc_crc_ok (const wfm_frame_desc_t *d,
                             const uint8_t          *rx_bits);

  /**
   * @brief Check a received frame's CRC in place.
   *
   * **This is what makes a truth-free frame error rate possible.** It needs
   * the layout and the received bits and no payload truth at all — so it works
   * on a real capture, and unlike a self-referenced EVM or a blind M2M4 it
   * still catches a false lock, because a rotated constellation fails the
   * check rather than looking clean.
   *
   * @param f        the frame the bits are laid out by.
   * @param rx_bits  received bits, `wfm_frame_nbits(f)` of them.
   * @return 1 pass, 0 fail, -1 if the frame carries no CRC (or on NULL).
   */
  int wfm_frame_crc_ok (const wfm_frame_t *f, const uint8_t *rx_bits);

#ifdef __cplusplus
}
#endif

#endif /* WFM_FRAME_H */
