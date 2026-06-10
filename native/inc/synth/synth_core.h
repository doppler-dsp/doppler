/**
 * @file synth_core.h
 * @brief Synth component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * synth_state_t *obj = synth_create(0, 1000000.0, 0.0, 100.0, 0, 1, 8, 7, 0);
 * float complex y = synth_step(obj);
 * synth_destroy(obj);
 * @endcode
 */
#ifndef SYNTH_CORE_H
#define SYNTH_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "awgn/awgn_core.h"
#include "pn/pn_core.h"
#include <math.h> /* log10/powf/sqrtf in create_impl */
#ifdef __cplusplus
extern "C" {
#endif

/** Waveform type discriminant (the `type` create argument / --type choice). */
enum {
    SYNTH_TONE = 0,  /* continuous-wave complex tone (LO)        */
    SYNTH_NOISE = 1, /* complex AWGN only                        */
    SYNTH_PN = 2,    /* BPSK-modulated PN m-sequence chips       */
    SYNTH_BPSK = 3,  /* BPSK over PN-sourced data bits           */
    SYNTH_QPSK = 4,  /* Gray-coded QPSK over PN-sourced data     */
};

/* snr >= this (dB) means "clean": no AWGN is generated at all (the common case
 * — a clean waveform shouldn't pay the noise cost). 100 dB SNR is the default
 * and is numerically clean anyway. Lower --snr to add noise. (type=noise always
 * generates AWGN regardless.) */
#define SYNTH_SNR_CLEAN 100.0

/**
 * @brief Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the
 * given register length n, in pn_core's right-shift Galois convention. Returns
 * 0 for lengths outside 2..64 (caller errors). Generated from verified
 * primitive polynomials (period 2^n-1); the n=2..16 values are unchanged.
 */
JM_FORCEINLINE uint64_t
synth_mls_poly(uint32_t n)
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
 * Allocate with synth_create().
 */
typedef struct {
    int wtype;
    int nsps;
    int sym_pos;
    float cur_re;
    float cur_im;
    lo_state_t * lo;
    awgn_state_t * awgn;
    pn_state_t * pn;
} synth_state_t;

/**
 * @brief Allocate and configure a waveform synthesiser.
 * The synthesiser combines a local oscillator (LO), optional AWGN, and an
 * optional PN LFSR into a single streaming source.  One call to
 * synth_step() or synth_steps() advances all sub-components in lock-step.
 * SNR >= SYNTH_SNR_CLEAN (100 dB) skips AWGN entirely — clean waveforms
 * pay no noise overhead.  When ``snr_mode`` is "auto" the library picks the
 * natural reference: Es/No for modulated types (BPSK, QPSK), fs-band SNR
 * for tone/noise/PN.
 *
 * @param type  Waveform type: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk.
 *              The Python binding accepts strings "tone"|"noise"|"pn"|
 *              "bpsk"|"qpsk".
 * @param fs  Sample rate in Hz.  Sets the carrier frequency normalisation
 *              and the noise bandwidth.  Default 1 000 000.0.
 * @param freq  Carrier frequency offset in Hz (−fs/2 … fs/2).  A
 *              complex LO is created only when freq != 0.  Default 0.0.
 * @param snr  Target SNR in dB, interpreted per ``snr_mode``.  Values >=
 *              SYNTH_SNR_CLEAN (100) disable AWGN.  Default 100.0.
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
 *              synth_mls_poly table.  Default 0.
 * @param lfsr  LFSR realization: PN_GALOIS (0) or PN_FIBONACCI (1).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call synth_destroy() when done.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> import numpy as np
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.dtype
 * dtype('complex64')
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
synth_state_t *synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint64_t pn_poly, int lfsr);

/**
 * @brief Destroy a synth instance and release all memory.
 * Recursively frees the LO, AWGN, and PN sub-objects, then the struct
 * itself.  Safe to call with NULL (no-op).
 *
 * @param state  Pointer to heap-allocated state; may be NULL.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.destroy()   # explicit teardown; no exception
 * @endcode
 */
void synth_destroy(synth_state_t *state);

/**
 * @brief Reset Synth to its post-create state.
 * Resets the LO phase accumulator, AWGN internal state, and PN LFSR
 * register to their initial values so the output sequence is perfectly
 * reproducible from sample 0.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> import numpy as np
 * >>> s = Synth(type="qpsk", sps=4, seed=1, snr=100.0)
 * >>> a = s.steps(16).copy()
 * >>> s.reset()
 * >>> np.array_equal(a, s.steps(16))
 * True
 * @endcode
 */
void synth_reset(synth_state_t *state);

/* Forward declaration: synth_step() delegates to the block generator so the
 * two share a single implementation (see synth_step's note below). */
void synth_steps(synth_state_t *state, float complex *output, size_t n);

/**
 * @brief Generate one output sample from internal state.
 * Advances the PN LFSR (modulated types only, on symbol boundaries), the
 * LO phase accumulator, and the AWGN engine, then returns the mixed
 * result: ``sym * carrier + noise``.
 *
 * Implemented as ``synth_steps(state, &y, 1)`` rather than a separate scalar
 * recurrence.  A hand-rolled scalar form is *not* reliably byte-identical to
 * the block path under ``-ffast-math``: writing ``sym*carrier + noise`` as one
 * expression lets the compiler contract it into FMAs on targets with a fused
 * multiply-add (arm64), whereas synth_steps() rounds the multiply and the
 * noise-add separately — and the gap is unfixable per-function (Clang ignores
 * ``#pragma STDC FP_CONTRACT OFF`` and ``fp contract(off)`` under fast-math).
 * QPSK's irrational ±1/√2 leg made the two diverge by an ULP, which broke the
 * macOS composer/CLI byte-parity (#67).  One code path removes the class of
 * bug entirely; per-sample step() has no hot caller, so the block setup cost
 * is irrelevant.
 *
 * @param state  Must be non-NULL.
 * @return Next output sample (float complex).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.step()
 * (1+0j)
 * @endcode
 */
JM_FORCEINLINE JM_HOT float complex
synth_step(synth_state_t *state)
{
    float complex y;
    synth_steps(state, &y, 1);
    return y;
}

/**
 * @brief Generate a block of output samples.
 * Calls synth_step() in a tight loop, writing each cf32 sample into
 * ``output``.  The Python binding returns a freshly allocated NumPy
 * complex64 array; ownership is transferred to the caller.
 *
 * @param state   Initialised Synth state returned by ``synth_create``.
 * @param output  Output buffer of at least ``n`` cf32 elements.
 * @param n       Number of samples to generate.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> import numpy as np
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> x = s.steps(4)
 * >>> x.shape, x.dtype
 * ((4,), dtype('complex64'))
 * >>> x.tolist()
 * [(1+0j), (1+0j), (1+0j), (1+0j)]
 * @endcode
 */
void synth_steps(
    synth_state_t *state,
    float complex          *output,
    size_t               n);

/**
 * @brief Return the active waveform type discriminant.
 * Maps to the SYNTH_* enum: 0=tone, 1=noise, 2=pn, 3=bpsk, 4=qpsk.
 * Use this to inspect which synthesis path is active at runtime.
 *
 * @param state  Must be non-NULL.
 * @return Integer waveform type index (SYNTH_TONE .. SYNTH_QPSK).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.get_wtype()
 * 0
 * @endcode
 */
int synth_get_wtype(const synth_state_t *state);

/**
 * @brief Override the waveform type discriminant in-place.
 * Changing wtype does not reinitialise sub-objects; use with care.
 *
 * @param state  Must be non-NULL.
 * @param val    New wtype value (SYNTH_TONE .. SYNTH_QPSK).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.set_wtype(1)
 * >>> s.get_wtype()
 * 1
 * @endcode
 */
void synth_set_wtype(synth_state_t *state, int val);

/**
 * @brief Return the samples-per-symbol count.
 * For modulated types (BPSK, QPSK, PN) each symbol is held for nsps
 * consecutive output samples.  For tone/noise this field is present but
 * unused by the synthesis path.
 *
 * @param state  Must be non-NULL.
 * @return Samples per symbol (nsps >= 1).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
 * >>> s.get_nsps()
 * 4
 * @endcode
 */
int synth_get_nsps(const synth_state_t *state);

/**
 * @brief Override the samples-per-symbol count in-place.
 * Does not flush the symbol-position counter (sym_pos); set sym_pos=0
 * as well when changing sps mid-stream.
 *
 * @param state  Must be non-NULL.
 * @param val    New nsps value (>= 1).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
 * >>> s.set_nsps(8)
 * >>> s.get_nsps()
 * 8
 * @endcode
 */
void synth_set_nsps(synth_state_t *state, int val);

/**
 * @brief Return the current position within the current symbol (0..nsps-1).
 * Reaches nsps and wraps to 0 each time a new symbol is consumed from the
 * PN LFSR.  Useful for frame alignment: sym_pos==0 on a step boundary
 * means the very next sample begins a fresh symbol.
 *
 * @param state  Must be non-NULL.
 * @return Symbol position counter (0 <= sym_pos < nsps).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
 * >>> s.get_sym_pos()
 * 0
 * @endcode
 */
int synth_get_sym_pos(const synth_state_t *state);

/**
 * @brief Override the symbol-position counter in-place.
 * Injecting 0 forces the next synth_step() to latch a new PN chip; any
 * other value fast-forwards into the middle of the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New sym_pos value (0 <= val < nsps).
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="bpsk", sps=4, fs=1.0, snr=100.0)
 * >>> s.set_sym_pos(0)
 * >>> s.get_sym_pos()
 * 0
 * @endcode
 */
void synth_set_sym_pos(synth_state_t *state, int val);

/**
 * @brief Return the real part of the current held symbol.
 * For modulated types this is the I component latched at the last symbol
 * boundary (±1 for BPSK/PN, ±1/√2 for QPSK).  For tone the synthesiser
 * initialises cur_re to 1.0 so that the held symbol is a clean unit-power
 * carrier; for noise it is 0.0 (noise has no held symbol).
 *
 * @param state  Must be non-NULL.
 * @return Current symbol real (I) component.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.get_cur_re()  # tone initialises to 1.0
 * 1.0
 * @endcode
 */
float synth_get_cur_re(const synth_state_t *state);

/**
 * @brief Override the held-symbol real (I) component in-place.
 * Takes effect on the next synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_re value.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.set_cur_re(1.0)
 * >>> s.get_cur_re()
 * 1.0
 * @endcode
 */
void synth_set_cur_re(synth_state_t *state, float val);

/**
 * @brief Return the imaginary part of the current held symbol.
 * For QPSK this is the Q component (±1/√2); for BPSK/PN it is always 0;
 * for tone/noise it is 0.
 *
 * @param state  Must be non-NULL.
 * @return Current symbol imaginary (Q) component.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.get_cur_im()
 * 0.0
 * @endcode
 */
float synth_get_cur_im(const synth_state_t *state);

/**
 * @brief Override the held-symbol imaginary (Q) component in-place.
 * Takes effect on the next synth_step() within the current symbol hold.
 *
 * @param state  Must be non-NULL.
 * @param val    New cur_im value.
 * @code
 * >>> from doppler.wfmgen import Synth
 * >>> s = Synth(type="tone", fs=1.0, freq=0.0, snr=100.0)
 * >>> s.set_cur_im(0.5)
 * >>> s.get_cur_im()
 * 0.5
 * @endcode
 */
void synth_set_cur_im(synth_state_t *state, float val);



#ifdef __cplusplus
}
#endif

#endif /* SYNTH_CORE_H */
