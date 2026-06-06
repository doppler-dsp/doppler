/**
 * @file synth_core.h
 * @brief Synth component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * synth_state_t *obj = synth_create(0, 1000000.0, 0.0, 100.0, 1);
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
#include <math.h> /* powf/sqrtf in create_impl */
#ifdef __cplusplus
extern "C" {
#endif

/** Waveform type discriminant (the `type` create argument). */
enum {
    SYNTH_TONE = 0,  /* continuous-wave complex tone (LO) */
    SYNTH_NOISE = 1, /* complex AWGN only */
};

/**
 * @brief Synth state.
 *
 * Allocate with synth_create().
 */
typedef struct {
    lo_state_t * lo;
    awgn_state_t * awgn;
} synth_state_t;

/**
 * @brief Create a synth instance.
 *
 * @param type  type (default: 0).
 * @param fs  fs (default: 1000000.0).
 * @param freq_offset  freq_offset (default: 0.0).
 * @param snr_db  snr_db (default: 100.0).
 * @param seed  seed (default: 1).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call synth_destroy() when done.
 */
synth_state_t *synth_create(int type, double fs, double freq_offset, double snr_db, uint32_t seed);

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
    float complex sig = 0.0f + 0.0f * I;
    if (state->lo)
        lo_steps(state->lo, 1, &sig);
    float complex noise = 0.0f + 0.0f * I;
    if (state->awgn)
        awgn_generate(state->awgn, 1, &noise);
    return sig + noise;
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





#ifdef __cplusplus
}
#endif

#endif /* SYNTH_CORE_H */
