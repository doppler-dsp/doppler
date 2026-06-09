

# File wfm\_compose.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the documentation of this file](wfm__compose_8h.md)


```C++

#ifndef WFM_COMPOSE_H
#define WFM_COMPOSE_H

#include "clib_common.h"
#include "synth/synth_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int type;          /* SYNTH_TONE … SYNTH_QPSK */
    double fs;         /* sample rate (Hz) */
    double freq;       /* freq offset (Hz) */
    double snr;        /* dB, per snr_mode */
    int snr_mode;      /* 0 auto, 1 fs, 2 ebno, 3 esno */
    uint32_t seed;     /* PRNG / LFSR seed */
    int sps;           /* samples per symbol / chip */
    int pn_length;     /* LFSR register length */
    uint64_t pn_poly;  /* 0 → MLS poly for the length */
    int lfsr;          /* 0 galois, 1 fibonacci */
    size_t num_samples; /* on-time (samples) */
    size_t off_samples; /* off-time gap after the segment (samples) */
} wfm_segment_t;

typedef struct wfm_compose_state wfm_compose_state_t;

wfm_compose_state_t *wfm_compose_create(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

size_t wfm_compose_execute(
    wfm_compose_state_t *state, float complex *out, size_t max);

void wfm_compose_destroy(wfm_compose_state_t *state);

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

char *wfm_spec_to_json(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

wfm_compose_state_t *wfm_compose_from_json(const char *json);

wfm_compose_state_t *wfm_compose_from_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
```


