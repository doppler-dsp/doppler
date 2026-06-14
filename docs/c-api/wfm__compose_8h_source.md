

# File wfm\_compose.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the documentation of this file](wfm__compose_8h.md)


```C++

#ifndef WFM_COMPOSE_H
#define WFM_COMPOSE_H

#include "clib_common.h"
#include "wfm_synth/wfm_synth_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int type;          /* WFM_SYNTH_TONE … WFM_SYNTH_BITS */
    double freq;       /* freq offset (Hz); chirp: start frequency f_start */
    double snr;        /* dB, per snr_mode */
    int snr_mode;      /* 0 auto, 1 fs, 2 ebno, 3 esno */
    uint32_t seed;     /* PRNG / LFSR seed */
    int sps;           /* samples per symbol / chip */
    int pn_length;     /* LFSR register length */
    uint64_t pn_poly;  /* 0 → MLS poly for the length */
    int lfsr;          /* 0 galois, 1 fibonacci */
    double level;      /* source level in dBFS (≤0); 0 = unit power, no gain */
    double f_end;      /* chirp end frequency (Hz); ignored by other types */
    uint8_t *bits;     /* type=bits: pattern (0/1), owned; NULL otherwise */
    size_t n_bits;     /* type=bits: pattern length */
    int modulation;    /* type=bits: 0 none, 1 bpsk, 2 qpsk */
    int pulse;         /* pn/bpsk/qpsk pulse shape: 0 rect, 1 rrc */
    double rrc_beta;   /* RRC roll-off (pulse=rrc) */
    int rrc_span;      /* RRC support in symbols (pulse=rrc) */
} wfm_source_t;

typedef struct {
    wfm_source_t *sources; /* n_sources sources summed at the same time */
    size_t n_sources;
    double fs;             /* sample rate (Hz) — one per segment */
    size_t num_samples;    /* on-time (samples) */
    size_t off_samples;    /* off-time gap after the segment (samples) */
} wfm_segment_t;

int wfm_resolve_noise(wfm_segment_t *segs, size_t n);

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

char *wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                       int continuous, double headroom);

double wfm_spec_headroom(const char *json);

char *wfm_spec_template_json(void);

wfm_compose_state_t *wfm_compose_from_json(const char *json);

wfm_compose_state_t *wfm_compose_from_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
```


