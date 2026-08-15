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

  /**
   * @brief A frame's bit layout: `[preamble × reps | sync | payload | crc]`.
   *
   * The preamble sits OUTSIDE the sync/payload/CRC group, matching the DSSS
   * contract this generalises: it is unmodulated, it is not covered by the
   * CRC, and in the spread case it is not spread. It is the
   * coherent-integration target.
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
