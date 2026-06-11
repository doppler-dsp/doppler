/**
 * @file wfm_compose.h
 * @brief Multi-segment waveform composer (Phase B).
 *
 * Sequences a list of segments — each one a `synth` configuration plus an
 * on-time and a trailing off-time gap — into a single IQ stream, optionally
 * repeating the whole sequence or running forever. The composer owns one
 * `synth` at a time (the active segment) and reuses the Phase-A engine
 * verbatim, so every waveform type / SNR mode / MLS behaviour is identical to
 * the single-waveform path; a one-segment spec is byte-identical to calling
 * `synth` directly.
 *
 * Lifecycle: wfm_compose_create -> wfm_compose_execute* -> wfm_compose_destroy
 *
 * @code
 * wfm_source_t tone = {.type = 0, .freq = 1e5, .snr = 100.0};
 * wfm_source_t qpsk = {.type = 4, .sps = 8, .snr = 9.0};
 * wfm_segment_t segs[2] = {
 *     {.sources = &tone, .n_sources = 1, .fs = 1e6,
 *      .num_samples = 1000, .off_samples = 500},          // tone, then a gap
 *     {.sources = &qpsk, .n_sources = 1, .fs = 1e6,
 *      .num_samples = 4096, .off_samples = 0},            // qpsk
 * };
 * wfm_compose_state_t *c = wfm_compose_create(segs, 2, 0, 0);
 * float complex buf[4096];
 * size_t n;
 * while ((n = wfm_compose_execute(c, buf, 4096)) > 0) { ... }
 * wfm_compose_destroy(c);
 * @endcode
 */
#ifndef WFM_COMPOSE_H
#define WFM_COMPOSE_H

#include "clib_common.h"
#include "synth/synth_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One additive source within a segment: a `synth` config + its level.
 *
 * The nine synth fields mirror `synth_create()` (minus `fs`, which is the
 * segment's — one receiver, one sample rate). `level` is the source's average
 * power in dBFS (≤0); the segment sums its sources, each scaled by
 * `10^(level/20)`.
 */
typedef struct {
    int type;          /* SYNTH_TONE … SYNTH_QPSK */
    double freq;       /* freq offset (Hz) */
    double snr;        /* dB, per snr_mode */
    int snr_mode;      /* 0 auto, 1 fs, 2 ebno, 3 esno */
    uint32_t seed;     /* PRNG / LFSR seed */
    int sps;           /* samples per symbol / chip */
    int pn_length;     /* LFSR register length */
    uint64_t pn_poly;  /* 0 → MLS poly for the length */
    int lfsr;          /* 0 galois, 1 fibonacci */
    double level;      /* source level in dBFS (≤0); 0 = unit power, no gain */
} wfm_source_t;

/**
 * @brief One composer segment: one or more sources summed over the same span,
 * then a trailing off-time gap.
 *
 * A 1-source segment is byte-identical to driving that source's `synth`
 * directly. `num_samples` is the on-time; `off_samples` is a trailing gap of
 * zeros. Durations in seconds are `round(duration * fs)` — the caller resolves.
 */
typedef struct {
    wfm_source_t *sources; /* n_sources sources summed at the same time */
    size_t n_sources;
    double fs;             /* sample rate (Hz) — one per segment */
    size_t num_samples;    /* on-time (samples) */
    size_t off_samples;    /* off-time gap after the segment (samples) */
} wfm_segment_t;

/**
 * @brief Resolve a segment list's noise model in place (Phase 4b).
 *
 * No-op for 1-source segments (keeps the bundled-synth path byte-identical).
 * For a multi-source segment it sets one shared noise floor (from an explicit
 * SYNTH_NOISE source, else the first snr-bearing source), cleans the signal
 * sources, and appends a SYNTH_NOISE source at the floor — so the composer's
 * accumulator just sums. May `realloc` each segment's `sources`. Idempotent.
 *
 * `wfm_compose_create()` calls this on its private copy, so every face (CLI,
 * JSON, Python) resolves identically.
 *
 * @return 0 on success; -1 if a non-anchor source over-specifies (snr + level)
 *         or on allocation failure.
 */
int wfm_resolve_noise(wfm_segment_t *segs, size_t n);

/** Opaque composer state. */
typedef struct wfm_compose_state wfm_compose_state_t;

/**
 * @brief Build a composer over a copy of `segs`.
 *
 * @param segs        Segment list (copied; caller keeps ownership).
 * @param n_segs      Number of segments (>= 1).
 * @param repeat      Non-zero: loop the whole sequence after the last segment.
 * @param continuous  Non-zero: never finish (implies repeat); execute always
 *                    returns `max`.
 * @return Heap state, or NULL on bad args / allocation / synth failure.
 * @note Caller must wfm_compose_destroy() when done.
 */
wfm_compose_state_t *wfm_compose_create(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

/**
 * @brief Emit up to `max` samples of the composed stream.
 * @return Number of samples written: < `max` (or 0) signals the sequence
 *         finished (never, when `continuous`).
 */
size_t wfm_compose_execute(
    wfm_compose_state_t *state, float complex *out, size_t max);

/** @brief Destroy a composer and its active synth. @param state May be NULL. */
void wfm_compose_destroy(wfm_compose_state_t *state);

/**
 * @brief Borrow the composer's stored segment list (for --record / SigMF).
 * @param state      the composer.
 * @param n_out      receives the segment count.
 * @param repeat     receives the repeat flag (may be NULL).
 * @param continuous receives the continuous flag (may be NULL).
 * @return Pointer to the internal segments (owned by the composer; valid until
 *         wfm_compose_destroy).
 */
const wfm_segment_t *wfm_compose_segments(const wfm_compose_state_t *state,
                                          size_t *n_out, int *repeat,
                                          int *continuous);

/* ── JSON spec (the shared --from-file / --record format) ─────────────────── */
/*
 * One canonical schema, sample-exact so a recorded run reproduces byte-for-byte
 * when fed back via --from-file:
 *
 *   { "version": "wfmgen-1", "repeat": false, "continuous": false,
 *     "segments": [
 *       { "type": "tone", "fs": 1e6, "freq": 1e5, "snr": 100.0,
 *         "snr_mode": "auto", "seed": 1, "sps": 8, "pn_length": 7,
 *         "pn_poly": 0, "num_samples": 1000, "off_samples": 500 }, … ] }
 *
 * `type` and `snr_mode` are strings; everything else is numeric. Missing fields
 * fall back to the synth defaults.
 */

/**
 * @brief Serialise a spec to a JSON string (for --record).
 * @return malloc'd JSON (caller frees), or NULL on allocation failure.
 */
char *wfm_spec_to_json(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

/**
 * @brief Build a composer from a JSON spec string (for --from-file).
 * @return Composer state, or NULL on parse error / bad type / no segments.
 */
wfm_compose_state_t *wfm_compose_from_json(const char *json);

/**
 * @brief Build a composer from a JSON spec file.
 * @return Composer state, or NULL on read/parse error.
 */
wfm_compose_state_t *wfm_compose_from_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
