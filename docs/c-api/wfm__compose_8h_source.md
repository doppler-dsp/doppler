

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

enum
{
  WFM_RANGE_FREQ          = 1u << 0, /* source.freq  → [freq, freq_hi]   */
  WFM_RANGE_SNR           = 1u << 1, /* source.snr   → [snr, snr_hi]     */
  WFM_RANGE_LEVEL         = 1u << 2, /* source.level → [level, level_hi] */
  WFM_RANGE_FEND          = 1u << 3, /* source.f_end → [f_end, f_end_hi] */
  WFM_RANGE_NUM_SAMPLES   = 1u << 4, /* segment.num_samples span         */
  WFM_RANGE_OFF_SAMPLES   = 1u << 5, /* segment.off_samples span         */
  WFM_RANGE_DELAY_SAMPLES = 1u << 6, /* segment.delay_samples span       */
};

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
    float _Complex *symbols; /* type=symbols: stream, owned; NULL otherwise */
    size_t n_symbols;        /* type=symbols: stream length */
    int pulse;         /* pn/bpsk/qpsk pulse shape: 0 rect, 1 rrc */
    double rrc_beta;   /* RRC roll-off (pulse=rrc) */
    int rrc_span;      /* RRC support in symbols (pulse=rrc) */
    unsigned ranged;   /* WFM_RANGE_{FREQ,SNR,LEVEL,FEND} bitmask */
    double freq_hi;    /* upper bound when WFM_RANGE_FREQ is set */
    double snr_hi;     /* upper bound when WFM_RANGE_SNR is set */
    double level_hi;   /* upper bound when WFM_RANGE_LEVEL is set */
    double f_end_hi;   /* upper bound when WFM_RANGE_FEND is set */
    /* type=dsss: the two-code burst geometry (wfm_frame_dsss_chips). The
       payload bits ride the shared `bits` field above (alias "payload"). */
    uint8_t *acq_code;   /* preamble code (0/1), owned; NULL = no preamble */
    size_t n_acq_code;   /* preamble code length in chips */
    size_t acq_reps;     /* preamble repetitions */
    uint8_t *data_code;  /* payload spreading code (0/1), owned */
    size_t n_data_code;  /* chips per frame symbol (spreading factor) */
    uint8_t *sync;       /* frame-sync word bits (0/1), owned; NULL = none */
    size_t n_sync;       /* sync word length in bits */
    int crc;             /* frame trailer: 0 none, 1 crc16 (dp_crc16.h) */
} wfm_source_t;

typedef struct {
    wfm_source_t *sources; /* n_sources sources summed at the same time */
    size_t n_sources;
    double fs;             /* sample rate (Hz) — one per segment */
    size_t num_samples;    /* on-time (samples) */
    size_t off_samples;    /* off-time gap after the segment (samples) */
    unsigned ranged;       /* WFM_RANGE_{NUM,OFF}_SAMPLES bitmask */
    size_t num_samples_hi; /* upper bound when WFM_RANGE_NUM_SAMPLES is set */
    size_t off_samples_hi; /* upper bound when WFM_RANGE_OFF_SAMPLES is set */
    /* Bounded instancing: play this segment `repeats` times back-to-back
       (each instance = delay + on-time + trailing gap) before advancing.
       Every ranged field re-draws per instance and the AWGN is always fresh
       per instance, while the signal (codes/payload/PN phase) stays fixed —
       so `repeats=5` with a ranged off_samples is a 5-burst train with
       jittered gaps from one declaration. 0 and 1 both mean one instance;
       instance 0 renders byte-identically to a repeats-less segment. */
    size_t repeats;
    /* Leading gap before the on-time (samples) — "the burst arrives after a
       delay". Ranged like off_samples (WFM_RANGE_DELAY_SAMPLES), re-drawn
       per repeats instance, so a ranged delay is per-burst arrival jitter.
       Inter-burst spacing composes as off(k) + delay(k+1). */
    size_t delay_samples;
    size_t delay_samples_hi; /* upper bound when WFM_RANGE_DELAY_SAMPLES */
    /* Gap-noise policy for this segment's delay + trailing gap. 0 (auto,
       the default): the gaps carry the segment's noise floor — every
       source's additive-AWGN term keeps running (same stream, same power)
       while the signal stops, so a noisy scene's inter-burst region is the
       channel, not digital silence. Clean sources have no AWGN, so a clean
       scene's gaps remain exact zeros. 1 (off): gaps are hard zeros. */
    int gap_noise;
} wfm_segment_t;

typedef struct {
    size_t seg;      /* segment index in the spec */
    size_t instance; /* repeats instance, 0-based */
    size_t start;    /* absolute sample index where the instance begins */
    size_t delay;    /* leading gap length (samples) */
    size_t on;       /* on-time length (samples) */
    size_t off;      /* trailing gap length (samples) */
} wfm_span_t;

size_t wfm_compose_spans(const wfm_segment_t *segs, size_t n_segs,
                         wfm_span_t *out, size_t cap);

int wfm_resolve_noise(wfm_segment_t *segs, size_t n);

double wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf, double snr);

double wfm_source_create_snr(const wfm_source_t *src, double snr,
                             int *snr_mode);

wfm_synth_state_t *wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance,
                                           size_t instance);

typedef enum
{
  WFM_SEED_ADVANCE_NONE  = 0, /* byte-identical repeats (default) */
  WFM_SEED_ADVANCE_NOISE = 1, /* signal fixed, AWGN fresh per repeat */
  WFM_SEED_ADVANCE_ALL   = 2, /* whole seed advances (code+data+noise) */
} wfm_seed_advance_t;

typedef struct wfm_compose_state wfm_compose_state_t;

wfm_compose_state_t *wfm_compose_create(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

void wfm_compose_set_seed_advance(wfm_compose_state_t *state, int mode);

size_t wfm_compose_execute(
    wfm_compose_state_t *state, float complex *out, size_t max);

void wfm_compose_destroy(wfm_compose_state_t *state);

const wfm_segment_t *wfm_compose_segments(const wfm_compose_state_t *state,
                                          size_t *n_out, int *repeat,
                                          int *continuous);

/* ── JSON spec (the shared --from-file / --record format) ─────────────────── */
/*
 * Canonical schema: docs/schema/wfmgen.schema.json (JSON Schema 2020-12).
 * A recorded run reproduces byte-for-byte when fed back via --from-file.
 * Use `wfmgen json-template` for a ready-to-edit example covering all fields.
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


