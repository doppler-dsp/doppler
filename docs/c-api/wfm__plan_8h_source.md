

# File wfm\_plan.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_plan.h**](wfm__plan_8h.md)

[Go to the documentation of this file](wfm__plan_8h.md)


```C++
/*
 * wfm_plan.h — "prepare once, materialize many" stimulus engine.
 *
 * A composed scene is a sequence of segments, each a linear form
 *
 *     segment = Σ_k gain_k · signal_k  +  noise
 *
 * (wfm_resolve_noise() cleans every signal source and appends one
 * WFM_SYNTH_NOISE source at the shared floor, for a multi-source segment; a
 * lone source carrying its own real SNR keeps it — its AWGN is baked into
 * its own synth, "bundled"). The expensive DSP — LFSR spread, RRC
 * convolution, transcendental LO — lives entirely in the signal terms,
 * which are INVARIANT across a parameter sweep and across a segment's
 * `repeats` instances (only the AWGN, and any ranged gap length, vary per
 * instance). So a Plan renders each segment's signal ON-time ONCE (via the
 * composer's own wfm_compose_build_synth, so a cached render is
 * byte-identical to a full compose), caches it, and then re-materializes
 * any variation as a cheap re-weighted sum per segment/instance plus a
 * regenerated noise synth spanning that instance's delay+on+off:
 *
 *     render(θ) = concat over segments, repeat instances of
 *                 [ noise(delay) | Σ_k g_k(θ)·e^{jφ_k(θ)}·cache_k + noise(on) | noise(off) ]
 *
 * v1 axes (all bit-exact vs a full compose): per-source gain/level, phase,
 * enable/disable, global SNR/noise-floor and Monte-Carlo noise-seed —
 * applied uniformly across every segment/instance that carries noise.
 * Frequency (Doppler) and multipath delay are staged follow-ups on the same
 * frame.
 *
 * Scope: any number of finite segments (no continuous/repeat scene — that
 * has no fixed capacity); each segment may declare `repeats` (bounded
 * instancing, AWGN fresh per instance, signal fixed) and ranged
 * `off_samples`/`delay_samples` (redrawn per instance at materialize time,
 * via the same deterministic hash the streaming composer uses). Still out
 * of scope: a ranged on-time (`num_samples`) — it would invalidate the
 * fixed-length signal cache — and any ranged per-source field
 * (freq/snr/level/f_end) — redrawing a source's frequency or SNR would
 * invalidate its cached render, defeating the "expensive DSP once"
 * guarantee this exists to provide.
 *
 * `wfm_plan_len()` is a WORST-CASE capacity (every ranged gap at its `hi`
 * bound): `render()`/`at()` write up to that many samples but return the
 * ACTUAL length of that specific draw — trailing samples beyond the
 * return value are zero padding. A `seed` override drives both the noise
 * realization AND (for a scene with a ranged gap/delay) the gap/delay
 * redraw; the "no-override reproduces wfm_compose to the bit" guarantee is
 * scoped to `render()` with no `"seed"` key (or `NULL`/`"{}"` overrides) —
 * each segment then draws from its own baked default seed (its first
 * source's `seed` field), same as a plain compose. `at()`'s `seed` is
 * always an explicit override (parallel to its always-explicit `snr`), so
 * it does not carry that baseline guarantee for a ranged-gap scene.
 *
 * `gains`/`phases`/`enable` override arrays are flat and segment-major
 * (segment 0's sources in scene order, then segment 1's, ...) — length
 * `wfm_plan_n_sources()`.
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


