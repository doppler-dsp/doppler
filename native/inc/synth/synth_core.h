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

/**
 * @brief Maximal-length-sequence (MLS) primitive polynomial for an LFSR of the
 * given register length, in pn_core's Galois convention. Returns 0 for lengths
 * with no table entry (caller errors). Verified period 2^n-1 for n=2..16.
 */
JM_FORCEINLINE uint32_t
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
 * @brief Create a synth instance.
 *
 * @param type  Enum index; 0=tone…4=qpsk.
 * @param fs  fs (default: 1000000.0).
 * @param freq  freq (default: 0.0).
 * @param snr  snr (default: 100.0).
 * @param snr_mode  Enum index; 0=auto…3=esno.
 * @param seed  seed (default: 1).
 * @param sps  sps (default: 8).
 * @param pn_length  pn_length (default: 7).
 * @param pn_poly  pn_poly (default: 0).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call synth_destroy() when done.
 */
synth_state_t *synth_create(int type, double fs, double freq, double snr, int snr_mode, uint32_t seed, int sps, int pn_length, uint32_t pn_poly);

/**
 * @brief Destroy a synth instance and release all memory.
 * @param state  May be NULL.
 */
void synth_destroy(synth_state_t *state);

/**
 * @brief Reset Synth to its post-create state.
 * @param state  Must be non-NULL.
 */
void synth_reset(synth_state_t *state);

/**
 * @brief Generate one output sample from internal state.
 * @param state  Must be non-NULL.
 * @return Next output sample (float complex).
 */
JM_FORCEINLINE JM_HOT float complex
synth_step(synth_state_t *state)
{
    if (state->wtype >= SYNTH_PN) {
        if (state->sym_pos == 0) {
            if (state->wtype == SYNTH_QPSK) {
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
        if (++state->sym_pos >= state->nsps)
            state->sym_pos = 0;
    }
    float complex sym = state->cur_re + state->cur_im * I;
    float complex carrier = 1.0f + 0.0f * I;
    if (state->lo)
        lo_steps(state->lo, 1, &carrier);
    float complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        awgn_generate(state->awgn, 1, &noise);
    return sym * carrier + noise;
}

/**
 * @brief Generate a block of output samples.
 *
 * @param state   Component state (mutated).
 * @param output  Output array (length >= n).
 * @param n       Number of samples to generate.
 */
void synth_steps(
    synth_state_t *state,
    float complex          *output,
    size_t               n);

/**
 * @brief Get current wtype.
 * @param state  Must be non-NULL.
 */
int synth_get_wtype(const synth_state_t *state);

/**
 * @brief Set wtype.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void synth_set_wtype(synth_state_t *state, int val);

/**
 * @brief Get current nsps.
 * @param state  Must be non-NULL.
 */
int synth_get_nsps(const synth_state_t *state);

/**
 * @brief Set nsps.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void synth_set_nsps(synth_state_t *state, int val);

/**
 * @brief Get current sym_pos.
 * @param state  Must be non-NULL.
 */
int synth_get_sym_pos(const synth_state_t *state);

/**
 * @brief Set sym_pos.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void synth_set_sym_pos(synth_state_t *state, int val);

/**
 * @brief Get current cur_re.
 * @param state  Must be non-NULL.
 */
float synth_get_cur_re(const synth_state_t *state);

/**
 * @brief Set cur_re.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void synth_set_cur_re(synth_state_t *state, float val);

/**
 * @brief Get current cur_im.
 * @param state  Must be non-NULL.
 */
float synth_get_cur_im(const synth_state_t *state);

/**
 * @brief Set cur_im.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void synth_set_cur_im(synth_state_t *state, float val);



#ifdef __cplusplus
}
#endif

#endif /* SYNTH_CORE_H */
