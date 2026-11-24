/**
 * @file ccsds_415_frame.h
 * @brief One period of the CCSDS 415.1-B-1 point's stimulus as a frame
 *        description: the gap, the acquisition sequence, the start sequence
 *        and one CCSDS TC transfer frame (232.0-B-4), field by field.
 *
 * docs/design/async-dsss-receiver.md §1.4 states the period; this is that
 * table as DATA, a configuration of the general description in
 * `wfm/wfm_frame.h` (which knows nothing about CCSDS), as
 * dp_ccsds_tm_frame_describe() is 131.0-B's. Every field is named, so a
 * caller reads a field's place by name (dp_wfm_frame_field_index(),
 * dp_wfm_frame_desc_layout()) and checks the frame with
 * dp_wfm_frame_desc_crc_ok() or dp_wfm_frame_check().
 *
 * | field      | bits           | content                                      |
 * | ---------- | -------------- | -------------------------------------------- |
 * | `gap`      | `gap_sym`      | zeros: the code alone (415.1-B-1 §6.1)       |
 * | `acq`      | 128            | 1010...: the acquisition sequence (231.0-B-4 §7.2.2), WFM_SEQ_DOTTED |
 * | `start`    | 16             | EB90, bit 0 first (231.0-B-4 §5.2.2.2)       |
 * | `version`  | 2              | `00` (232.0-B-4 §4.1.2.2.2)                  |
 * | `bypass`   | 1              | `1`, Type-B (§4.1.2.3.1)                     |
 * | `ctrl_cmd` | 1              | `0`, Type-D (§4.1.2.3.2)                     |
 * | `spare`    | 2              | `00` (§4.1.2.4.2)                            |
 * | `scid`     | 10             | the spacecraft identifier (§4.1.2.5)         |
 * | `vcid`     | 6              | the virtual channel identifier (§4.1.2.6)    |
 * | `length`   | 10             | the frame's octets minus one (§4.1.2.7.2)    |
 * | `seq`      | 8              | the frame sequence number (§4.1.2.8)         |
 * | `data`     | 8 x octets     | the frame's data source (WFM_SEQ_DATA)       |
 * | `fecf`     | 16             | CRC-16 over `version` .. `data` (§4.1.4)     |
 *
 * The FECF is the description's own CRC stage, WFM_STAGE_CRC16: CRC-16 with
 * G(X) = X^16 + X^12 + X^5 + 1, preset to all ones, no final inversion
 * (dp_crc16_ccitt(), DP_CRC_16_IBM_3740), which is 232.0-B-4 §4.1.4.2's.
 *
 * **Field 0 is the gap, not the start sequence**, so
 * dp_wfm_frame_desc_rx(), which reads a receiver's sync word from field 0,
 * refuses this description (test_ccsds_415_frame.c pins the refusal). A
 * receiver finds the frame by searching for the start sequence itself:
 * detection's SyncFinder on the `start` field's bits.
 *
 * The description BORROWS the bit arrays in its dp_ccsds_415_frame_t, so
 * use it in place: a copy of the struct points into the original.
 *
 * @code
 * #include <doppler/ccsds/ccsds_415_frame.h>
 *
 * dp_ccsds_415_frame_t        f;
 * dp_ccsds_415_frame_cfg_t    cfg = DP_CCSDS_415_FRAME_CFG_DEFAULTS;
 * wfm_frame_desc_layout_t     lay;
 * if (dp_ccsds_415_frame_desc (&f, &cfg, 17, NULL) != 0
 *     || dp_wfm_frame_desc_layout (&f.desc, &lay) != 0)
 *   return 1;
 * // lay.frame_bits == 275 + 128 + 16 + 40 + 17 * 8 + 16
 * @endcode
 */
#ifndef DP_CCSDS_415_FRAME_H
#define DP_CCSDS_415_FRAME_H

#include "doppler/ccsds/ccsds_415.h"
#include "doppler/wfm/wfm_frame.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Bits of the acquisition sequence (231.0-B-4 §7.2.2's preferred
 *  minimum, 16 octets). */
#define DP_CCSDS_415_ACQ_BITS 128u
/** The start sequence, EB90, sent from its most significant bit. */
#define DP_CCSDS_415_START_SEQ 0xEB90u
/** Bits of the start sequence. */
#define DP_CCSDS_415_START_BITS 16u
/** Bits of the transfer frame's primary header (232.0-B-4 §4.1.2). */
#define DP_CCSDS_415_HEADER_BITS 40u
/** The fewest data octets a frame carries (232.0-B-4 §4.1.3.1.2). */
#define DP_CCSDS_415_DATA_MIN 1u
/** The most data octets a frame carries with its FECF present
 *  (232.0-B-4 §4.1.3.1.2): a 1,024-octet frame. */
#define DP_CCSDS_415_DATA_MAX 1017u

/** The values §1.4 leaves arbitrary (a receiver at the point reads none of
 *  them), and the gap. */
typedef struct
{
  /** Spacecraft identifier, 0 to 1,023. */
  unsigned scid;
  /** Virtual channel identifier, 0 to 63. */
  unsigned vcid;
  /** Frame sequence number, 0 to 255 (0: the value the COP sets on a
   *  Type-B frame). */
  unsigned seq;
  /** Gap symbols ahead of the acquisition sequence: the code alone. */
  size_t gap_sym;
} dp_ccsds_415_frame_cfg_t;

/** §1.4's values: SCID 42, VCID 1, sequence 0, and a gap of the point's
 *  code-only symbols (#DP_CCSDS_415_CODE_ONLY_SYM). */
#define DP_CCSDS_415_FRAME_CFG_DEFAULTS                                       \
  { 42u, 1u, 0u, DP_CCSDS_415_CODE_ONLY_SYM }

/** A frame description and the bits it borrows: use it in place. */
typedef struct
{
  /** The description; its literal fields point into the arrays below. */
  wfm_frame_desc_t desc;
  /** The gap's one bit, a zero, repeated `gap_sym` times. */
  uint8_t zero;
  /** The start sequence, one bit a byte, MSB-first. */
  uint8_t start[DP_CCSDS_415_START_BITS];
  /** The primary header, one bit a byte, MSB-first. */
  uint8_t header[DP_CCSDS_415_HEADER_BITS];
} dp_ccsds_415_frame_t;

/**
 * @brief Describe one period of the 415.1-B-1 stimulus with a frame of
 *        @p data_octets data octets.
 *
 * Fills @p f in place (see the file comment for the fields). The data field
 * is a WFM_SEQ_DATA field: its bits come from the caller at assembly
 * (dp_wfm_frame_assemble_data()). The header's `length` is written here,
 * `5 + data_octets + 2 - 1`, since a description is one frame of one known
 * length.
 *
 * @param f            receives the description and the bits it borrows.
 * @param cfg          the arbitrary values and the gap; NULL takes
 *                     #DP_CCSDS_415_FRAME_CFG_DEFAULTS.
 * @param data_octets  the data field's octets, #DP_CCSDS_415_DATA_MIN to
 *                     #DP_CCSDS_415_DATA_MAX.
 * @param why          on refusal, receives a static sentence naming the
 *                     fix; may be NULL.
 * @return 0, or -1 with @p why set: data octets out of range, a value that
 *         does not fit its field, a zero gap, or NULL @p f.
 *
 * @code
 * dp_ccsds_415_frame_t f;
 * const char          *why = NULL;
 * if (dp_ccsds_415_frame_desc (&f, NULL, 1018, &why) != 0)
 *   puts (why);   // the data field holds 1 to 1,017 octets ...
 * @endcode
 */
int dp_ccsds_415_frame_desc (dp_ccsds_415_frame_t           *f,
                             const dp_ccsds_415_frame_cfg_t *cfg,
                             size_t data_octets, const char **why);

#ifdef __cplusplus
}
#endif

#endif /* DP_CCSDS_415_FRAME_H */
