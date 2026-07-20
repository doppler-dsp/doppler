/**
 * @file wfm_dsp.h
 * @brief DSSS spreading + root-raised-cosine pulse shaping (Phase B).
 *
 * Two pure DSP primitives the engine/composer use to build spread-spectrum and
 * band-limited waveforms:
 *   - wfm_dsss_spread:  multiply each data symbol by a PN chip code.
 *   - wfm_rrc_taps:     a unit-energy root-raised-cosine FIR (matched-filter
 *                       pulse shape), applied by upsample + FIR.
 */
#ifndef WFM_DSP_H
#define WFM_DSP_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of taps a `wfm_rrc_taps` call produces: `2*span*sps + 1`.
 * @param sps   samples per symbol (>= 1).
 * @param span  one-sided filter span in symbols (>= 1).
 */
static inline size_t
wfm_rrc_ntaps(int sps, int span)
{
    return (size_t)(2 * span * sps + 1);
}

/**
 * @brief Fill `taps` with a unit-energy root-raised-cosine impulse response.
 *
 * Length is `wfm_rrc_ntaps(sps, span)`; the response is symmetric about the
 * centre tap and normalised so `sum(taps^2) == 1` (so cascading TX·RX gives a
 * Nyquist raised cosine). The `t = 0` and `t = ±1/(4β)` singularities are
 * handled by their closed-form limits.
 *
 * @param beta  roll-off in `[0, 1]`.
 * @param sps   samples per symbol (>= 1).
 * @param span  one-sided span in symbols (>= 1).
 * @param taps  output array of length `wfm_rrc_ntaps(sps, span)`.
 */
void wfm_rrc_taps(double beta, int sps, int span, float *taps);

/**
 * @brief Spread `n_sym` complex data symbols by a binary PN code.
 *
 * `out[i*sf + j] = syms[i] * (code[j] ? -1 : +1)` — each symbol is repeated
 * across `sf` chips, sign-flipped per code chip. Output length is `n_sym*sf`.
 * Works for BPSK (real syms) and QPSK (complex syms).
 *
 * @param syms   complex data symbols; @param n_sym their count.
 * @param code   PN chip code (0/1), length `sf`; @param sf spreading factor.
 * @param out    output chips, length `n_sym * sf`.
 */
void wfm_dsss_spread(const float _Complex *syms, size_t n_sym,
                     const uint8_t *code, size_t sf, float _Complex *out);

/**
 * @brief Chip count of a DSSS burst frame (sizes `wfm_frame_dsss_chips`).
 *
 * `acq_len*acq_reps + (sync_len + payload_len + crc_bits) * data_len`, where
 * `crc_bits` is 16 when `crc` is set and there are payload bits, else 0 (a
 * CRC over nothing protects nothing). Returns 0 when the geometry is invalid:
 * frame bits present but no data code, or nothing to transmit at all.
 *
 * @param acq_len      preamble code length in chips (0 = no preamble).
 * @param acq_reps     preamble repetitions (0 = no preamble).
 * @param data_len     payload spreading-code length (chips per symbol).
 * @param sync_len     frame-sync word length in bits (0 = none).
 * @param payload_len  payload length in bits.
 * @param crc          non-zero: a CRC-16 trailer follows the payload.
 * @return Total burst chips, or 0 if the geometry is invalid/empty.
 */
size_t wfm_frame_dsss_nchips(size_t acq_len, size_t acq_reps, size_t data_len,
                             size_t sync_len, size_t payload_len, int crc);

/**
 * @brief Build a two-code DSSS burst as one flat 0/1 chip pattern.
 *
 * The transmit side of `burst_demod`'s frame contract, assembled in one
 * place so TX and RX can never drift:
 *
 *   `[ acq_code × acq_reps | (sync | payload | crc16(payload)) ⊕ data_code ]`
 *
 * The preamble is the *unmodulated* repeated acquisition code (no data on
 * it — a pure coherent-integration target). Every frame bit is then spread
 * by the (distinct) data code: chip `j` of frame bit `b` is `b ^ data_code[j]`.
 * The CRC-16-CCITT trailer (dp_crc16.h) is computed over the payload bits
 * only and spread MSB-first. Mapping chips to ±1 (BPSK) is the synth's job.
 *
 * @param acq_code     preamble code (0/1), length @p acq_len; NULL when
 *                     `acq_len*acq_reps == 0`.
 * @param acq_len      preamble code length in chips.
 * @param acq_reps     preamble repetitions.
 * @param data_code    payload spreading code (0/1), length @p data_len.
 * @param data_len     chips per frame symbol (the spreading factor).
 * @param sync         frame-sync word bits (0/1), length @p sync_len; NULL ok.
 * @param sync_len     sync word length in bits.
 * @param payload      payload bits (0/1), length @p payload_len; NULL ok.
 * @param payload_len  payload length in bits.
 * @param crc          non-zero: append the CRC-16 trailer after the payload.
 * @param out          output chip array (0/1) of `wfm_frame_dsss_nchips(...)`
 *                     elements.
 * @return Chips written, or 0 on invalid geometry (see
 *         `wfm_frame_dsss_nchips`).
 */
size_t wfm_frame_dsss_chips(const uint8_t *acq_code, size_t acq_len,
                            size_t acq_reps, const uint8_t *data_code,
                            size_t data_len, const uint8_t *sync,
                            size_t sync_len, const uint8_t *payload,
                            size_t payload_len, int crc, uint8_t *out);

/**
 * @brief Chip count for `wfm_cont_dsss_chips`: exactly @p n_chips.
 *
 * Trivial, but present so the two continuous entry points mirror the burst
 * pair (`wfm_frame_dsss_nchips` / `wfm_frame_dsss_chips`) and callers size
 * their buffer through a named function rather than an open-coded expression.
 */
static inline size_t
wfm_cont_dsss_nchips(size_t n_chips)
{
    return n_chips;
}

/**
 * @brief Build a CONTINUOUS, ASYNCHRONOUS DSSS chip pattern.
 *
 * The continuous counterpart to `wfm_frame_dsss_chips`. Two differences, both
 * required by a continuously-transmitting spread carrier (CCSDS command-link
 * style) rather than a bounded burst:
 *
 *  - **Continuous**: no preamble, no sync word, no CRC. The spreading code
 *    repeats end to end and data rides on it the whole way.
 *  - **Asynchronous**: the data-symbol clock is independent of the code epoch,
 *    so `chips_per_symbol` is a non-integer `double` and symbol boundaries
 *    land *inside* code epochs. The burst builder spreads exactly one bit per
 *    full code period — synchronous by construction, integer always.
 *
 * Chip `i` carries `code[i % code_len] ^ data[floor(i / chips_per_symbol)]`,
 * so both clocks advance independently off the same chip index. Because the
 * symbol index is a floor of a fractional quotient, consecutive symbols
 * legitimately span different numbers of chips (1136 or 1137 at SPEC.md's
 * 3.069 Mcps / 2700 bps) — that jitter IS the asynchronicity, not an artifact.
 *
 * Materialising the pattern up front, exactly as the burst builder does, is
 * what lets the synth's existing cyclic chip latch play it back unchanged: no
 * new per-sample branch, no new running state, no serialization change.
 *
 * @param code       spreading code chips (0/1), length @p code_len.
 * @param code_len   spreading code length in chips (> 0).
 * @param data       data bits (0/1), length @p n_data; cycled if exhausted.
 * @param n_data     data bit count (> 0).
 * @param chips_per_symbol  chips per data symbol (> 0, typically non-integer).
 * @param n_chips    chips to produce (the caller's requested span).
 * @param out        output chip array (0/1) of @p n_chips elements.
 * @return Chips written (== @p n_chips), or 0 on invalid geometry.
 */
size_t wfm_cont_dsss_chips(const uint8_t *code, size_t code_len,
                           const uint8_t *data, size_t n_data,
                           double chips_per_symbol, size_t n_chips,
                           uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DSP_H */
