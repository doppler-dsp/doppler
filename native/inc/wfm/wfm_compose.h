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
 * bits 4–6 on `wfm_segment_t.ranged`.
 */
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
    int background;    /* 1 = static background: prepare() folds a contiguous
                          prefix of background sources into ONE pre-summed Plan
                          cache slot (scaled/rotated/dropped as a unit), instead
                          of caching each individually. Ignored by compose().  */
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
    /* type=dsss, CONTINUOUS mode: a data-symbol rate independent of the code
       epoch rate selects the continuous form (wfm_synth_set_dsss_cont) over
       the burst form above -- one waveform type, one discriminator, rather
       than a tenth entry in five hand-maintained name tables. 0 = burst.
       The frame fields (acq_code/sync/crc/bits) are meaningless when this is
       set and are rejected by the caller rather than silently ignored. */
    double symbol_rate;  /* Hz; > 0 selects continuous async DSSS */
    int dsss_code_only;  /* continuous dsss: 1 = code-only (--data none), no
                            data modulation; 0 = data-modulated (payload if
                            supplied, else the seeded PN). Ignored for burst. */
    /* Channel coding over the frame, as STAGES with the spans they cover
       (wfm/wfm_frame.h). Each is optional and they do not all cover the same
       bits, which is the whole reason the frame is a description rather than
       a pipeline: the outer code and the randomiser reach over the payload
       group, and the inner code reaches over everything including a marker
       neither of the other two touches.

       Set all four with a Transfer Frame payload and no preamble or sync word
       and the result is a CCSDS CADU. That is the point -- CCSDS is the
       configuration these flags reach, not a mode they switch into. */
    unsigned rs_depth;   /* outer code interleaving depth; 0 = no outer code.
                            4.3.5.1 allows 1,2,3,4,5,8 and the payload must be
                            exactly 223*depth octets -- virtual fill is not
                            implemented (gh-813), so any other length is
                            refused rather than padded. */
    int randomise;       /* XOR a section-10 pseudo-random sequence over the
                            payload group -- not over a marker, which has to
                            look the same in every frame to be found.
                            0 = off, 1 = 131.0-B-6 10.4.1's 131071-bit
                            sequence (the `shall`), 2 = 10.4.2's 255-bit one,
                            which B-6 keeps only for legacy systems. It is a
                            CHOICE rather than a flag because B-6 makes it
                            one, and because the two produce waveforms only
                            the matching receiver derandomises. */
    int attach_asm;      /* prepend the 0x1ACFFC1D marker as a FIELD */
    int convolutional;   /* inner code over the whole frame, marker included;
                            doubles the bit count (rate 1/2, K=7) */
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

/**
 * @brief One rendered segment instance's exact timing: where it lands in the
 * composed stream and how its `delay | on | off` spans divide it.
 *
 * Produced by wfm_compose_spans() — the deterministic replay of the ranged
 * draws (same hash, epoch 0), so the reported positions match the rendered
 * capture sample-for-sample without rendering anything. This is the ground
 * truth a detector-scoring pipeline or a SigMF annotation needs: the burst
 * (on-time) of instance k starts at `start + delay` and runs `on` samples.
 */
typedef struct {
    size_t seg;      /* segment index in the spec */
    size_t instance; /* repeats instance, 0-based */
    size_t start;    /* absolute sample index where the instance begins */
    size_t delay;    /* leading gap length (samples) */
    size_t on;       /* on-time length (samples) */
    size_t off;      /* trailing gap length (samples) */
} wfm_span_t;

/**
 * @brief Replay the (epoch 0) instance timeline of a resolved segment list.
 *
 * Walks every segment's `repeats` instances, re-deriving each instance's
 * drawn delay/on/off exactly as the streaming composer will (identical draw
 * hash), and fills `out` with up to `cap` spans in stream order. Returns the
 * TOTAL instance count regardless of `cap` — call once with cap 0 to size,
 * then again with a buffer. Pass the RESOLVED segments (wfm_compose_segments()
 * on a live composer) so intrinsic on-times (dsss) are already folded in.
 *
 * Assumes every segment builds: a segment that fails at render time (invalid
 * burst geometry) degrades to its gaps only, so positions after it would
 * shift relative to this replay.
 *
 * @param segs   Resolved segment array.
 * @param n_segs Segment count.
 * @param out    Span buffer (may be NULL when cap is 0).
 * @param cap    Capacity of out in spans.
 * @return Total number of instances in one pass of the spec.
 */
size_t wfm_compose_spans(const wfm_segment_t *segs, size_t n_segs,
                         wfm_span_t *out, size_t cap);

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
 * For `type=dsss` the symbol is the outer *data* symbol. For a BURST that
 * spans `sf * sps` samples (sf chips, sps samples per chip). For a CONTINUOUS
 * async stream the data clock is independent of the code, so the span is
 * `fs / symbol_rate` samples — passed as @p sym_span (non-integer), which
 * OVERRIDES the `sf·sps` reconstruction when non-zero. `auto` picks esno, and
 * esno/ebno convert as `snr − 10·log10(span)` (BPSK payload, so the two
 * coincide). Every other type ignores `sf` and `sym_span`.
 *
 * @param snr_mode 0 auto, 1 fs, 2 ebno, 3 esno.
 * @param type     A WFM_SYNTH_* waveform type (selects the auto convention).
 * @param sps      Samples per symbol/chip (≥1; <1 treated as 1).
 * @param sf       Spreading factor — chips per data symbol (burst dsss; ≥1,
 *                 <1 treated as 1).
 * @param sym_span Continuous-dsss symbol span in samples (`fs/symbol_rate`);
 *                 0 = burst/non-dsss, derive from `sf·sps`.
 * @param snr      The declared SNR in dB.
 * @return SNR over fs in dB.
 */
double wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf,
                       double sym_span, double snr);

/**
 * @brief Resolve a source's (snr, snr_mode) into the pair to hand to
 * `wfm_synth_create()`.
 *
 * `wfm_synth_create()` runs before a dsss source's codes are attached, so it
 * cannot know the spreading factor its own esno would need. This helper — the
 * one create-time entry point shared by the composer (`wfm_compose_build_synth`)
 * and the standalone-Synth bridge (`wfm_source_to_synth`), so every face agrees
 * to the bit — converts a dsss source's SNR to the over-fs reference (via
 * `wfm_snr_over_fs`; the burst span is `sf = n_data_code`, a continuous stream
 * uses `fs/symbol_rate`) and returns `snr_mode=fs`; every other type passes
 * through unchanged.
 *
 * @param src      The source (supplies type/sps/snr_mode/n_data_code/
 *                 symbol_rate).
 * @param fs       Segment sample rate (Hz) — needed for a continuous dsss
 *                 source's `fs/symbol_rate` span; ignored otherwise.
 * @param snr      The declared SNR in dB, already ranged-resolved.
 * @param snr_mode Receives the snr_mode for create.
 * @return The SNR in dB for create.
 */
double wfm_source_create_snr(const wfm_source_t *src, double fs, double snr,
                             int *snr_mode);

/**
 * @brief Attach a dsss source's data to a freshly-created synth.
 *
 * The single dsss-attach path, called by BOTH synth-construction faces
 * (`wfm_compose_build_synth` and the standalone `wfm_source_to_synth`), so the
 * two cannot drift on how a dsss stream is configured. Selects on
 * `symbol_rate`: 0 → the burst form (`wfm_synth_set_dsss`); > 0 → the
 * continuous form (`wfm_synth_set_dsss_cont`) with `chips_per_symbol =
 * (fs/sps)/symbol_rate`, taking the data from the payload when one is supplied
 * (`bits`) and otherwise from the seeded PN. A no-op for a non-dsss source.
 *
 * @param syn  A synth from wfm_synth_create() with `wtype == WFM_SYNTH_DSSS`.
 * @param src  The source (codes, payload, symbol_rate, pn config).
 * @param fs   Segment sample rate (Hz) — the continuous chip rate is fs/sps.
 * @return 0 on success (or non-dsss no-op); -1 on invalid geometry.
 */
int wfm_source_attach_dsss(wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs);

/**
 * @brief Non-zero when this source describes a FRAME.
 *
 * A preamble or a sync word is what says "framed". **Deliberately not `crc`**:
 * it defaults to crc16 on every source (`[[module.wfm_compose.source.fields]]`
 * and wfmgen alike), so reading it as intent would silently append a trailer
 * to every unframed bit pattern anyone has ever generated. With neither a
 * preamble nor a sync word, `crc` stays inert exactly as it always was.
 *
 * @param src  The source; NULL reads as unframed.
 */
int wfm_source_has_frame(const wfm_source_t *src);

/**
 * @brief NULL when this source's frame fields can be honoured; else why not.
 *
 * ONE rule, asked by all three faces — the wfmgen CLI before it generates, the
 * standalone `Synth` through `wfm_source_to_synth`, and the composer through
 * `wfm_compose_create` — because the alternative is what shipped: the flags
 * were accepted, stored and readable back on every face, and applied on none
 * of them, so a caller who asked for a framed waveform silently got an
 * unframed one.
 *
 * A frame needs a payload, and the unspread types that source their symbols
 * from the PN LFSR (`bpsk`/`qpsk`/`pn`) have no length to bound one. So the
 * frame is honoured where the payload is EXPLICIT — `type=bits` with a
 * pattern, which `modulation` already maps to BPSK or QPSK — and refused with
 * a reason everywhere else.
 *
 * @param src  The source.
 * @return NULL if there is nothing wrong, else a static message.
 */
const char *wfm_source_frame_error(const wfm_source_t *src);

/**
 * @brief Attach an unspread source's bit pattern, framed or not.
 *
 * The `type=bits` counterpart of wfm_source_attach_dsss(), and called from the
 * same two places for the same reason. When the source carries a frame, the
 * pattern handed to `wfm_synth_set_bits()` is `wfm_frame_bits()` of
 * `[preamble x reps | sync | payload | crc]` rather than the payload alone —
 * so the layout, the CRC's position and its bit order come from the one
 * descriptor that the DSSS path and the receiver already read.
 *
 * The frame CYCLES, exactly as an unframed pattern does: one descriptor fills
 * whatever length is asked for, which is what turns a one-frame description
 * into a multi-frame record.
 *
 * @param syn  A synth from wfm_synth_create() with `wtype == WFM_SYNTH_BITS`.
 * @param src  The source (pattern, modulation, and any frame fields).
 * @return 0 on success (or a non-bits/no-pattern no-op); -1 on failure.
 */
int wfm_source_attach_frame(wfm_synth_state_t *syn, const wfm_source_t *src);

/**
 * @brief Construct + configure the synth for one resolved source.
 *
 * THE single synth-construction path (create + chirp-span pin + bits/symbols/RRC
 * attach + per-repeat NOISE reseed) shared by the streaming composer and the
 * Plan stimulus cache, so a cached per-source render is byte-identical to the
 * composed one. `freq/snr/f_end` are passed already ranged-resolved by the
 * caller; `on_len` pins a chirp's sweep to the on-time; `epoch`/`seed_advance`
 * (a ::wfm_seed_advance_t) drive the per-repeat seed policy — `epoch == 0`
 * yields the unmodified seed. `instance` is the segment's `repeats` counter
 * (0-based): a non-zero instance always reseeds the AWGN (fresh noise per
 * burst instance, signal fixed, regardless of `seed_advance`); instance 0 is
 * byte-identical to the pre-`repeats` behaviour.
 *
 * @return A heap synth (caller wfm_synth_destroy()s it), or NULL on failure.
 */
wfm_synth_state_t *wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance,
                                           size_t instance);

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
