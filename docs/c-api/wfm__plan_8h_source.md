

# File wfm\_plan.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_plan.h**](wfm__plan_8h.md)

[Go to the documentation of this file](wfm__plan_8h.md)


```C++
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

typedef struct wfm_plan wfm_plan_t;

wfm_plan_t *wfm_plan_prepare(const char *spec_json);

size_t wfm_plan_len(const wfm_plan_t *p);

size_t wfm_plan_n_sources(const wfm_plan_t *p);

uint64_t wfm_plan_anchor_seed(const wfm_plan_t *p);

size_t wfm_plan_render(const wfm_plan_t *p, const char *overrides_json,
                       float _Complex *out);

size_t wfm_plan_at(const wfm_plan_t *p, double snr, uint64_t seed,
                   float _Complex *out);

void wfm_plan_destroy(wfm_plan_t *p);

#ifdef __cplusplus
}
#endif

#endif /* WFM_PLAN_H */
```


