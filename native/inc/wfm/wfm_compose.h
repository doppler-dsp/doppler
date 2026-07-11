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
#include "wfm_synth/wfm_synth_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Per-field "draw uniformly each repeat" flags (`ranged` bitmask).
 *
 * A scalar field is a constant; a *ranged* field carries a `[lo, hi]` span (the
 * scalar holds `lo`, a companion `*_hi` holds `hi`) and is redrawn uniformly in
 * `[lo, hi]` at the start of every repeat (composer epoch) — so a looped /
 * continuous stream can vary Doppler (`freq`), arrival jitter (`off_samples`),
 * etc. burst-to-burst while staying *reproducible*: the draw is a deterministic
 * hash of the source seed, the epoch, the segment/source index, and the field,
 * so `--record` stores the span (not a drawn value) and `--from-file` replays
 * the same sequence byte-for-byte. Bits 0–3 live on `wfm_source_t.ranged`;
 * bits 4–5 on `wfm_segment_t.ranged`.
 */
enum
{
  WFM_RANGE_FREQ        = 1u << 0, /* source.freq  → [freq, freq_hi]   */
  WFM_RANGE_SNR         = 1u << 1, /* source.snr   → [snr, snr_hi]     */
  WFM_RANGE_LEVEL       = 1u << 2, /* source.level → [level, level_hi] */
  WFM_RANGE_FEND        = 1u << 3, /* source.f_end → [f_end, f_end_hi] */
  WFM_RANGE_NUM_SAMPLES = 1u << 4, /* segment.num_samples span         */
  WFM_RANGE_OFF_SAMPLES = 1u << 5, /* segment.off_samples span         */
};

/**
 * @brief One additive source within a segment: a `synth` config + its level.
 *
 * The nine synth fields mirror `wfm_synth_create()` (minus `fs`, which is the
 * segment's — one receiver, one sample rate). `level` is the source's average
 * power in dBFS (≤0); the segment sums its sources, each scaled by
 * `10^(level/20)`.
 *
 * Any of `freq`/`snr`/`level`/`f_end` may be a per-repeat uniform draw: set the
 * matching `WFM_RANGE_*` bit in `ranged`, leave the scalar as `lo`, and put `hi`
 * in the `*_hi` companion (see the `ranged` enum).
 */
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
    unsigned ranged;       /* WFM_RANGE_{NUM,OFF}_SAMPLES bitmask */
    size_t num_samples_hi; /* upper bound when WFM_RANGE_NUM_SAMPLES is set */
    size_t off_samples_hi; /* upper bound when WFM_RANGE_OFF_SAMPLES is set */
} wfm_segment_t;

/**
 * @brief Resolve a segment list's noise model in place (Phase 4b).
 *
 * No-op for 1-source segments (keeps the bundled-synth path byte-identical).
 * For a multi-source segment it sets one shared noise floor (from an explicit
 * WFM_SYNTH_NOISE source, else the first snr-bearing source), cleans the signal
 * sources, and appends a WFM_SYNTH_NOISE source at the floor — so the composer's
 * accumulator just sums. May `realloc` each segment's `sources`. Idempotent.
 *
 * `wfm_compose_create()` calls this on its private copy, so every face (CLI,
 * JSON, Python) resolves identically.
 *
 * @return 0 on success; -1 if a non-anchor source over-specifies (snr + level)
 *         or on allocation failure.
 */
int wfm_resolve_noise(wfm_segment_t *segs, size_t n);

/**
 * @brief SNR (dB) referred to fs, from a source's snr/snr_mode/sps/type.
 *
 * The single source of truth for the Es/No, Eb/No, and over-fs conventions
 * (`snr_mode` 0 auto / 1 fs / 2 ebno / 3 esno). `wfm_resolve_noise()` uses it to
 * place the shared noise floor at `level(anchor) − wfm_snr_over_fs(anchor)`, and
 * the Plan stimulus engine reuses it to recompute the floor at an arbitrary
 * swept SNR — so both agree to the bit.
 *
 * For `type=dsss` the symbol is the outer *data* symbol, which spans
 * `sf * sps` samples (sf chips, sps samples per chip): `auto` picks esno, and
 * esno/ebno convert as `snr − 10·log10(sf·sps)` (BPSK payload, so the two
 * coincide). Every other type ignores `sf`.
 *
 * @param snr_mode 0 auto, 1 fs, 2 ebno, 3 esno.
 * @param type     A WFM_SYNTH_* waveform type (selects the auto convention).
 * @param sps      Samples per symbol/chip (≥1; <1 treated as 1).
 * @param sf       Spreading factor — chips per data symbol (dsss only; ≥1,
 *                 <1 treated as 1).
 * @param snr      The declared SNR in dB.
 * @return SNR over fs in dB.
 */
double wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf, double snr);

/**
 * @brief Resolve a source's (snr, snr_mode) into the pair to hand to
 * `wfm_synth_create()`.
 *
 * `wfm_synth_create()` runs before a dsss source's codes are attached, so it
 * cannot know the spreading factor its own esno would need. This helper — the
 * one create-time entry point shared by the composer (`wfm_compose_build_synth`)
 * and the standalone-Synth bridge (`wfm_source_to_synth`), so every face agrees
 * to the bit — converts a dsss source's SNR to the over-fs reference
 * (via `wfm_snr_over_fs` with `sf = n_data_code`) and returns `snr_mode=fs`;
 * every other type passes through unchanged.
 *
 * @param src      The source (supplies type/sps/snr_mode/n_data_code).
 * @param snr      The declared SNR in dB, already ranged-resolved.
 * @param snr_mode Receives the snr_mode for create.
 * @return The SNR in dB for create.
 */
double wfm_source_create_snr(const wfm_source_t *src, double snr,
                             int *snr_mode);

/**
 * @brief Construct + configure the synth for one resolved source.
 *
 * THE single synth-construction path (create + chirp-span pin + bits/symbols/RRC
 * attach + per-repeat NOISE reseed) shared by the streaming composer and the
 * Plan stimulus cache, so a cached per-source render is byte-identical to the
 * composed one. `freq/snr/f_end` are passed already ranged-resolved by the
 * caller; `on_len` pins a chirp's sweep to the on-time; `epoch`/`seed_advance`
 * (a ::wfm_seed_advance_t) drive the per-repeat seed policy — `epoch == 0`
 * yields the unmodified seed.
 *
 * @return A heap synth (caller wfm_synth_destroy()s it), or NULL on failure.
 */
wfm_synth_state_t *wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance);

/**
 * @brief Per-repeat seed policy for a looped/continuous stream.
 *
 * A source's single `seed` feeds two RNGs: the PN LFSR (spreading code *and*
 * data bits — one register) and the AWGN generator. The clean cut is therefore
 * signal (code+data) vs. noise, exposed as an ordered, cumulative level.
 */
typedef enum
{
  WFM_SEED_ADVANCE_NONE  = 0, /* byte-identical repeats (default) */
  WFM_SEED_ADVANCE_NOISE = 1, /* signal fixed, AWGN fresh per repeat */
  WFM_SEED_ADVANCE_ALL   = 2, /* whole seed advances (code+data+noise) */
} wfm_seed_advance_t;

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
 * @brief Choose how the seed advances on each repeat of a looped/continuous
 * stream (a `wfm_seed_advance_t`):
 *  - `WFM_SEED_ADVANCE_NONE` (default): byte-identical repeats.
 *  - `WFM_SEED_ADVANCE_NOISE`: advance only the AWGN seed → a fresh noise
 *    realization each pass while the signal (LO / PN code / data / pulse) stays
 *    bit-identical (so a fixed preamble/code re-acquires every burst).
 *  - `WFM_SEED_ADVANCE_ALL`: advance the whole seed → code, data, and noise all
 *    change (a fully stochastic stream).
 *
 * Set before the first execute(); the first pass is always unchanged. An
 * out-of-range mode is ignored.
 * @param state  Compose state (may be NULL).
 * @param mode   A wfm_seed_advance_t value.
 */
void wfm_compose_set_seed_advance(wfm_compose_state_t *state, int mode);

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
 * Canonical schema: docs/schema/wfmgen.schema.json (JSON Schema 2020-12).
 * A recorded run reproduces byte-for-byte when fed back via --from-file.
 * Use `wfmgen json-template` for a ready-to-edit example covering all fields.
 */

/**
 * @brief Serialise a spec to a JSON string (for --record).
 *
 * `headroom` (dB of output backoff applied at the writer, not the composer) is
 * emitted as a top-level field only when non-zero, so an unrecorded run and any
 * pre-headroom spec stay byte-identical. Read it back with wfm_spec_headroom().
 *
 * @return malloc'd JSON (caller frees), or NULL on allocation failure.
 */
char *wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                       int continuous, double headroom);

/**
 * @brief The top-level `headroom` (dB) from a spec JSON, or 0 if absent.
 *
 * Lets `--from-file` reproduce a recorded `--headroom`; the value is a writer
 * gain, so it lives outside the composer state.
 */
double wfm_spec_headroom(const char *json);

/**
 * @brief A ready-to-edit example spec in the canonical --from-file schema.
 *
 * Returns a representative multi-segment template — an inline tone, an
 * RRC-shaped QPSK-from-bits burst with a trailing gap, and a two-source
 * additive `sum` mix — serialised with wfm_spec_to_json(), so it is valid by
 * construction and round-trips through wfm_compose_from_json() unchanged. It
 * therefore doubles as a working starting point for `wfmgen --from-file`, not
 * just documentation: dump it, edit the fields, feed it back.
 *
 * @return malloc'd JSON (caller frees), or NULL on allocation failure.
 */
char *wfm_spec_template_json(void);

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
