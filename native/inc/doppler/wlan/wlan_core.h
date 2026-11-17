/**
 * @file wlan_core.h
 * @brief IEEE 802.11 non-HT OFDM (Clause 17) builders: a PSDU in, the
 *        constellation points and the layout the OFDM modulator takes out.
 *
 * `wlan` is a table of PPDU formats and a builder from a format and a PSDU
 * to a layout and the constellation points the layout takes
 * (docs/design/wlan.md section 3). It calls the library's own blocks: the
 * parametric CRC for the FCS, `conv` with puncturing for the BCC, the block
 * interleaver's first permutation, the M-PSK and QAM maps, and the OFDM
 * modulator (ofdm_core.h) for the layout. What it adds is the 802.11 rules
 * about field contents, written once.
 *
 * ### What this header claims, and what it does not
 *
 * Every number the design page marks checked against the standard's text
 * is a constant here, or a member of dp_wlan_tables_t, which
 * dp_wlan_tables_standard() returns filled with the Clause 17 literals
 * transcribed from the text and each pinned in the test against its printed
 * form. dp_wlan_tables_empty() is the same struct with nothing in it: a
 * builder that needs an empty entry refuses (returns 0 or -1) and changes
 * nothing, and the machinery tests run on SYNTHETIC tables that claim no
 * conformance. The tables are:
 *
 *   - the SIGNAL field's RATE bits for each (constellation, code rate);
 *   - the L-STF and L-LTF subcarrier values (17.3.3) and each field's
 *     normalization;
 *   - the constellation bit-to-level labels, where they differ from the
 *     library's maps (Tables 17-15 to 17-18);
 *   - the second permutation of the interleaver (17.3.5.7).
 *
 * ### A PPDU
 *
 * L-STF (160 samples at 20 MHz), L-LTF (160), SIGNAL (one OFDM symbol of 80),
 * then `n_sym` Data OFDM symbols of 80. Durations scale with the clocking
 * (20, 10, 5 MHz) and the sample counts do not: this component counts
 * samples at one per `1/bandwidth`.
 *
 * @code
 * dp_wlan_rate_t r = { 4u, WLAN_CODE_3_4 };      // 16-QAM, rate 3/4
 * size_t n_sym = dp_wlan_n_sym (&r, 100u);       // 100-octet PSDU
 * @endcode
 */
#ifndef DP_WLAN_CORE_H
#define DP_WLAN_CORE_H

#include "doppler/clib_common.h"
#include "doppler/conv/conv_core.h"
#include "doppler/dp_crc.h"
#include "doppler/ofdm/ofdm_core.h"
#include <stddef.h>
#include <stdint.h>
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/pn/pn_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* ── The numbers the design page marks checked (section 9) ─────────────── */

/** @brief Transform length at 20 MHz (Table 17-5). */
#define WLAN_N_FFT 64u
/** @brief Data subcarriers per OFDM symbol (Table 17-5). */
#define WLAN_N_SD 48u
/** @brief Pilot subcarriers per OFDM symbol (Table 17-5). */
#define WLAN_N_SP 4u
/** @brief Guard interval, in samples (0.8 us at 20 MHz). */
#define WLAN_GI 16u
/** @brief An OFDM symbol with its guard interval, in samples. */
#define WLAN_SYM_LEN 80u
/** @brief The L-STF, in samples (8 us at 20 MHz). */
#define WLAN_STF_LEN 160u
/** @brief The L-LTF, in samples (8 us at 20 MHz). */
#define WLAN_LTF_LEN 160u
/** @brief Bits in the SIGNAL field (24: RATE, reserved, LENGTH, parity, tail). */
#define WLAN_SIG_BITS 24u
/** @brief Bits in the SERVICE field. */
#define WLAN_SERVICE_BITS 16u
/** @brief Tail bits that return the BCC encoder to zero. */
#define WLAN_TAIL_BITS 6u
/** @brief The largest PSDU, in octets (LENGTH is 12 bits; 17.3.4.3). */
#define WLAN_PSDU_MAX 4095u
/** @brief The pilot subcarrier indices (17.3.5.9). */
#define WLAN_PILOT_K { -21, -7, 7, 21 }
/** @brief The pilot values before the polarity element (17.3.5.10). */
#define WLAN_PILOT_V { 1.0, 1.0, 1.0, -1.0 }
/** @brief Length of the pilot polarity sequence. */
#define WLAN_POLARITY_LEN 127u

/**
 * @brief The code rate of the BCC, with puncturing (Figure 17-9).
 *
 * `WLAN_CODE_5_6` is Clause 19's and is unchecked against the text; Clause 17
 * has no RATE bits for it.
 */
typedef enum
{
  WLAN_CODE_1_2 = 0, /**< rate 1/2, no puncturing        */
  WLAN_CODE_2_3 = 1, /**< rate 2/3, one of four deleted  */
  WLAN_CODE_3_4 = 2, /**< rate 3/4, two of six deleted   */
  WLAN_CODE_5_6 = 3  /**< rate 5/6 (unchecked, Clause 19) */
} dp_wlan_code_t;

/**
 * @brief A non-HT rate, by the two columns of Table 17-4 that do not change
 *        with the clocking: a constellation and a code rate.
 *
 * `n_bpsc` is bits per subcarrier: 1 BPSK, 2 QPSK, 4 16-QAM, 6 64-QAM.
 */
typedef struct
{
  unsigned       n_bpsc; /**< 1, 2, 4 or 6 */
  dp_wlan_code_t code;   /**< the BCC rate */
} dp_wlan_rate_t;

/** @brief A (rate, SIGNAL RATE bits) pair of dp_wlan_tables_t. */
typedef struct
{
  dp_wlan_rate_t rate; /**< the rate this entry names              */
  uint8_t        bits; /**< the four RATE bits, as an integer 0..15 */
} dp_wlan_signal_rate_t;

/** @brief The second interleaver permutation: input index @p i to output. */
typedef size_t (*dp_wlan_perm2_fn) (size_t i, size_t n_cbps, size_t n_bpsc);

/**
 * @brief What the standard supplies and this tree does not yet hold.
 *
 * Empty (NULL, 0) until transcribed from the text; see the file comment.
 * Every pointer is borrowed and must outlive its use.
 */
typedef struct
{
  const float _Complex *stf; /**< 53 L-STF values, k = -26..26 (index k+26) */
  const float _Complex *ltf; /**< 53 L-LTF values, k = -26..26              */
  double scale_stf;  /**< L-STF time-domain scale; 0 = absent              */
  double scale_ltf;  /**< L-LTF scale; 0 = absent                          */
  double scale_data; /**< SIGNAL and Data scale; 0 = absent               */
  const dp_wlan_signal_rate_t *signal_rates; /**< RATE bits per rate      */
  size_t                       n_signal_rates; /**< entries above         */
  dp_wlan_perm2_fn perm2; /**< second interleaver permutation; NULL = absent */
  /** Per constellation (index n_bpsc, 1..6): bit-group value -> the
   *  library's symbol index, or NULL for the library's own labels. */
  const uint16_t *relabel[7];
} dp_wlan_tables_t;

/**
 * @brief The tables with nothing in them.
 * @return A pointer to a static, all-empty dp_wlan_tables_t.
 */
const dp_wlan_tables_t *dp_wlan_tables_empty (void);

/**
 * @brief The tables of IEEE Std 802.11-2024 Clause 17, transcribed from the
 *        text: the RATE bits (Table 17-6), the L-STF and L-LTF sequences
 *        (Eq. 17-6, 17-8), one scale for every field, the constellation
 *        bit encodings (Figure 17-10, Tables 17-14 to 17-18, as relabels of
 *        the library's maps) and the second interleaver permutation
 *        (Eq. 17-16, 17-17).
 *
 * The 5/6 code rate has no entry: it is Clause 19's, not Clause 17's, and
 * dp_wlan_signal_bits() refuses it.
 * @return A pointer to a static, filled dp_wlan_tables_t.
 */
const dp_wlan_tables_t *dp_wlan_tables_standard (void);

/* ── The object ──────────────────────────────────────────────────────────── */

/**
 * @brief A non-HT rate, held with what follows from it.
 *
 * Immutable after dp_wlan_create(): the rate is configuration, so there is no
 * running state and no state blob.
 */
typedef struct
{
  uint32_t n_bpsc; /**< bits per subcarrier: 1, 2, 4 or 6        */
  uint32_t code;   /**< the dp_wlan_code_t, as an integer         */
  uint32_t n_cbps; /**< coded bits per OFDM symbol, 48 * n_bpsc   */
  uint32_t n_dbps; /**< data bits per OFDM symbol                 */
} dp_wlan_state_t;

/**
 * @brief Create a rate holder, by the index of the constellation and of the
 *        code rate.
 * @param constellation  0 BPSK, 1 QPSK, 2 16-QAM, 3 64-QAM.
 * @param code_rate      0 for 1/2, 1 for 2/3, 2 for 3/4, 3 for 5/6.
 * @return Heap-allocated state, or NULL if either index is outside its range
 *         or the pair's data bits per OFDM symbol are not a whole number.
 * @note Caller must call dp_wlan_destroy() when done.
 *
 * @code
 * >>> from doppler.wlan import Wlan
 * >>> w = Wlan(constellation="qam16", code_rate="r34")  # 36 Mb/s at 20 MHz
 * >>> w.n_bpsc, w.n_cbps, w.n_dbps
 * (4, 192, 144)
 * >>> Wlan(constellation="qam256")
 * Traceback (most recent call last):
 *     ...
 * ValueError: constellation must be one of "bpsk", "qpsk", "qam16", "qam64", got 'qam256'
 *
 * @endcode
 */
dp_wlan_state_t *dp_wlan_create (int constellation, int code_rate);

/**
 * @brief Destroy a rate holder.
 * @param state  May be NULL.
 */
void dp_wlan_destroy (dp_wlan_state_t *state);

/**
 * @brief Reset a rate holder: nothing to reset, the rate is configuration.
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.wlan import Wlan
 * >>> w = Wlan(constellation="qpsk", code_rate="r34")
 * >>> w.reset()                      # nothing runs, so nothing changes
 * >>> w.n_dbps
 * 72
 *
 * @endcode
 */
void dp_wlan_reset (dp_wlan_state_t *state);

/* ── Rates ───────────────────────────────────────────────────────────────── */

/**
 * @brief Whether @p r is a rate this component builds: `n_bpsc` in
 *        {1, 2, 4, 6} and a data-bit count per OFDM symbol that is whole.
 * @param r  The rate.
 * @return 1 if it is, 0 if not.
 */
int dp_wlan_rate_ok (const dp_wlan_rate_t *r);

/**
 * @brief Coded bits per OFDM symbol, `48 * n_bpsc`.
 * @param r  A valid rate.
 * @return `N_CBPS`, or 0 for an invalid rate.
 */
unsigned dp_wlan_n_cbps (const dp_wlan_rate_t *r);

/**
 * @brief Data bits per OFDM symbol, `N_CBPS * code rate`.
 * @param r  A valid rate.
 * @return `N_DBPS`, or 0 for an invalid rate.
 */
unsigned dp_wlan_n_dbps (const dp_wlan_rate_t *r);

/**
 * @brief The Data OFDM symbols a PSDU needs:
 *        `ceil((16 + 8 * psdu_len + 6) / N_DBPS)`.
 * @param r         A valid rate.
 * @param psdu_len  Octets, 1 to WLAN_PSDU_MAX.
 * @return The symbol count, or 0 for an invalid rate or length.
 */
size_t dp_wlan_rate_n_sym (const dp_wlan_rate_t *r, size_t psdu_len);

/**
 * @brief The Data OFDM symbols a PSDU needs at the rate this object holds:
 *        `ceil((16 + 8 * psdu_len + 6) / n_dbps)`.
 *
 * @param state     A rate holder from dp_wlan_create().
 * @param psdu_len  Octets, 1 to WLAN_PSDU_MAX.
 * @return The symbol count, or 0 for a length of 0 or above WLAN_PSDU_MAX.
 *
 * @code
 * >>> from doppler.wlan import Wlan
 * >>> w = Wlan(constellation="qam16", code_rate="r34")
 * >>> w.n_sym(100)
 * 6
 * >>> w.n_sym(0)
 * 0
 *
 * @endcode
 */
size_t dp_wlan_n_sym (const dp_wlan_state_t *state, size_t psdu_len);

/**
 * @brief The samples a PPDU of @p n_sym Data symbols occupies:
 *        `160 + 160 + 80 + 80 * n_sym`, plus the @p window samples of the last
 *        symbol's window tail.
 * @param n_sym   Data OFDM symbols.
 * @param window  Window length in samples, 0 for none.
 * @return Samples.
 */
size_t dp_wlan_ppdu_len (size_t n_sym, unsigned window);

/**
 * @brief The BCC at a code rate: K = 7, generators 133 and 171 octal, with
 *        the puncture pattern of the rate.
 * @param code     The rate.
 * @param[out] c   The code. Must be non-NULL.
 * @return 1 on success, 0 for an unknown rate.
 */
int dp_wlan_bcc (dp_wlan_code_t code, conv_code_t *c);

/* ── The scrambler ───────────────────────────────────────────────────────── */

/**
 * @brief The register state that produces a scrambling sequence whose first
 *        seven bits, read as an integer with the first bit most significant,
 *        are @p seq7 (Annex I writes the example's start this way: 54).
 *
 * The sequence is the library's `pn` (a Galois register, polynomial `0x48`,
 * seven bits) and obeys `s[n] = s[n-7] xor s[n-4]`, so it is its own history:
 * the register `x1..x7` is `s[0], s[-1], ..., s[-6]` (an integer with `x1` most
 * significant) and the first output is `x7 xor x4`. The state that produces
 * given first seven bits is the recurrence run backward seven steps, and the
 * state 1011101 (93) produces the sequence 0110110 (54).
 *
 * @param seq7  The first seven sequence bits, 1..127.
 * @return The register state, 1..127; 0 for 0 or a value above 127.
 */
uint8_t dp_wlan_scrambler_state_from_sequence (uint8_t seq7);

/**
 * @brief The scrambling sequence from a register state.
 * @param state     The register state, 1..127 (`x1` the most significant bit).
 * @param n         Bits wanted.
 * @param[out] out  @p n unpacked bits.
 * @return @p n, or 0 for a state of 0 or above 127.
 */
size_t dp_wlan_scrambler_sequence (uint8_t state, size_t n, uint8_t *out);

/* ── The minimal MAC ─────────────────────────────────────────────────────── */

/** @brief What the caller supplies, and what the PSDU is (section 3). */
typedef enum
{
  WLAN_MAC_NONE = 0, /**< the whole PSDU, as given                    */
  WLAN_MAC_FCS  = 1, /**< an MPDU without its FCS: append the FCS     */
  WLAN_MAC_DATA = 2  /**< a header and a body: header, body, FCS      */
} dp_wlan_mac_t;

/**
 * @brief The PSDU length a MAC wrap produces.
 * @param mode        How the caller's octets are wrapped.
 * @param header_len  Header octets (WLAN_MAC_DATA only).
 * @param body_len    The caller's octets: the body in DATA mode, the whole
 *                    PSDU or MPDU otherwise.
 * @return Octets, or 0 if the result is 0 or above WLAN_PSDU_MAX.
 */
size_t dp_wlan_mpdu_len (dp_wlan_mac_t mode, size_t header_len,
                         size_t body_len);

/**
 * @brief Wrap the caller's octets as one MPDU.
 *
 * The FCS is CRC-32/ISO-HDLC over every octet before it, appended least
 * significant octet first. The check value of that CRC (`CBF43926` over the
 * nine ASCII digits) is pinned by the test, independent of 802.11.
 *
 * @param mode        The wrap.
 * @param header      Header octets (DATA mode; may be NULL otherwise).
 * @param header_len  Their count.
 * @param body        The caller's octets.
 * @param body_len    Their count.
 * @param[out] out    Receives dp_wlan_mpdu_len() octets.
 * @param out_cap     Capacity of @p out.
 * @return Octets written, or 0 (and @p out untouched) if the length is 0 or
 *         too large or @p out_cap is too small.
 */
size_t dp_wlan_mpdu (dp_wlan_mac_t mode, const uint8_t *header,
                     size_t header_len, const uint8_t *body, size_t body_len,
                     uint8_t *out, size_t out_cap);

/* ── The coding chain, stage by stage ────────────────────────────────────── */

/**
 * @brief The SIGNAL field's 24 bits: RATE, a reserved 0, LENGTH least
 *        significant bit first, an even-parity bit over the first 17, and six
 *        zero tail bits (Figure 17-5, 17.3.4.1).
 * @param t          The tables (RATE bits).
 * @param r          The rate.
 * @param length     PSDU octets, 1 to WLAN_PSDU_MAX.
 * @param[out] out   WLAN_SIG_BITS unpacked bits.
 * @return WLAN_SIG_BITS, or 0 if the rate has no RATE bits in the tables or
 *         the length is outside 1 to WLAN_PSDU_MAX.
 */
size_t dp_wlan_signal_bits (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                            size_t length, uint8_t *out);

/**
 * @brief The Data field's bits: SERVICE (16 zeros), the PSDU (each octet
 *        least significant bit first), six tail bits and pad bits to a
 *        whole number of symbols, all scrambled except the tail, which is
 *        reset to zero after scrambling.
 * @param r         The rate.
 * @param psdu      PSDU octets.
 * @param len       Their count, 1 to WLAN_PSDU_MAX.
 * @param state     The scrambler register state, 1..127.
 * @param[out] out  `n_sym * N_DBPS` unpacked bits.
 * @param cap       Capacity of @p out.
 * @return Bits written, or 0 for an invalid rate, length or state, or a
 *         capacity that is too small.
 */
size_t dp_wlan_data_bits (const dp_wlan_rate_t *r, const uint8_t *psdu,
                          size_t len, uint8_t state, uint8_t *out, size_t cap);

/**
 * @brief The BCC over a field's bits, punctured to the rate: the encoder
 *        starts at zero and runs the whole field.
 * @param code      The rate.
 * @param in        Unpacked bits.
 * @param n         Their count.
 * @param[out] out  The coded, punctured bits.
 * @param cap       Capacity of @p out.
 * @return Bits written, or 0 if @p code is unknown or @p cap is too small.
 */
size_t dp_wlan_encode (dp_wlan_code_t code, const uint8_t *in, size_t n,
                       uint8_t *out, size_t cap);

/**
 * @brief Interleave a coded field, one OFDM symbol of @p n_cbps at a time:
 *        the first permutation (`N_CBPS/16` rows by 16 columns), then the
 *        tables' second permutation.
 * @param t         The tables (second permutation).
 * @param n_bpsc    Bits per subcarrier.
 * @param in        Coded bits, a whole number of symbols.
 * @param n         Their count.
 * @param[out] out  @p n interleaved bits.
 * @return @p n, or 0 if the tables hold no second permutation, @p n is not a
 *         whole number of symbols, or @p n_bpsc is invalid.
 */
size_t dp_wlan_interleave (const dp_wlan_tables_t *t, unsigned n_bpsc,
                           const uint8_t *in, size_t n, uint8_t *out);

/**
 * @brief Map interleaved bits to constellation points: groups of @p n_bpsc
 *        bits, through the library's maps (or the tables' relabeling).
 * @param t         The tables (relabeling).
 * @param n_bpsc    1, 2, 4 or 6.
 * @param bits      Unpacked bits, a whole number of groups.
 * @param n         Their count.
 * @param[out] out  `n / n_bpsc` unit-average-power points.
 * @return Points written, or 0 for an invalid @p n_bpsc or a partial group.
 */
size_t dp_wlan_map (const dp_wlan_tables_t *t, unsigned n_bpsc,
                    const uint8_t *bits, size_t n, float _Complex *out);

/**
 * @brief The constellation points a PPDU's layout takes, in layout order:
 *        the SIGNAL field's 48, then the Data field's `48 * n_sym`.
 *
 * The SIGNAL field is BPSK at rate 1/2, not scrambled; the Data field runs
 * dp_wlan_data_bits(), dp_wlan_encode(), dp_wlan_interleave() and
 * dp_wlan_map().
 *
 * @param t         The tables.
 * @param r         The rate.
 * @param psdu      PSDU octets.
 * @param len       Their count.
 * @param state     The scrambler register state, 1..127.
 * @param[out] out  Receives `48 + 48 * n_sym` points.
 * @param cap       Capacity of @p out, in points.
 * @return Points written, or 0 if any stage refuses.
 */
size_t dp_wlan_points (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                       const uint8_t *psdu, size_t len, uint8_t state,
                       float _Complex *out, size_t cap);

/* ── The layout and the PPDU ─────────────────────────────────────────────── */

/**
 * @brief Build a PPDU's layout into an OFDM modulator: four classes (L-STF,
 *        L-LTF, SIGNAL, Data), the pilots as fixed entries with the pilot
 *        polarity, and four runs.
 *
 * @param o       A fresh modulator from dp_ofdm_create().
 * @param t       The tables (L-STF and L-LTF values and the scales).
 * @param n_sym   Data OFDM symbols.
 * @param window  Window length in samples at each symbol boundary, 0 for none.
 * @return 0 and a checked layout, or -1 if a table is empty, @p n_sym is 0,
 *         or the modulator refuses a call.
 * @note On -1 @p o may already hold some of the classes: it is not to be
 *       used again, only destroyed. dp_wlan_ppdu() builds into its own
 *       modulator and destroys it, so it is unaffected.
 */
int dp_wlan_layout (dp_ofdm_state_t *o, const dp_wlan_tables_t *t,
                    size_t n_sym, unsigned window);

/**
 * @brief Render a PPDU to samples.
 * @param t         The tables.
 * @param r         The rate.
 * @param psdu      PSDU octets.
 * @param len       Their count.
 * @param state     The scrambler register state, 1..127.
 * @param window    Window length in samples, 0 for none.
 * @param[out] out  Receives dp_wlan_ppdu_len() samples.
 * @param cap       Capacity of @p out, in samples.
 * @return Samples written, or 0 if any stage refuses (changing nothing).
 */
size_t dp_wlan_ppdu (const dp_wlan_tables_t *t, const dp_wlan_rate_t *r,
                     const uint8_t *psdu, size_t len, uint8_t state,
                     unsigned window, float _Complex *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* DP_WLAN_CORE_H */
