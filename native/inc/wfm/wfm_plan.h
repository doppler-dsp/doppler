/*
 * wfm_plan.h — "prepare once, materialize many" stimulus engine.
 *
 * A composed multi-source scene is a linear form
 *
 *     out = Σ_k gain_k · signal_k  +  noise
 *
 * (wfm_resolve_noise() cleans every signal source and appends one
 * WFM_SYNTH_NOISE source at the shared floor). The expensive DSP — LFSR
 * spread, RRC convolution, transcendental LO — lives entirely in the signal
 * terms, which are INVARIANT across a parameter sweep. So a Plan renders each
 * source's contribution ONCE (via the composer's own wfm_compose_build_synth,
 * so a cached render is byte-identical to a full compose), caches it, and then
 * re-materializes any variation as a cheap re-weighted sum + a regenerated
 * noise floor:
 *
 *     render(θ) = Σ_k g_k(θ)·e^{jφ_k(θ)}·cache_k  +  gain(snr(θ))·noise(seed(θ))
 *
 * v1 axes (all bit-exact vs a full compose): per-source gain/level, phase,
 * enable/disable, global SNR/noise-floor and Monte-Carlo noise-seed. Frequency
 * (Doppler) and timing (multipath) are staged follow-ups on the same frame.
 *
 * Scope (v1): a single finite, non-ranged ON segment (num_samples, no gap),
 * with at most one noise source (the resolve-appended floor, which must be
 * last). A lone *bundled* noisy source (one source carrying snr) is NOT
 * separable — its private RNG stays entangled — and is rejected.
 *
 * The Plan is a re-creatable derived cache, not evolving state: persist the
 * spec JSON (Composer.to_json()) and rebuild, rather than serializing the
 * multi-MB sample buffers.
 */
#ifndef WFM_PLAN_H
#define WFM_PLAN_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque prepared-plan state. */
typedef struct wfm_plan wfm_plan_t;

/**
 * @brief Prepare a Plan from a composer spec JSON (Composer.to_json()).
 *
 * Parses + resolves the scene, validates the v1 scope, then renders and caches
 * each signal source at gain 1. Returns NULL on parse failure or an
 * out-of-scope spec (multi-segment, continuous/repeat, ranged, a non-trailing
 * or multiple noise source, or a lone bundled noisy source).
 *
 * @param spec_json A NUL-terminated composer spec JSON string.
 * @return Heap Plan (caller wfm_plan_destroy()s it), or NULL.
 */
wfm_plan_t *wfm_plan_prepare(const char *spec_json);

/** @brief Cached length in samples (the jm binding's out_len_fn). */
size_t wfm_plan_len(const wfm_plan_t *p);

/** @brief Number of cached signal sources (excludes the noise floor). */
size_t wfm_plan_n_sources(const wfm_plan_t *p);

/**
 * @brief The noise seed that reproduces a full compose.
 *
 * Passing this as `wfm_plan_at`'s seed (with the scene's base SNR) yields the
 * byte-identical output of `wfm_compose`. Varying the seed draws independent
 * Monte-Carlo noise realizations over the same signal.
 */
uint64_t wfm_plan_anchor_seed(const wfm_plan_t *p);

/**
 * @brief General render: apply a JSON override spec, return a cf32 array.
 *
 * `overrides_json` is a small JSON object, all keys optional:
 * `{"gains":[dB…], "phases":[rad…], "enable":[bool…], "snr":dB, "seed":u}`
 * (`gains`/`phases`/`enable` are per-source, length = wfm_plan_n_sources()).
 * An empty object (or NULL) renders the baseline — bit-identical to
 * `Composer(scene).compose()`. Writes `wfm_plan_len(p)` samples to `out`.
 *
 * @return Samples written (== wfm_plan_len(p)).
 */
size_t wfm_plan_render(const wfm_plan_t *p, const char *overrides_json,
                       float _Complex *out);

/**
 * @brief Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse).
 *
 * `out = Σ gain_k·cache_k + gain(snr)·noise(seed)`; writes `wfm_plan_len(p)`
 * samples. Equivalent to `render` with only `{"snr":snr,"seed":seed}`.
 *
 * @return Samples written (== wfm_plan_len(p)).
 */
size_t wfm_plan_at(const wfm_plan_t *p, double snr, uint64_t seed,
                   float _Complex *out);

/** @brief Destroy a Plan and free its caches. NULL is a no-op. */
void wfm_plan_destroy(wfm_plan_t *p);

#ifdef __cplusplus
}
#endif

#endif /* WFM_PLAN_H */
