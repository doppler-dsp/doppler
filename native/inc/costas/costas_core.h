/**
 * @file costas_core.h
 * @brief Costas carrier-tracking loop (integer-NCO de-rotation + PI loop).
 *
 * A continuous BPSK carrier-recovery loop: per sample it de-rotates the input
 * with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it
 * dumps the coherent integrate-and-dump accumulator, runs a decision-directed
 * Costas phase discriminator, filters the error through an embedded 2nd-order
 * @ref loop_filter_state_t, and steers the NCO frequency + phase.  It tracks a
 * small *residual* carrier offset (the bulk Doppler is removed upstream by FFT
 * acquisition); the steering NCO is `lo`, so the phase is bounded and exactly
 * reproducible (no double-accumulator drift).
 *
 * The block API (costas_steps) is the Python face; the JM_FORCEINLINE
 * costas_wipeoff()/costas_update() are the C composition API a despreader /
 * tracking channel inlines into its own sample loop.
 *
 * Lifecycle: costas_create -> [steps / configure / reset]* -> costas_destroy,
 * or embed by value with costas_init().
 *
 * @code
 * costas_state_t *c = costas_create(0.05, 0.707, 0.01, 64);
 * float complex sym[16];
 * size_t k = costas_steps(c, rx, rx_len, sym, 16);  // one prompt per symbol
 * double f = c->nco.norm_freq;                       // tracked residual
 * costas_destroy(c);
 * @endcode
 */
#ifndef COSTAS_CORE_H
#define COSTAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define COSTAS_EPS 1e-12f
/* EMA smoothing for the |Re P|/|P| lock metric (status diagnostic). */
#define COSTAS_LOCK_ALPHA 0.1

/**
 * @brief Costas loop state.
 *
 * Allocate with costas_create(), or embed by value and costas_init().
 * The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so the
 * inline composition helpers can drive them; treat the integrate-and-dump and
 * diagnostic fields as internal.
 */
typedef struct {
    lo_state_t nco;          /**< integer carrier NCO (uint32 phase).      */
    loop_filter_state_t lf;  /**< 2nd-order carrier PI loop.               */
    size_t tsamps;           /**< samples per symbol (integrate-and-dump). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< loop noise bandwidth (retained).         */
    double zeta;             /**< damping factor (retained).               */
    float complex acc;       /**< running coherent I&D accumulator.        */
    size_t acc_n;            /**< samples accumulated into `acc`.          */
    double lock_metric;      /**< EMA of |Re P|/|P| (1 = locked).          */
    double last_error;       /**< last discriminator output (loop stress). */
} costas_state_t;

/**
 * @brief Initialise a Costas loop in place (no allocation).
 *
 * The by-value counterpart to costas_create(): a tracking channel that embeds
 * a costas_state_t initialises it here.  Seeds the NCO at @p init_norm_freq and
 * the loop integrator to the matching per-symbol frequency so de-rotation is
 * correct from the first sample.
 *
 * @param s               State to initialise.  Must be non-NULL.
 * @param bn              Loop noise bandwidth, normalised to the symbol rate.
 * @param zeta            Damping factor (0.707 = critically damped).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param tsamps          Samples per symbol (the integrate-and-dump period).
 */
void costas_init(costas_state_t *s, double bn, double zeta,
                 double init_norm_freq, size_t tsamps);

/**
 * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
 *
 * `x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the
 * matched-filter integrate-and-dump.  Inline, zero call overhead.
 *
 * @param s  Costas state.  Must be non-NULL.
 * @param x  One input sample.
 * @return The de-rotated sample to feed the integrator.
 */
JM_FORCEINLINE JM_HOT float complex
costas_wipeoff(costas_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

/**
 * @brief Per-symbol carrier update: discriminator -> loop filter -> steer NCO.
 *
 * Runs the decision-directed BPSK Costas discriminator on the prompt @p P,
 * filters it, and writes the new frequency (lo_set_norm_freq) plus a
 * proportional phase nudge into the NCO.  Updates the lock metric and
 * last_error (the instantaneous loop stress).  Inline for composition.
 *
 * @param s  Costas state.  Must be non-NULL.
 * @param P  The dumped integrate-and-dump prompt for this symbol.
 */
JM_FORCEINLINE JM_HOT void
costas_update(costas_state_t *s, float complex P)
{
    float reP = crealf(P), imP = cimagf(P);
    float aP = cabsf(P) + COSTAS_EPS;
    double e = (double)(((reP >= 0.0f) ? imP : -imP) / aP);
    s->last_error = e;
    loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> uint32 phase delta */
    s->nco.phase += (uint32_t)((s->lf.kp * e) / (2.0 * M_PI) * 4294967296.0);
    /* lock metric: |Re|/|P| EMA (1 = phase-locked BPSK, ~0 = no carrier) */
    double inst = (double)(fabsf(reP) / aP);
    s->lock_metric += COSTAS_LOCK_ALPHA * (inst - s->lock_metric);
}

/**
 * @brief Create a Costas instance.
 *
 * @param bn              Loop noise bandwidth (default 0.05).
 * @param zeta            Damping factor (default 0.707).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample (default 0.0).
 * @param tsamps          Samples per symbol (default 64).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call costas_destroy() when done.
 */
costas_state_t *costas_create(double bn, double zeta, double init_norm_freq, size_t tsamps);

/**
 * @brief Destroy a Costas instance and release all memory.
 * @param state  May be NULL.
 */
void costas_destroy(costas_state_t *state);

/**
 * @brief Re-seed the loop to its create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void costas_reset(costas_state_t *state);

size_t costas_steps_max_out(costas_state_t *state);
size_t costas_steps(costas_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void costas_configure(costas_state_t *state, double bn, double zeta);
double costas_get_bn(const costas_state_t *state);
void costas_set_bn(costas_state_t *state, double val);
double costas_get_norm_freq(const costas_state_t *state);
void costas_set_norm_freq(costas_state_t *state, double val);
double costas_get_lock_metric(const costas_state_t *state);
double costas_get_last_error(const costas_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* COSTAS_CORE_H */
