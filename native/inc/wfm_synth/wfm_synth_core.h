/**
 * @file wfm_synth_core.h
 * @brief Synth component API.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
 *
 * Example:
 * @code
 * wfm_synth_state_t *obj = wfm_synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
 * float complex y = wfm_synth_step(obj);
 * wfm_synth_destroy(obj);
 * @endcode
 */
#ifndef WFM_SYNTH_CORE_H
#define WFM_SYNTH_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "fir/fir_core.h"
#include "lo/lo_core.h"
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#ifdef __cplusplus
extern "C" {
#endif

/** Waveform type discriminant (the `type` create argument / --type choice). */
enum {
    WFM_SYNTH_TONE = 0,  /* continuous-wave complex tone (LO)        */
    WFM_SYNTH_NOISE = 1, /* complex AWGN only                        */
    WFM_SYNTH_PN = 2,    /* BPSK-modulated PN m-sequence chips       */
    WFM_SYNTH_BPSK = 3,  /* BPSK over PN-sourced data bits           */
    WFM_SYNTH_QPSK = 4,  /* Gray-coded QPSK over PN-sourced data     */
    WFM_SYNTH_CHIRP = 5, /* linear-FM sweep f_start→f_end (no symbols) */
    WFM_SYNTH_BITS = 6,  /* user bit pattern, oversampled + cycled    */
};

/* snr >= this (dB) means "clean": no AWGN is generated at all (the common case
 * — a clean waveform shouldn't pay the noise cost). 100 dB SNR is the default
 * and is numerically clean anyway. Lower --snr to add noise. (type=noise always
 * generates AWGN regardless.) */
#define WFM_SYNTH_SNR_CLEAN 100.0

/**
 * @brief Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the
 * given register length n, in pn_core's right-shift Galois convention. Returns
 * 0 for lengths outside 2..64 (caller errors). Generated from verified
 * primitive polynomials (period 2^n-1); the n=2..16 values are unchanged.
 */
JM_FORCEINLINE uint64_t
wfm_synth_mls_poly(uint32_t n)
{
    switch (n) {
    case 2: return 0x3u;
    case 3: return 0x5u;
    case 4: return 0x9u;
    case 5: return 0x12u;
    case 6: return 0x21u;
    case 7: return 0x41u;
    case 8: return 0x8Eu;
    case 9: return 0x108u;
    case 10: return 0x204u;
    case 11: return 0x402u;
    case 12: return 0x829u;
    case 13: return 0x100Du;
    case 14: return 0x2015u;
    case 15: return 0x4001u;
    case 16: return 0x8016u;
    case 17: return 0x10004u;
    case 18: return 0x20013u;
    case 19: return 0x40013u;
    case 20: return 0x80004u;
    case 21: return 0x100002u;
    case 22: return 0x200001u;
    case 23: return 0x400010u;
    case 24: return 0x80000Du;
    case 25: return 0x1000004u;
    case 26: return 0x2000023u;
    case 27: return 0x4000013u;
    case 28: return 0x8000004u;
    case 29: return 0x10000002u;
    case 30: return 0x20000029u;
    case 31: return 0x40000004u;
    case 32: return 0x80000057u;
    case 33: return 0x100000029ull;
    case 34: return 0x200000073ull;
    case 35: return 0x400000002ull;
    case 36: return 0x80000003Bull;
    case 37: return 0x100000001Full;
    case 38: return 0x2000000031ull;
    case 39: return 0x4000000008ull;
    case 40: return 0x800000001Cull;
    case 41: return 0x10000000004ull;
    case 42: return 0x2000000001Full;
    case 43: return 0x4000000002Cull;
    case 44: return 0x80000000032ull;
    case 45: return 0x10000000000Dull;
    case 46: return 0x200000000097ull;
    case 47: return 0x400000000010ull;
    case 48: return 0x80000000005Bull;
    case 49: return 0x1000000000038ull;
    case 50: return 0x200000000000Eull;
    case 51: return 0x4000000000025ull;
    case 52: return 0x8000000000004ull;
    case 53: return 0x10000000000023ull;
    case 54: return 0x2000000000003Eull;
    case 55: return 0x40000000000023ull;
    case 56: return 0x8000000000004Aull;
    case 57: return 0x100000000000016ull;
    case 58: return 0x200000000000031ull;
    case 59: return 0x40000000000003Dull;
    case 60: return 0x800000000000001ull;
    case 61: return 0x1000000000000013ull;
    case 62: return 0x2000000000000034ull;
    case 63: return 0x4000000000000001ull;
    case 64: return 0x800000000000000Dull;
    default: return 0u;
    }
}

/**
 * @brief Synth state.
 *
 * Allocate with wfm_synth_create().
 */
typedef struct {
    int wtype;
    int nsps;
    int sym_pos;
    float cur_re;
    float cur_im;
    double chirp_f0;
    double chirp_fend;
    double chirp_k;
    double chirp_ph;
    size_t chirp_n;
    size_t chirp_span;
    uint8_t * bits;
    size_t n_bits;
    size_t bit_idx;
    int bit_mod;
    fir_state_t * fir;
    lo_state_t * lo;
    awgn_state_t * awgn;
    pn_state_t * pn;
} wfm_synth_state_t;

/**
 * @brief Allocate and configure a waveform synthesiser.
 * The synthesiser combines a local oscillator (LO), optional AWGN, and an
 * optional PN LFSR into a single streaming source.  One call to
 * wfm_synth_step() or wfm_synth_steps() advances all sub-components in lock-step.
 * SNR >= WFM_SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms
 * pay no noise overhead.  When ``snr_mode`` is "auto" the library picks the
 * natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR
 * for tone/noise/PN.
 *
 * @param type  Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk,
 *              5=chirp, 6=bits.  The Python binding accepts strings "tone"|
 *              "noise"|"pn"|"bpsk"|"qpsk"|"chirp"|"bits".  For "bits" attach
 *              the pattern with wfm_synth_set_bits() after create().
 * @param fs  Sample rate in Hz.  Sets the carrier frequency normalisation
 *              and the noise bandwidth.  Default 1 000 000.0.
 * @param freq  Carrier frequency offset in Hz (−fs/2 … fs/2).  A
 *              complex LO is created only when freq != 0.  For a chirp this
 *              is the start frequency f_start (the instantaneous frequency at
 *              t=0).  Default 0.0.
 * @param snr  Target SNR in dB, interpreted per ``snr_mode``.  Values >=
 *              WFM_SYNTH_SNR_CLEAN (100) disable AWGN.  Default 100.0.
 * @param snr_mode  SNR reference: 0=auto, 1=fs (full-band), 2=ebno,
 *              3=esno.  The Python binding accepts strings
 *              "auto"|"fs"|"ebno"|"esno".  Default 0.
 * @param seed  PRNG seed shared by AWGN and the PN LFSR.  Default 1.
 * @param sps  Samples per symbol for modulated types (BPSK, QPSK, PN).
 *              Ignored for tone/noise.  Default 8.
 * @param pn_length  LFSR register length (1..64); period = 2^pn_length - 1.
 *              Default 7 (period 127).
 * @param pn_poly  Galois tap polynomial for the LFSR.  0 means "look up
 *              the canonical MLS polynomial for pn_length" from the
 *              wfm_synth_mls_poly table.  Default 0.
 * @param lfsr  LFSR realization: PN_GALOIS (0) or PN_FIBONACCI (1).
 * @param f_end  Chirp end frequency in Hz (type=chirp only; ignored otherwise).
 *              With ``freq`` as the start, the instantaneous frequency sweeps
 *              linearly from ``freq`` to ``f_end`` over the span (set by
 *              wfm_synth_set_chirp_span() or the first wfm_synth_steps() call),
 *              then holds at ``f_end``.  ``f_end < freq`` is a down-chirp.
 *              Default 0.0.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call wfm_synth_destroy() when done.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.dtype
 * dtype('complex64')
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
wfm_synth_state_t *wfm_synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr, double f_end);

/**
 * @brief Pin a chirp's sweep span to @p span samples (no-op for non-chirp).
 *
 * A linear chirp's slope is `(f_end − f_start) / span`, so the span — the
 * number of samples the sweep occupies — must be known before generation. The
 * composer/CLI call this with the segment length; a standalone synth that is
 * never pinned locks its span to the first wfm_synth_steps() call instead.
 * Only the first pin (while the span is still 0) takes effect, so it is safe to
 * call unconditionally after wfm_synth_create().
 *
 * @param state  Must be non-NULL.
 * @param span   Sweep length in samples (> 0).
 */
void wfm_synth_set_chirp_span(wfm_synth_state_t *state, size_t span);

/**
 * @brief Attach a user bit pattern to a type=bits synth (no-op otherwise).
 *
 * Copies @p n bits (each 0/1) into the synth; @p modulation maps them to
 * symbols (0=none → 0/1 amplitude, 1=bpsk → ±1, 2=qpsk → Gray-coded ±1/√2,
 * two bits per symbol). The pattern is oversampled by the create-time `sps`
 * and **cycled** to fill whatever length `wfm_synth_steps()` requests, so one
 * pass is `n * sps` samples (`2*ceil... ` — `n/2 * sps` for qpsk). Replaces any
 * previous pattern; resets the read position. Safe to call repeatedly.
 *
 * @param state  Must be non-NULL.
 * @param bits   Array of @p n bytes, each 0 or 1.
 * @param n      Number of bits (> 0).
 * @param modulation  0=none, 1=bpsk, 2=qpsk.
 * @return 0 on success; -1 on bad args or allocation failure.
 */
int wfm_synth_set_bits(wfm_synth_state_t *state, const uint8_t *bits, size_t n,
                       int modulation);

/**
 * @brief Enable RRC pulse shaping on a modulated synth (pn/bpsk/qpsk).
 *
 * Replaces the default rectangular sample-and-hold with a root-raised-cosine
 * pulse: the symbol-rate impulse train is filtered by @p taps (a real FIR of
 * @p ntaps coefficients, typically `wfm_rrc_taps(beta, sps, span)`). The taps
 * are scaled by sqrt(sps) internally for unit transmit power, so every caller
 * passes the raw taps and gets byte-identical shaping. No-op for non-modulated
 * types. Replaces any existing shaper and clears its delay line.
 *
 * @param state  Must be non-NULL.
 * @param taps   Real FIR taps (copied).
 * @param ntaps  Number of taps (> 0).
 * @return 0 on success; -1 on bad args / allocation failure.
 */
int wfm_synth_set_rrc(wfm_synth_state_t *state, const float *taps,
                      size_t ntaps);

/**
 * @brief Destroy a synth instance and release all memory.
 * Recursively frees the LO, AWGN, and PN sub-objects, then the struct
 * itself.  Safe to call with NULL (no-op).
 *
 * @param state  Pointer to heap-allocated state; may be NULL.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.destroy()   # explicit teardown; no exception
 * @endcode
 */
void wfm_synth_destroy(wfm_synth_state_t *state);

/**
 * @brief Reset Synth to its post-create state.
 * Resets the LO phase accumulator, AWGN internal state, and PN LFSR
 * register to their initial values so the output sequence is perfectly
 * reproducible from sample 0.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="qpsk", sps=4, seed=1, snr=100.0)
 * >>> a = s.steps(16).copy()
 * >>> s.reset()
 * >>> np.array_equal(a, s.steps(16))
 * True
 * @endcode
 */
void wfm_synth_reset(wfm_synth_state_t *state);

/**
 * @brief Reseed only the additive-noise (AWGN) generator, leaving the signal
 * (LO / PN code / data / pulse shaping) untouched. A no-op for a synth with no
 * noise. Used by the composer to give each repeat a fresh noise realization
 * while the underlying waveform stays bit-identical.
 * @param state  Synth state (may be NULL).
 * @param seed   New noise RNG seed.
 */
void wfm_synth_reseed_noise(wfm_synth_state_t *state, uint32_t seed);

/**
 * @brief Generate one output sample from internal state.
 * Advances the PN LFSR (modulated types only, on symbol boundaries), the
 * LO phase accumulator, and the AWGN engine, then returns the mixed
 * result: ``sym * carrier + noise``.  Inlined and hot-path annotated so
 * tight per-sample loops pay no call overhead.
 *
 * @param state  Must be non-NULL.
 * @return Next output sample (float complex).
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.step()
 * (1+0j)
 * @endcode
 */
JM_FORCEINLINE JM_HOT float complex
wfm_synth_step(wfm_synth_state_t *state)
{
    float complex sym;
    if (state->wtype == WFM_SYNTH_BITS) {
        /* User bit pattern, oversampled sps and cycled to fill the request. The
         * symbol latch mirrors the PN path but sources bits from bits[bit_idx]
         * instead of the LFSR; bit_mod picks the mapping. */
        if (state->sym_pos == 0 && state->bits && state->n_bits) {
            if (state->bit_mod == 2) { /* qpsk: 2 bits/symbol, Gray-mapped */
                uint8_t b0 = state->bits[state->bit_idx];
                uint8_t b1 = state->bits[(state->bit_idx + 1) % state->n_bits];
                const float s = 0.70710678118654752f;
                state->cur_re = b0 ? -s : s;
                state->cur_im = b1 ? -s : s;
                state->bit_idx = (state->bit_idx + 2) % state->n_bits;
            } else if (state->bit_mod == 1) { /* bpsk: 0->+1, 1->-1 */
                state->cur_re = state->bits[state->bit_idx] ? -1.0f : 1.0f;
                state->cur_im = 0.0f;
                state->bit_idx = (state->bit_idx + 1) % state->n_bits;
            } else { /* none: unmodulated 0/1 amplitude */
                state->cur_re = state->bits[state->bit_idx] ? 1.0f : 0.0f;
                state->cur_im = 0.0f;
                state->bit_idx = (state->bit_idx + 1) % state->n_bits;
            }
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
        sym = state->cur_re + state->cur_im * I; /* bits: no pulse shaping */
    } else if (state->wtype >= WFM_SYNTH_PN && state->wtype <= WFM_SYNTH_QPSK) {
        if (state->sym_pos == 0) {
            if (state->wtype == WFM_SYNTH_QPSK) {
                uint8_t b0 = pn_step(state->pn);
                uint8_t b1 = pn_step(state->pn);
                const float s = 0.70710678118654752f;
                state->cur_re = b0 ? -s : s;
                state->cur_im = b1 ? -s : s;
            } else { /* pn or bpsk: +-1 */
                uint8_t b = pn_step(state->pn);
                state->cur_re = b ? -1.0f : 1.0f;
                state->cur_im = 0.0f;
            }
        }
        if (state->fir) {
            /* RRC pulse shaping: feed the symbol-rate impulse train (the held
             * symbol at a boundary, zero between) through the matched FIR. The
             * FIR carries its delay line across calls, so this is chunk-invariant
             * — step() and the block path agree bit-for-bit. */
            float complex imp = (state->sym_pos == 0)
                                    ? (state->cur_re + state->cur_im * I)
                                    : (0.0f + 0.0f * I);
            fir_execute(state->fir, &imp, 1, &sym);
        } else {
            sym = state->cur_re + state->cur_im * I; /* rect sample-and-hold */
        }
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
    } else {
        sym = state->cur_re + state->cur_im * I;
    }
    float complex carrier = 1.0f + 0.0f * I;
    if (state->lo) {
        lo_steps(state->lo, 1, &carrier);
    } else if (state->wtype == WFM_SYNTH_CHIRP) {
        /* Sweeping carrier: f(n) = f0 + k*n (normalised cycles/sample), held at
         * f_end once the span is reached. Phase accumulates in cycles, wrapped to
         * [0,1) each step so the double keeps precision over a long sweep. The
         * fused sym*carrier + noise below is the *same* expression the tone path
         * (and wfm_synth_steps) uses, so step()/steps() stay byte-identical. */
        double nf = (state->chirp_span && state->chirp_n >= state->chirp_span)
                        ? (double)state->chirp_span
                        : (double)state->chirp_n;
        double w   = state->chirp_f0 + state->chirp_k * nf;
        carrier    = cexpf((float)(6.283185307179586 * state->chirp_ph) * I);
        state->chirp_ph += w;
        state->chirp_ph -= floor(state->chirp_ph);
        state->chirp_n++;
    }
    float complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        awgn_generate(state->awgn, 1, &noise);
    return sym * carrier + noise;
}

/**
 * @brief Generate a block of output samples.
 * Calls wfm_synth_step() in a tight loop, writing each cf32 sample into
 * ``output``.  The Python binding returns a freshly allocated NumPy
 * complex64 array; ownership is transferred to the caller.
 *
 * @param state   Initialised Synth state returned by ``wfm_synth_create``.
 * @param output  Output buffer of at least ``n`` cf32 elements.
 * @param n       Number of samples to generate.
 * @code
 * >>> from doppler.wfm import _SynthEngine
 * >>> import numpy as np
 * >>> s = _SynthEngine(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.shape, x.dtype
 * ((4,), dtype('complex64'))
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
void wfm_synth_steps(
    wfm_synth_state_t *state,
    float complex          *output,
    size_t               n);

/**
 * @brief Return the active waveform type discriminant.
 * Maps to the WFM_SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk.
 * Use this to inspect which synthesis path is active at runtime.
 *
 * @param state  Must be non-NULL.
 * @return Integer waveform type index (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
int wfm_synth_get_wtype(const wfm_synth_state_t *state);

/**
 * @brief Override the waveform type discriminant in-place.
 * Changing wtype does not reinitialise sub-objects; use with care.
 *
 * @param state  Must be non-NULL.
 * @param val    New wtype value (WFM_SYNTH_TONE .. WFM_SYNTH_QPSK).
 */
void wfm_synth_set_wtype(wfm_synth_state_t *state, int val);

/**
 * @brief Return the samples-per-symbol count.
 * For modulated types (BPSK, QPSK, PN) each symbol is held for nsps
 * consecutive output samples.  For tone/noise this field is present but
 * unused by the synthesis path.
 *
 * @param state  Must be non-NULL.
 * @return Samples per symbol (nsps >= 1).
 */
int wfm_synth_get_nsps(const wfm_synth_state_t *state);

/**
 * @brief Override the samples-per-symbol count in-place.
 * Does not flush the symbol-position counter (sym_pos); set sym_pos=0
 * as well when changing sps mid-stream.
 *
 * @param state  Must be non-NULL.
 * @param val    New nsps value (>= 1).
 */
void wfm_synth_set_nsps(wfm_synth_state_t *state, int val);

/**
 * @brief Return the current position within the current symbol (0..nsps-1).
 * Reaches nsps and wraps to 0 each time a new symbol is consumed from the
 * PN LFSR.  Useful for frame alignment: sym_pos==0 on a step boundary
 * means the very next sample begins a fresh symbol.
 *
 * @param state  Must be non-NULL.
 * @return Symbol position counter (0 <= sym_pos < nsps).
 */
int wfm_synth_get_sym_pos(const wfm_synth_state_t *state);

/**
 * @brief Override the symbol-position counter in-place.
 * Injecting 0 forces the next wfm_synth_step() to latch a new PN chip; any
 * other value fast-forwards into the middle of the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New sym_pos value (0 <= val < nsps).
 */
void wfm_synth_set_sym_pos(wfm_synth_state_t *state, int val);

/**
 * @brief Return the real part of the current held symbol.
 * For modulated types this is the I component latched at the last symbol
 * boundary (±1 for BPSK/PN, ±1/√2 for QPSK).  For tone the synthesiser
 * initialises cur_re to 1.0 so that the held symbol is a clean unit-power
 * carrier; for noise it is 0.0 (noise has no held symbol).
 *
 * @param state  Must be non-NULL.
 * @return Current symbol real (I) component.
 */
float wfm_synth_get_cur_re(const wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol real (I) component in-place.
 * Takes effect on the next wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_re value.
 */
void wfm_synth_set_cur_re(wfm_synth_state_t *state, float val);

/**
 * @brief Return the imaginary part of the current held symbol.
 * For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0;
 * for tone/noise it is 0.
 *
 * @param state  Must be non-NULL.
 * @return Current symbol imaginary (Q) component.
 */
float wfm_synth_get_cur_im(const wfm_synth_state_t *state);

/**
 * @brief Override the held-symbol imaginary (Q) component in-place.
 * Takes effect on the next wfm_synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_im value.
 */
void wfm_synth_set_cur_im(wfm_synth_state_t *state, float val);



/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition of optional fir/lo/awgn/pn children (presence-flagged) +
 * running waveform-position scalars; bits/config restored by create. */
#define WFM_SYNTH_STATE_MAGIC DP_FOURCC ('W','F','M','S')
#define WFM_SYNTH_STATE_VERSION 1u
size_t wfm_synth_state_bytes (const wfm_synth_state_t *state);
void wfm_synth_get_state (const wfm_synth_state_t *state, void *blob);
int wfm_synth_set_state (wfm_synth_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* WFM_SYNTH_CORE_H */
