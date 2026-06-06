#include "synth/synth_core.h"
#include <math.h>

/*
 * Single-waveform synthesizer (MVP: tone, noise). A tone is a unit-power
 * complex CW from the LO at the given normalised freq_offset; AWGN is added at
 * the target snr_db (signal power = 1). A noise waveform is unit-power AWGN.
 * Per-sample generation via synth_step (block-wrapped by synth_steps).
 */

synth_state_t *
synth_create(int type, double fs, double freq_offset, double snr_db,
             uint32_t seed)
{
    (void)fs; /* fs is carried by the spec/sink, not needed for generation */
    synth_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->type = type;
    if (type == SYNTH_TONE) {
        obj->lo = lo_create(freq_offset);
        if (!obj->lo) {
            free(obj);
            return NULL;
        }
        /* additive noise at the requested SNR (tone power = 1); awgn complex
         * power = 2*amplitude², so amplitude = sqrt(P_sig / (2*SNR_lin)). */
        float snr_lin = powf(10.0f, (float)snr_db / 10.0f);
        float amp = sqrtf(1.0f / (2.0f * snr_lin));
        obj->awgn = awgn_create((uint64_t)seed, amp);
        if (!obj->awgn) {
            lo_destroy(obj->lo);
            free(obj);
            return NULL;
        }
    } else { /* SYNTH_NOISE — unit total power (2*amplitude² = 1) */
        obj->awgn = awgn_create((uint64_t)seed,
                                (float)(1.0 / 1.4142135623730951));
        if (!obj->awgn) {
            free(obj);
            return NULL;
        }
    }
    return obj;
}

void
synth_destroy(synth_state_t *state)
{
    if (state) {
        if (state->lo)
            lo_destroy(state->lo);
        if (state->awgn)
            awgn_destroy(state->awgn);
        free(state);
    }
}

void
synth_reset(synth_state_t *state)
{
    if (state->lo)
        lo_reset(state->lo);
    if (state->awgn)
        awgn_reset(state->awgn);
}

void synth_steps(synth_state_t *state, float complex *output, size_t n)
{
    for (size_t i = 0; i < n; i++)
        output[i] = synth_step(state);
}
