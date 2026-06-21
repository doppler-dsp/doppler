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

#ifdef __cplusplus
}
#endif

#endif /* WFM_DSP_H */
