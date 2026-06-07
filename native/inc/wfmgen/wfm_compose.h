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
 * wfm_segment_t segs[2] = {
 *     {.type = 0, .fs = 1e6, .freq = 1e5, .snr = 100.0,
 *      .num_samples = 1000, .off_samples = 500},          // tone, then a gap
 *     {.type = 4, .fs = 1e6, .sps = 8, .snr = 9.0,
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
 * @brief One composer segment: a `synth` config + on/off sample counts.
 *
 * The nine synth fields mirror `synth_create()` exactly. `num_samples` is the
 * on-time (samples emitted from the synth); `off_samples` is a trailing gap of
 * zeros inserted after the segment (off-time). Durations in seconds are
 * `round(duration * fs)` — the caller resolves them.
 */
typedef struct {
    int type;          /* SYNTH_TONE … SYNTH_QPSK */
    double fs;         /* sample rate (Hz) */
    double freq;       /* freq offset (Hz) */
    double snr;        /* dB, per snr_mode */
    int snr_mode;      /* 0 auto, 1 fs, 2 ebno, 3 esno */
    uint32_t seed;     /* PRNG / LFSR seed */
    int sps;           /* samples per symbol / chip */
    int pn_length;     /* LFSR register length */
    uint32_t pn_poly;  /* 0 → MLS poly for the length */
    size_t num_samples; /* on-time (samples) */
    size_t off_samples; /* off-time gap after the segment (samples) */
} wfm_segment_t;

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
