/**
 * @file dp_frame_test.h
 * @brief The named starter frame set: one `wfm_frame_t`, five configurations.
 *
 * A frame is arbitrary by construction — that is the point of the descriptor
 * (`wfm/wfm_frame.h`). But an arbitrary frame per test is how a convention
 * goes wrong silently: two harnesses pick lengths that differ for no stated
 * reason, their numbers stop being comparable, and nothing says so. So the
 * harness ships **a handful of named ones** and a test says which it used.
 *
 * Every entry below is the SAME struct with different `wfm_seq_t` values.
 * There is no per-name code path, no switch in the builder that changes what
 * a field means, and no second layout — five rows of a table, materialised by
 * the one `wfm_frame_bits()`. Sync and payload draw from the same generators
 * independently, so a Gold sync with a PN payload is a configuration this set
 * simply does not happen to name.
 *
 * | name             | preamble       | sync        | payload    | crc | bits
 * | | ---------------- | -------------- | ----------- | ---------- | --- |
 * ---- | | `RX_FRAME_NONE`  | —              | —           | PN 304     | no
 * | 304 | | `RX_FRAME_BURST` | dotted x 64    | Barker-13   | PN 128     |
 * yes |  285 | | `RX_FRAME_CONT`  | dotted x 256   | PN 127      | PN 304 |
 * yes | 959 | | `RX_FRAME_GOLD`  | dotted x 256   | Gold 127    | Gold 304
 * | yes | 959 | | `RX_FRAME_ACQ`   | dotted x 256   | —           | — | no  |
 * 512 |
 *
 * ## What each one is FOR, which is what stops the set growing
 *
 * - **`RX_FRAME_NONE`** is the null case: an unframed PRBS, which is what
 *   every unspread receiver test uses today. It is here so that "the frame
 *   helped" is a measured claim rather than an assumption — it carries the
 *   SAME payload descriptor as `RX_FRAME_CONT`, so the two differ by the
 *   frame furniture and nothing else.
 * - **`RX_FRAME_BURST`** is the burst flavor, and its sync word is the
 *   Barker-13 every caller currently types as a literal (`wfmgen --sync`,
 *   `burst_demod`'s docstrings, `gen_wfmgen_flag_matrix.py`). Whether 13
 *   symbols can clear a Pfa-corrected detection threshold at a low Es/N0 is
 *   an open question in the design (section 6), and this is the configuration
 *   that measures it rather than deciding it.
 * - **`RX_FRAME_CONT`** is the continuous flavor at an Es/N0 floor, where 13
 *   bits will not do. Its 127-bit sync is one register period (`reg_bits =
 *   7`) and is a PLACEHOLDER pending that same measurement.
 *
 *   Its payload is **304 bits, and that length is load-bearing** (gh-796).
 *   At the 1024 it carried, the CRC protects 1040 bits, so at the battery's
 *   SER=1e-3 anchor roughly two thirds of frames fail on noise alone --
 *   and a frame error rate that high HIDES faults rather than exposing
 *   them. Corrupting every other frame's CRC adds `0.5 * (1 - FER)`, so
 *   with 0.68 already failing it moved the measurement only 0.68 -> 0.84,
 *   a factor of 1.23, and `dp_rx_check`'s FER anchor could not see it. At
 *   304 the baseline is 0.30 and the same sabotage reads 0.65, a factor of
 *   2.18, which the anchor rejects. Shortening the payload rather than
 *   raising Es/N0 keeps ONE Es/N0 across all four battery metrics, which
 *   is what makes them comparable.
 * - **`RX_FRAME_GOLD`** earns its place against `RX_FRAME_CONT` by isolating
 *   ONE variable: same preamble, same sync LENGTH, same payload length, same
 *   CRC — only the sequence family differs. That is what makes the
 *   bounded-cross-correlation argument measurable through `runner_db` instead
 *   of asserted.
 * - **`RX_FRAME_ACQ`** is preamble only: settling, AGC and coarse acquisition
 *   with nothing to demodulate. It is the shape a receiver sees before a
 *   burst, and the one whose FER is undefined by construction.
 *
 * ## Three choices that are not free, written down so they are not re-guessed
 *
 * **A dotted preamble is `len = 2` repeated N times, not `len = 2N` once.**
 * Its period is what a coherent integration across repetitions depends on, so
 * saying "the two-bit unit, 64 times" states the period; saying "128 bits of
 * alternating" states a length and leaves the period to be inferred. The
 * repetition count is also the field the DSSS acquisition contract uses, and
 * this is the only place in the set that exercises it.
 *
 * **`RX_FRAME_GOLD`'s registers are 10 bits wide while `RX_FRAME_CONT`'s are
 * 7.** A Gold family is only a Gold family when its two m-sequences are a
 * genuine PREFERRED PAIR, and doppler ships verified taps for exactly one such
 * pair — the CCSDS length-10 polynomials, whose three-valued correlation set
 * `test_gold_core.c` checks. `wfm_seq_t` names the output length apart from
 * the register width precisely so this is expressible: both syncs are 127 bits
 * OUT, which is the quantity the comparison holds equal. Widening CONT to 10
 * as well would hold the register equal instead and break the section 7.4
 * intent, which is a sync of one PN period.
 *
 * **Seeds differ between a frame's fields.** A sync and a payload drawn from
 * the same generator with the same seed are the same bits, and a sync-word
 * detector would then find its marker inside the payload — an ambiguity
 * invented by the test rather than by the waveform.
 *
 * @see docs/design/rx-test.md section 7.4
 */
#ifndef DP_FRAME_TEST_H
#define DP_FRAME_TEST_H

#include "wfm/wfm_frame.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Barker-13, the sync word every caller currently types as a literal.
 *
 * It appears as the string `"1111100110101"` in
 * `scripts/gen_wfmgen_flag_matrix.py`, as prose in `wfmgen --sync`'s help and
 * again in `burst_demod`'s docstrings. This is the harness's one copy; a test
 * that wants it says `RX_FRAME_BURST` instead of retyping it.
 */
static const uint8_t dp_frame_barker13[13]
    = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };

/** @brief The named frames. `DP_FRAME_COUNT` bounds a sweep over the set. */
typedef enum
{
  RX_FRAME_NONE  = 0, /**< unframed PRBS — the comparison baseline     */
  RX_FRAME_BURST = 1, /**< dotted preamble, Barker-13 sync, short PN   */
  RX_FRAME_CONT  = 2, /**< long preamble, PN-127 sync, long PN payload */
  RX_FRAME_GOLD  = 3, /**< as CONT, with the Gold family instead       */
  RX_FRAME_ACQ   = 4, /**< preamble only; nothing to demodulate        */
  DP_FRAME_COUNT = 5
} dp_frame_name_t;

/** @brief The name, for a report that has to say which frame it used. */
static inline const char *
dp_frame_label (dp_frame_name_t name)
{
  switch (name)
    {
    case RX_FRAME_NONE:
      return "RX_FRAME_NONE";
    case RX_FRAME_BURST:
      return "RX_FRAME_BURST";
    case RX_FRAME_CONT:
      return "RX_FRAME_CONT";
    case RX_FRAME_GOLD:
      return "RX_FRAME_GOLD";
    case RX_FRAME_ACQ:
      return "RX_FRAME_ACQ";
    case DP_FRAME_COUNT:
      break;
    }
  return "?";
}

/* The CCSDS Command Link preferred pair (CCSDS 415.0-G-1 5.2.2.4), which is
   gold_create()'s documented default and the one whose three-valued
   correlation set test_gold_core.c verifies. Register width 10. */
#define DP_FRAME_GOLD_TAPS_A 934u
#define DP_FRAME_GOLD_TAPS_B 567u
#define DP_FRAME_GOLD_BITS 10u

/**
 * @brief The named frame @p name, by value.
 *
 * Returns a frame with `preamble.len == 0` for a name outside the set, which
 * `wfm_frame_nbits()` reports as 0 bits — a caller that fails to check gets an
 * empty frame it cannot transmit, not a plausible one it can.
 */
static inline wfm_frame_t
dp_frame_named (dp_frame_name_t name)
{
  wfm_frame_t f = { 0 };

  /* Every frame that has a preamble has the SAME one, differing only in how
     many periods of it there are — so a comparison between two of them is not
     also a comparison between two acquisition targets. */
  f.preamble.kind = WFM_SEQ_DOTTED;
  f.preamble.len  = 2; /* one period of 1010...; see the file docstring */

  switch (name)
    {
    case RX_FRAME_NONE:
      /* No preamble, no sync, no CRC: the unframed PRBS a receiver test uses
         today. The payload descriptor is RX_FRAME_CONT's, verbatim, so the
         pair differs by the frame and by nothing else. */
      f.preamble.len     = 0;
      f.preamble_reps    = 0;
      f.payload.kind     = WFM_SEQ_PN;
      f.payload.len      = 304;
      f.payload.reg_bits = 11;
      f.payload.seed     = 1;
      f.crc              = 0;
      return f;

    case RX_FRAME_BURST:
      f.preamble_reps    = 64; /* 128 preamble bits */
      f.sync.kind        = WFM_SEQ_LITERAL;
      f.sync.bits        = dp_frame_barker13;
      f.sync.len         = sizeof dp_frame_barker13;
      f.payload.kind     = WFM_SEQ_PN;
      f.payload.len      = 128;
      f.payload.reg_bits = 9;
      f.payload.seed     = 3;
      f.crc              = 1;
      return f;

    case RX_FRAME_CONT:
      f.preamble_reps    = 256; /* 512 preamble bits */
      f.sync.kind        = WFM_SEQ_PN;
      f.sync.len         = 127; /* one period of a 7-bit register */
      f.sync.reg_bits    = 7;
      f.sync.seed        = 5;
      f.payload.kind     = WFM_SEQ_PN;
      f.payload.len      = 304;
      f.payload.reg_bits = 11;
      f.payload.seed     = 1;
      f.crc              = 1;
      return f;

    case RX_FRAME_GOLD:
      /* Identical geometry to RX_FRAME_CONT; only the sequence family moves.
       */
      f.preamble_reps    = 256;
      f.sync.kind        = WFM_SEQ_GOLD;
      f.sync.len         = 127;
      f.sync.reg_bits    = DP_FRAME_GOLD_BITS;
      f.sync.taps_a      = DP_FRAME_GOLD_TAPS_A;
      f.sync.seed_a      = 350;
      f.sync.taps_b      = DP_FRAME_GOLD_TAPS_B;
      f.sync.seed_b      = 73;
      f.payload.kind     = WFM_SEQ_GOLD;
      f.payload.len      = 304;
      f.payload.reg_bits = DP_FRAME_GOLD_BITS;
      f.payload.taps_a   = DP_FRAME_GOLD_TAPS_A;
      f.payload.seed_a   = 511;
      f.payload.taps_b   = DP_FRAME_GOLD_TAPS_B;
      f.payload.seed_b   = 97;
      f.crc              = 1;
      return f;

    case RX_FRAME_ACQ:
      f.preamble_reps = 256; /* 512 preamble bits, and nothing else */
      return f;

    case DP_FRAME_COUNT:
      break;
    }

  f.preamble.len = 0;
  return f;
}

#endif /* DP_FRAME_TEST_H */
