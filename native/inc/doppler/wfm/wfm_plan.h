/**
 * @file wfm_plan.h
 * @brief "Prepare once, materialize many" stimulus engine.
 *
 * A composed scene is a sequence of segments, each a linear form
 *
 *     segment = Σ_k gain_k · signal_k  +  noise
 *
 * (dp_wfm_resolve_noise() cleans every signal source and appends one
 * WFM_SYNTH_NOISE source at the shared floor, for a multi-source segment; a
 * lone source carrying its own real SNR keeps it — its AWGN is baked into
 * its own synth, "bundled"). The expensive DSP — LFSR spread, RRC
 * convolution, transcendental LO — lives entirely in the signal terms,
 * which are INVARIANT across a parameter sweep and across a segment's
 * `repeats` instances (only the AWGN, and any ranged gap length, vary per
 * instance). So a Plan renders each segment's signal ON-time ONCE (via the
 * composer's own dp_wfm_compose_build_synth, so a cached render is
 * byte-identical to a full compose), caches it, and then re-materializes
 * any variation as a cheap re-weighted sum per segment/instance plus a
 * regenerated noise synth spanning that instance's delay+on+off:
 *
 *     render(θ) = concat over segments, repeat instances of
 *                 [ noise(delay) | Σ_k g_k(θ)·e^{jφ_k(θ)}·cache_k + noise(on)
 * | noise(off) ]
 *
 * v1 axes (all bit-exact vs a full compose): per-source gain/level, phase,
 * enable/disable, global SNR/noise-floor and Monte-Carlo noise-seed —
 * applied uniformly across every segment/instance that carries noise.
 * Clock Doppler is served too, by a different mechanism: the cache holds the
 * signal BEFORE the channel, and the channel is run at render time over it
 * (see dp_wfm_plan_prepare()). Multipath delay is a staged follow-up.
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
 * `dp_wfm_plan_len()` is a WORST-CASE capacity (every ranged gap at its `hi`
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
 * `dp_wfm_plan_n_sources()`.
 *
 * The Plan is a re-creatable derived cache, not evolving state. The cheapest,
 * most portable persistence is therefore the spec JSON (Composer.to_json())
 * plus a rebuild — KB, and always tracks the current DSP. dp_wfm_plan_save() /
 * dp_wfm_plan_restore() are the OPTIONAL fast-path alternative: they serialize
 * the cached sample buffers alongside the embedded spec so a restore SKIPS the
 * expensive DSP (build_synth) that dominates prepare(). This is a versioned
 * performance cache, NOT the dp_state.h elastic-state contract — it is gated
 * by a build-time DSP fingerprint (wfm_plan_dsp_hash.h), and on any mismatch
 * (different DSP build, or a spec/buffer inconsistency) restore transparently
 * REBUILDS from the embedded spec rather than reinterpreting stale bytes. So a
 * restore is never silently wrong: worst case it is as slow as prepare(); best
 * case (matching build) it is a memcpy. Prefer save/restore only when the DSP
 * cost is high and the spec-rebuild is undesirable (e.g. fanning one prepared
 * Plan across many workers); otherwise persist the spec and rebuild.
 */
#ifndef WFM_PLAN_H
#define WFM_PLAN_H

#include "doppler/dp_complex.h"
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
   * Parses + resolves the scene, validates scope per segment, then renders
   * and caches each segment's clean signal ON-time at gain 1. Returns NULL on
   * parse failure or an out-of-scope spec (continuous/repeat scene, a ranged
   * on-time, a ranged per-source field, a non-trailing/multiple noise
   * source within a segment, or a BACKGROUND source carrying clock Doppler).
   *
   * A source with clock Doppler IS served, and the render is compose() to the
   * bit. A Doppler channel is a resampler with state that runs through the
   * gaps too, so what a burst renders as depends on the leading delay and on
   * the previous instance's gap, and it resamples the AWGN of a bundled
   * source along with the signal. A per-source on-time cache cannot hold
   * either, so the cache holds the signal BEFORE the channel and render()
   * runs the channel over it, through the composer's own renderer: the delay,
   * the gaps, the noise and (for a PERSIST lifetime) the earlier segments are
   * all in hand when it runs. A ranged `doppler` / `doppler_rate` is drawn per
   * instance exactly as compose() draws it, and a seed override moves it.
   * Cost: a Doppler scene's render() is no longer a pure re-weight of the
   * cache -- each Doppler source runs one resampler pass over its timeline.
   * That pass is what render() parallelises: a segment's repeat instances
   * render concurrently when no source PERSISTs (they share no channel), and a
   * PERSIST segment, whose instances its channels chain, fans out across its
   * sources instead. The result is the same bits as the serial render. A
   * single burst from a single source has nothing to fan out, so there the
   * pass is simply the cost.
   *
   * The one refusal is a BACKGROUND source with Doppler: the background fold
   * sums those into a single composite before the render, and a channel is
   * per source, so it cannot be applied to a sum.
   *
   * @param spec_json A NUL-terminated composer spec JSON string.
   * @return Heap Plan (caller dp_wfm_plan_destroy()s it), or NULL.
   */
  wfm_plan_t *dp_wfm_plan_prepare (const char *spec_json);

  /** @brief Worst-case materialized length in samples (every ranged gap at
   * its `hi` bound) — the jm binding's out_len_fn / allocation capacity.
   *
   * @param p  The Plan, or NULL.
   * @return Capacity in samples that `out` must hold for render()/at();
   *         0 for a NULL Plan. */
  size_t dp_wfm_plan_len (const wfm_plan_t *p);

  /** @brief Number of cached signal sources across every segment (excludes
   * noise floors); the length of the `gains`/`phases`/`enable` arrays.
   *
   * @param p  The Plan, or NULL.
   * @return Source count; 0 for a NULL Plan. */
  size_t dp_wfm_plan_n_sources (const wfm_plan_t *p);

  /**
   * @brief The noise seed that reproduces a full compose.
   *
   * The first noisy segment's default seed (its first source's `seed`
   * field). Passing this as `dp_wfm_plan_at`'s seed (with the scene's base SNR)
   * yields the byte-identical output of `wfm_compose` for a single-segment
   * scene; for a multi-segment scene each segment still draws from its own
   * default seed unless overridden. Varying the seed draws independent
   * Monte-Carlo noise (and, for a ranged-gap scene, timing) realizations.
   *
   * @param p  The Plan, or NULL.
   * @return The first noisy segment's noise seed; 0 for a NULL Plan or a
   *         scene with no noisy segment.
   */
  uint64_t dp_wfm_plan_anchor_seed (const wfm_plan_t *p);

  /**
   * @brief Why a Plan refuses an `snr`: its scene carries no noise.
   *
   * A segment is noisy when a source in it has an `snr` below
   * WFM_SYNTH_SNR_CLEAN (100 dB), which gives the segment a noise floor that
   * an `snr` override moves. A scene with no such segment has nothing for
   * the override to move, so `dp_wfm_plan_at()` and a `"snr"` key to
   * `dp_wfm_plan_render()` are refused rather than returning the clean
   * signal at every SNR -- a sweep over it would read a perfect receiver.
   * The static reason every face reports.
   */
#define DP_WFM_PLAN_WHY_NO_NOISE                                              \
  "this scene carries no noise, so the Plan has no noise floor for snr to "   \
  "move: give a source a finite snr (below 100 dB) and an snr_mode"

  /**
   * @brief Whether an `snr` can be applied to this Plan.
   *
   * O(1): the answer is fixed at prepare time. A caller about to sweep SNR
   * checks it once; `dp_wfm_plan_at()` and `dp_wfm_plan_render()` apply the
   * same rule themselves.
   *
   * @code
   * >>> from doppler.wfm import Composer, prepare
   * >>> clean = prepare(Composer(type="tone", num_samples=64))
   * >>> clean.at(6.0)
   * Traceback (most recent call last):
   *     ...
   * ValueError: this scene carries no noise, so the Plan has no noise floor for snr to move: give a source a finite snr (below 100 dB) and an snr_mode (rc=-4)
   * >>> len(prepare(Composer(type="tone", num_samples=64, snr=10.0)).at(6.0))
   * 64
   * @endcode
   *
   * @param p  The Plan; NULL is refused like a noiseless scene.
   * @return 0 (DP_OK) when some segment carries noise; DP_ERR_INVALID (-4)
   *         when none does, for the reason DP_WFM_PLAN_WHY_NO_NOISE.
   */
  int dp_wfm_plan_check_snr (const wfm_plan_t *p);

  /** @brief Why an override is refused: the text is not a JSON object. */
#define DP_WFM_PLAN_WHY_NOT_OBJECT "overrides must be a JSON object"
  /** @brief Why an override is refused: it names a key the Plan does not
   * know (a typo would otherwise render the baseline). */
#define DP_WFM_PLAN_WHY_UNKNOWN_KEY                                           \
  "overrides take only the keys gains, phases, enable, snr and seed"
  /** @brief Why an override is refused: a per-source array has the wrong
   * length (or is not an array). */
#define DP_WFM_PLAN_WHY_ARRAY_LEN                                             \
  "gains, phases and enable must each be an array of n_sources entries"
  /** @brief Why an override is refused: a gains/phases entry is not a
   * number. */
#define DP_WFM_PLAN_WHY_NUMBER "every gains and phases entry must be a number"
  /** @brief Why an override is refused: an enable entry is neither a bool
   * nor a number. */
#define DP_WFM_PLAN_WHY_ENABLE "every enable entry must be a bool or a number"
  /** @brief Why an override is refused: `snr` is not a number. */
#define DP_WFM_PLAN_WHY_SNR "snr must be a number (dB)"
  /** @brief Why an override is refused: `seed` is not an integer in
   * `0..2^32-1` (every seed a Plan feeds is 32-bit). */
#define DP_WFM_PLAN_WHY_SEED "seed must be an integer in 0..4294967295"
  /** @brief Why an override is refused: there is no Plan. */
#define DP_WFM_PLAN_WHY_NULL "no Plan"

  /**
   * @brief Whether dp_wfm_plan_render() can honour @p overrides_json, and if
   *        not, why.
   *
   * The one rule render() applies before it draws anything (see its doc for
   * the keys). NULL or "" is the baseline and is accepted. An `snr` on a
   * Plan whose scene carries no noise is refused for the reason
   * DP_WFM_PLAN_WHY_NO_NOISE.
   *
   * @param p               The Plan; NULL is refused (DP_WFM_PLAN_WHY_NULL).
   * @param overrides_json  NUL-terminated override object, or NULL / "".
   * @param[out] why        When non-NULL, receives NULL on success or one of
   *                        the static DP_WFM_PLAN_WHY_* strings.
   * @return 0 (DP_OK) when render() would honour every key; DP_ERR_INVALID
   *         (-4) when it would refuse.
   */
  int dp_wfm_plan_check_overrides_why (const wfm_plan_t *p,
                                       const char *overrides_json,
                                       const char **why);

  /**
   * @brief dp_wfm_plan_check_overrides_why() without the reason.
   *
   * The status-only face, for a binding that raises a fixed message.
   *
   * @param p               The Plan.
   * @param overrides_json  NUL-terminated override object, or NULL / "".
   * @return 0 (DP_OK), or DP_ERR_INVALID (-4) when render() would refuse.
   */
  int dp_wfm_plan_check_overrides (const wfm_plan_t *p,
                                   const char *overrides_json);

  /**
   * @brief General render: apply a JSON override spec, return a cf32 array.
   *
   * `overrides_json` is a small JSON object, all keys optional:
   * `{"gains":[dB…], "phases":[rad…], "enable":[bool…], "snr":dB, "seed":u}`
   * (`gains`/`phases`/`enable` are per-source, flat and segment-major, length
   * = dp_wfm_plan_n_sources()). An empty object (or NULL) renders the baseline —
   * bit-identical to `Composer(scene).compose()`. Writes up to
   * `dp_wfm_plan_len(p)` samples to `out`. An `"snr"` key on a Plan whose
   * scene carries no noise is refused (see dp_wfm_plan_check_snr()).
   *
   * Overrides the Plan cannot honour are refused, never read as the
   * baseline: text that is not a JSON object, a key other than the five
   * above, a `gains`/`phases`/`enable` array whose length is not
   * dp_wfm_plan_n_sources(), a `gains`/`phases` entry that is not a number,
   * an `enable` entry that is neither a bool nor a number, a non-numeric
   * `snr`, and a `seed` that is not an integer in `0..2^32-1`. The rule is
   * dp_wfm_plan_check_overrides_why(), which names the reason. An `enable`
   * entry is on when it is `true` or a non-zero number.
   *
   * @param p               The Plan; NULL renders nothing and returns 0.
   * @param overrides_json  NUL-terminated override object, or NULL / ""
   *                        for the baseline.
   * @param[out] out        Receives the samples; must hold
   *                        dp_wfm_plan_len(p) of them.
   * @return Samples actually written for this draw (<= dp_wfm_plan_len(p)),
   *         or 0 when refused (a NULL Plan, an override
   *         dp_wfm_plan_check_overrides() refuses), with `out` untouched.
   */
  size_t dp_wfm_plan_render (const wfm_plan_t *p, const char *overrides_json,
                          float _Complex *out);

  /**
   * @brief Scalar fast-path for the hot Monte-Carlo/SNR loop (no JSON parse).
   *
   * `out = Σ gain_k·cache_k + gain(snr)·noise(seed)` per segment/instance;
   * writes up to `dp_wfm_plan_len(p)` samples. Equivalent to `render` with only
   * `{"snr":snr,"seed":seed}` — `seed` is always an explicit override here,
   * and so is `snr`: on a Plan whose scene carries no noise it is refused
   * (see dp_wfm_plan_check_snr()).
   *
   * @param p         The Plan.
   * @param snr       SNR override, dB, read per segment in the snr_mode
   *                  of the source that sets its SNR: a bundled segment's
   *                  lone source, else the source sharing the noise
   *                  source's seed. A segment whose noise floor was set
   *                  explicitly keeps that floor.
   * @param seed      Noise (and ranged-gap) seed override, `0..2^32-1`:
   *                  the seeds it replaces are 32-bit, so a larger one is
   *                  refused rather than cut to its low 32 bits (which
   *                  would make seeds differing only above bit 31 draw the
   *                  same realization).
   * @param[out] out  Receives the samples; must hold dp_wfm_plan_len(p)
   *                  of them.
   * @return Samples actually written for this draw (<= dp_wfm_plan_len(p)),
   *         or 0 when refused (no noise floor, or @p seed above 2^32-1),
   *         with `out` untouched.
   */
  size_t dp_wfm_plan_at (const wfm_plan_t *p, double snr, uint64_t seed,
                      float _Complex *out);

  /**
   * @brief Serialized size of a Plan blob (envelope + spec + cached buffers).
   *
   * The number of bytes dp_wfm_plan_save() writes: a small envelope with the DSP
   * fingerprint, the embedded spec JSON, and every cached signal buffer.
   * Dominated by the buffers (Σ per-source num_samples · 8 bytes) — multi-MB
   * for a large scene, which is exactly why the spec-rebuild path is the
   * default.
   *
   * @param p  The Plan, or NULL.
   * @return Blob size in bytes; 0 for a NULL Plan.
   */
  size_t dp_wfm_plan_save_bytes (const wfm_plan_t *p);

  /**
   * @brief Serialize a Plan into @p blob (dp_wfm_plan_save_bytes(p) bytes).
   *
   * Native-endian. The blob embeds the spec JSON, so a restore is
   * self-contained. Returns the number of bytes written (==
   * dp_wfm_plan_save_bytes(p)) — the actual-length contract a variable-output
   * binding needs, so `save() -> bytes` generates with no hand-written glue.
   *
   * @param p          The Plan.
   * @param[out] blob  Receives the blob; must hold
   *                   dp_wfm_plan_save_bytes(p) bytes.
   * @return Bytes written; 0 (nothing written) if @p p or @p blob is NULL.
   */
  size_t dp_wfm_plan_save (const wfm_plan_t *p, void *blob);

  /**
   * @brief Reconstruct a Plan from a blob produced by dp_wfm_plan_save().
   *
   * If the blob's DSP fingerprint matches this build AND its structure matches
   * the embedded spec, the cached buffers are loaded directly (no DSP).
   * Otherwise the Plan is REBUILT from the embedded spec via the full DSP —
   * same result, just paying prepare()'s cost. Returns NULL only on a
   * malformed/foreign-endian blob or an unparseable/out-of-scope embedded spec
   * (the same cases dp_wfm_plan_prepare rejects), never on a mere fingerprint
   * mismatch.
   *
   * @param blob Bytes from dp_wfm_plan_save().
   * @param n    Length of @p blob in bytes.
   * @return Heap Plan (caller dp_wfm_plan_destroy()s it), or NULL.
   */
  wfm_plan_t *dp_wfm_plan_restore (const void *blob, size_t n);

  /**
   * @brief Save a Plan to a file (dp_wfm_plan_save() bytes at @p path).
   *
   * @param p     The Plan.
   * @param path  File to create or truncate.
   * @return 0 on success; -1 on a NULL argument, an allocation failure, or
   *         an open/write/close error.
   */
  int dp_wfm_plan_dump (const wfm_plan_t *p, const char *path);

  /**
   * @brief Load a Plan from a file written by dp_wfm_plan_dump().
   *
   * Same fingerprint semantics as dp_wfm_plan_restore(): a matching build loads the
   * cached buffers, a mismatch rebuilds from the embedded spec.
   *
   * @param path  File written by dp_wfm_plan_dump().
   * @return Heap Plan (caller dp_wfm_plan_destroy()s it), or NULL on an open/read
   *         error or a malformed/foreign-endian file.
   */
  wfm_plan_t *dp_wfm_plan_load (const char *path);

  /** @brief Destroy a Plan and free its caches. NULL is a no-op.
   *
   * @param p  The Plan, or NULL. */
  void dp_wfm_plan_destroy (wfm_plan_t *p);

#ifdef __cplusplus
}
#endif

#endif /* WFM_PLAN_H */
