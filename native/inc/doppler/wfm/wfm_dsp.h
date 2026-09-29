/**
 * @file wfm_dsp.h
 * @brief DSSS spreading + root-raised-cosine pulse shaping (Phase B).
 *
 * Two pure DSP primitives the engine/composer use to build spread-spectrum and
 * band-limited waveforms:
 *   - dp_wfm_dsss_spread:  multiply each data symbol by a PN chip code.
 *   - dp_wfm_rrc_taps:     a unit-energy root-raised-cosine FIR (matched-filter
 *                       pulse shape), applied by upsample + FIR.
 */
#ifndef WFM_DSP_H
#define WFM_DSP_H

#include "doppler/clib_common.h"
#include "doppler/util/util_core.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Number of taps a `dp_wfm_rrc_taps` call produces: `2*span*sps + 1`.
 * @param sps   samples per symbol (>= 1).
 * @param span  one-sided filter span in symbols (>= 1).
 */
static inline size_t
wfm_rrc_ntaps(int sps, int span)
{
    return (size_t)(2 * span * sps + 1);
}

/**
 * @brief Analytic root-raised-cosine impulse response at one instant.
 *
 * The RRC formula itself, evaluated at an arbitrary continuous time — the
 * single source of truth every RRC consumer samples. `dp_wfm_rrc_taps()` walks
 * this on the uniform `1/sps` grid and normalises; a receiver's polyphase
 * matched-filter bank (RateConverter's pulse-shaped terminal stage) samples it at
 * `num_phases * num_taps` instants that are NOT a uniform sub-multiple of the
 * input grid, which is why the point evaluator is public: an arbitrary
 * (non-integer) samples-per-symbol bank cannot be built by decomposing an
 * integer-oversampled prototype, and a second copy of this formula is exactly
 * the kind of peer implementation that drifts.
 *
 * Both removable singularities are handled by their closed-form limits: the
 * `0/0` at `t = 0`, and the `0/0` at `t = ±1/(4β)` where the denominator's
 * `1 - (4βt)^2` vanishes.
 *
 * @param t     time in SYMBOL periods (T = 1), relative to the pulse centre.
 * @param beta  roll-off in `[0, 1]`.
 * @return      `h(t)`, unnormalised (peak ≈ `1 - β + 4β/π` at `t = 0`).
 */
static inline double
wfm_rrc_h(double t, double beta)
{
    if (fabs(t) < 1e-9)
        /* limit at t = 0 */
        return 1.0 - beta + 4.0 * beta / M_PI;
    if (beta > 0.0 && fabs(fabs(t) - 1.0 / (4.0 * beta)) < 1e-9)
    {
        /* limit at t = ±1/(4β) (0/0 in the general form) */
        double a = M_PI / (4.0 * beta);
        return (beta / sqrt(2.0))
               * ((1.0 + 2.0 / M_PI) * sin(a) + (1.0 - 2.0 / M_PI) * cos(a));
    }
    double pt  = M_PI * t;
    double num = sin(pt * (1.0 - beta)) + 4.0 * beta * t * cos(pt * (1.0 + beta));
    double den = pt * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));
    return num / den;
}

/**
 * @brief The MATCHED pair's composite pulse: `rrc * rrc`, in closed form.
 *
 * A root-raised cosine convolved with itself is a raised cosine, so the pulse
 * a matched receiver actually sees needs no convolution and no table — which
 * is what lets a constructor evaluate it. Normalised to `g(0) = 1`, the level
 * a unity-gain matched cascade delivers (see dp_RateConverter_gain()), so `g(t)`
 * IS the recovered symbol amplitude at timing offset `t`.
 *
 * Nyquist by construction: `g(k) = 0` at every non-zero integer `k`, which is
 * why a timing error and not an amplitude error is what inter-symbol
 * interference looks like here.
 *
 * The removable singularity at `t = ±1/(2β)` is handled by its closed-form
 * limit; `t = 0` needs none (the sinc limit is taken explicitly).
 *
 * @param t     time in SYMBOL periods (T = 1), relative to the pulse centre.
 * @param beta  roll-off in `[0, 1]`.
 * @return      `g(t)`, with `g(0) = 1`.
 *
 * @code
 * printf ("%.4f %.6f\n", wfm_rc_h (0.0, 0.35), wfm_rc_h (1.0, 0.35));
 * // 1.0000 0.000000
 * @endcode
 */
static inline double
wfm_rc_h(double t, double beta)
{
    if (fabs(t) < 1e-9)
        return 1.0;
    if (beta > 0.0 && fabs(fabs(t) - 1.0 / (2.0 * beta)) < 1e-9)
    {
        /* limit at t = ±1/(2β): cos(πβt) → -πβε and the denominator → -4βε,
           so their ratio → π/4 and g → sinc(1/(2β)) * π/4. */
        return (beta / 2.0) * sin(M_PI / (2.0 * beta));
    }
    double den = 1.0 - (2.0 * beta * t) * (2.0 * beta * t);
    return dp_sinc(t) * cos(M_PI * beta * t) / den;
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
void dp_wfm_rrc_taps(double beta, int sps, int span, float *taps);

/**
 * @brief Number of taps per phase in a `dp_wfm_rrc_polyphase_bank`: `2*span + 1`.
 * @param span  one-sided filter span in symbols (>= 1).
 */
static inline size_t
wfm_rrc_bank_ntaps(int span)
{
    return (size_t)(2 * span + 1);
}

/**
 * @brief Deal an arbitrary FIR prototype into a polyphase interpolation bank.
 *
 * The pure decomposition shared by every polyphase-bank builder: phase `p`
 * gets the prototype taps that land on output samples of residue `p`, so
 * `bank[p*num_taps + t] = proto[t*num_phases + p]` (zero-padded past
 * `proto_len`). Row-major, `num_phases * num_taps` floats — exactly the layout
 * `dp_resamp_create_custom(num_phases, num_taps, bank, rate)` consumes. Interpolate
 * an input stream by `num_phases` (rate = num_phases) with the resulting bank
 * and you recompute the dense `proto` convolution from only the nonzero
 * upsampled contributions.
 *
 * @param proto       prototype FIR taps.
 * @param proto_len   number of prototype taps.
 * @param num_phases  interpolation factor (bank rows).
 * @param num_taps    taps per phase; must satisfy
 *                    `num_phases * num_taps >= proto_len`
 *                    (use `(proto_len + num_phases - 1) / num_phases`).
 * @param bank        output bank, row-major, length `num_phases * num_taps`.
 */
void dp_wfm_polyphase_bank(const float *proto, size_t proto_len,
                        size_t num_phases, size_t num_taps, float *bank);

/**
 * @brief Decompose the RRC pulse shape into a polyphase interpolation bank.
 *
 * The dense pulse shaper upsamples a symbol stream by `sps` (one impulse per
 * `sps` samples, the rest hard zeros) then runs the full `dp_wfm_rrc_taps` FIR
 * over it — `(sps-1)/sps` of every tap-multiply hits a structural zero. The
 * *polyphase* form computes the identical convolution from only the nonzero
 * contributions: it splits the length-`wfm_rrc_ntaps(sps, span)` prototype into
 * `sps` phases of `wfm_rrc_bank_ntaps(span)` taps each, so phase `p` selects the
 * subset of prototype taps that land on output samples of residue `p`.
 *
 * The prototype is `dp_wfm_rrc_taps(beta, sps, span)` scaled by `sqrt(sps)` — the
 * same unit-average-power scaling `dp_wfm_synth_set_rrc` applies to the dense taps,
 * folded in here so the two paths shape at byte-comparable amplitude. The
 * row-major layout `bank[p*num_taps + t] = proto[t*sps + p]` (zero-padded past
 * the final partial tap) is exactly the decomposition `resamp`'s own Kaiser
 * bank uses, so the bank drops straight into `dp_resamp_create_custom(sps,
 * wfm_rrc_bank_ntaps(span), bank, sps)` as an interpolate-by-`sps` shaper.
 *
 * Unlike `resamp`'s Kaiser prototype (which carries a `×num_phases` gain to
 * compensate interpolation energy spreading), the RRC prototype carries no such
 * gain: the interpolate path reproduces the dense FIR output to float precision
 * with the raw scaled taps.
 *
 * @param beta  roll-off in `[0, 1]`.
 * @param sps   samples per symbol (>= 1); also the number of phases.
 * @param span  one-sided span in symbols (>= 1).
 * @param bank  output bank, row-major, length `sps * wfm_rrc_bank_ntaps(span)`.
 */
void dp_wfm_rrc_polyphase_bank(double beta, int sps, int span, float *bank);

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
void dp_wfm_dsss_spread(const float _Complex *syms, size_t n_sym,
                     const uint8_t *code, size_t sf, float _Complex *out);

/**
 * @brief Chip count for `dp_wfm_cont_dsss_chips`: exactly @p n_chips.
 *
 * Trivial, but present so the two continuous entry points mirror the burst
 * pair (`dp_wfm_dsss_desc_nchips` / `dp_wfm_dsss_desc_chips`, in
 * `wfm/wfm_frame.h`) and callers size
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
 * The continuous counterpart to `dp_wfm_dsss_desc_chips`. Two differences, both
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
size_t dp_wfm_cont_dsss_chips(const uint8_t *code, size_t code_len,
                           const uint8_t *data, size_t n_data,
                           double chips_per_symbol, size_t n_chips,
                           uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DSP_H */
