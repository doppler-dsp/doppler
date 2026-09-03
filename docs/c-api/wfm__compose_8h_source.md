

# File wfm\_compose.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the documentation of this file](wfm__compose_8h.md)


```C++

#ifndef WFM_COMPOSE_H
#define WFM_COMPOSE_H

#include "clib_common.h"
#include "wfm_synth/wfm_synth_core.h"
#include "wfm/wfm_frame.h" /* wfm_frame_desc_t — a source's frame, described */
#include "doppler_channel/doppler_channel_core.h" /* a source's clock Doppler */

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
  /* Source again, continuing after the segment bits rather than renumbering
     them: the bit index is the draw's stream selector (wfm_draw_range), so
     moving one would change every drawn value in every existing scene. */
  WFM_RANGE_DOPPLER      = 1u << 7, /* source.doppler → [lo, doppler_hi] */
  WFM_RANGE_DOPPLER_RATE = 1u << 8, /* source.doppler_rate → [lo, hi]    */
};

typedef enum
{
  WFM_DOPPLER_PER_INSTANCE = 0,
  WFM_DOPPLER_PERSIST      = 1,
} wfm_doppler_lifetime_t;

typedef enum
{
  WFM_SNR_AUTO = 0, /* the type's own convention (esno for modulated) */
  WFM_SNR_FS   = 1, /* against the noise in the WHOLE sampled band    */
  WFM_SNR_EBNO = 2, /* per information bit                           */
  WFM_SNR_ESNO = 3, /* per transmitted symbol                        */
} wfm_snr_mode_t;

typedef enum
{
  WFM_BITMOD_NONE = 0, /* the payload is not modulated */
  WFM_BITMOD_BPSK = 1,
  WFM_BITMOD_QPSK = 2,
} wfm_bitmod_t;

typedef struct {
    int type;          /* WFM_SYNTH_TONE … WFM_SYNTH_BITS */
    double freq;       /* freq offset (Hz); chirp: start frequency f_start */
    double snr;        /* dB, per snr_mode */
    int snr_mode;      /* a wfm_snr_mode_t */
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
    /* The payload, as a SEQUENCE like its three siblings below rather than
       a bare array. A literal keeps its bits at `payload.bits`/`payload.len`
       exactly as `bits`/`n_bits` did; a GENERATED payload (PN/Gold/Dotted)
       carries its parameters instead, which is what lets a 100k-bit frame be
       six numbers in a --record. The bridge used to flatten this to
       WFM_SEQ_LITERAL on the way to the descriptor -- the same copy that
       made the preamble's generated kinds unreachable (gh-762). */
    wfm_seq_t payload; /* type=bits: pattern; type=dsss: frame payload */
    int modulation;    /* type=bits: a wfm_bitmod_t */
    float _Complex *symbols; /* type=symbols: stream, owned; NULL otherwise */
    size_t n_symbols;        /* type=symbols: stream length */
    int pulse;         /* pn/bpsk/qpsk pulse shape: 0 rect, 1 rrc */
    double rrc_beta;   /* RRC roll-off (pulse=rrc) */
    int rrc_span;      /* RRC support in symbols (pulse=rrc) */
    unsigned ranged;   /* WFM_RANGE_{FREQ,SNR,LEVEL,FEND,DOPPLER*} bitmask */
    double freq_hi;    /* upper bound when WFM_RANGE_FREQ is set */
    double snr_hi;     /* upper bound when WFM_RANGE_SNR is set */
    double level_hi;   /* upper bound when WFM_RANGE_LEVEL is set */
    double f_end_hi;   /* upper bound when WFM_RANGE_FEND is set */
    /* CLOCK DOPPLER, per source rather than per segment: it is a property of
       one emitter's motion, and two transmitters in a `sum` segment are on
       different geometries. `freq` cannot express it -- an offset moves the
       carrier alone, while Doppler rescales the whole received time base, so
       the symbol and chip rates move with it and a timing loop sees the error
       a carrier-only offset hides.

       Zero `doppler` AND zero `doppler_rate` means no channel is built at
       all, so a scene that does not ask for Doppler renders through exactly
       the code it always did. */
    double doppler;      /* ppm; time-base scale is 1 + doppler*1e-6 */
    double doppler_rate; /* ppm/s; linear ramp on `doppler` */
    double carrier_hz;   /* RF carrier the ppm is referred to, for the
                            coherent carrier term (0 = no carrier rotation) */
    double doppler_hi;      /* upper bound when WFM_RANGE_DOPPLER is set */
    double doppler_rate_hi; /* upper bound when WFM_RANGE_DOPPLER_RATE */
    int doppler_lifetime;   /* a wfm_doppler_lifetime_t */
    /* A frame the CALLER built, and the answer to "what frame is this?"
       when it is set. The flat framing and coding fields below stay, as
       SUGAR that builds one of these -- so every existing scene, flag and
       JSON key keeps working -- but a description says things they cannot:
       a field of the caller's own bits at a position of their choosing, a
       stage covering a span they name, an arrangement no flag spells.

       Borrowed, never owned: the description points at the caller's
       sequences exactly as `wfm_seq_t` is borrowed elsewhere here, so it
       must outlive the source. NULL means "derive one from the fields
       below", which is what every source did before this existed.

       KERNELS stay in C by design. A description names a stage's KIND; the
       code that runs it is a `wfm_frame_ops_t` entry, and a caller adding a
       genuinely new transform (convolutional interleaving, say) writes that
       kernel in C and hands it to `wfm_frame_assemble` directly. */
    const wfm_frame_desc_t *frame;

    /* type=dsss: the two-code burst geometry (wfm_frame_dsss_chips). The
       payload bits ride the shared `bits` field above (alias "payload"). */
    /* The three sequences a framed source carries. `wfm_seq_t` already names
       "a run of bits, however produced" -- LITERAL plus the generated PN /
       GOLD / DOTTED kinds and their parameters -- so carrying it here is
       what lets a face spell a generated one (gh-762). The literal case is
       `kind = WFM_SEQ_LITERAL`, which is what every one of these was before,
       so today's callers describe exactly what they described.

       OWNERSHIP: a source OWNS its `.bits`. `wfm_seq_t` declares them
       `const uint8_t *` because a frame DESCRIPTOR borrows them, and the
       borrowing consumer is the common one; the owner casts to free. */
    wfm_seq_t acq_code;  /* preamble code (0/1); len 0 = no preamble */
    size_t acq_reps;     /* preamble repetitions */
    wfm_seq_t data_code; /* payload spreading code; len = spreading factor */
    wfm_seq_t sync;      /* frame-sync word bits; len 0 = none */
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
    unsigned interleave_depth;      /* block interleaver over the data group;
                            0 = none. LAST of the data-group stages, so it is
                            what the channel sees: an interleaver exists to
                            make a burst arrive spread across the outer
                            code's codewords, so anything between it and the
                            wire would undo the point. */
    unsigned interleave_unit_bits;  /* bits per permuted unit; 0 reads as 1.
                            Match it to the outer code's symbol -- 8 for RS
                            over GF(256). Permuting BITS inside a symbol that
                            is already wrong buys nothing. */
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

typedef struct {
    size_t seg;      /* segment index in the spec                        */
    size_t instance; /* repeats instance, 0-based                        */
    size_t src;      /* source index within the segment                  */
    size_t start;    /* absolute sample index where the instance begins  */
    size_t delay;    /* leading gap length (samples)                     */
    size_t on;       /* on-time length (samples)                         */
    size_t off;      /* trailing gap length (samples)                    */
    double freq;     /* DRAWN carrier / sweep-start offset, Hz           */
    double f_end;    /* DRAWN sweep-end offset, Hz (chirp)               */
    double snr;      /* DRAWN SNR, dB, in the source's own snr_mode      */
    double level;    /* DRAWN level, dB                                  */
    /* Clock Doppler is DRAWN like the four above, so it is reported like
       them. A ranged `doppler` recorded in the spec is a span; what this
       instance actually flew is only ever knowable here. */
    double doppler;      /* DRAWN clock Doppler, ppm                     */
    double doppler_rate; /* DRAWN Doppler rate, ppm/s                    */
} wfm_draw_t;

size_t wfm_compose_draws(const wfm_segment_t *segs, size_t n_segs,
                         wfm_draw_t *out, size_t cap);

char *wfm_draws_json(const wfm_segment_t *segs, size_t n_segs);

int wfm_resolve_noise(wfm_segment_t *segs, size_t n);

double wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf,
                       double sym_span, double snr);

double wfm_source_create_snr(const wfm_source_t *src, double fs, double snr,
                             int *snr_mode);

int wfm_source_attach_dsss(wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs);

int wfm_source_has_frame(const wfm_source_t *src);

int wfm_source_describe_frame(const wfm_source_t *src, wfm_frame_desc_t *d);

size_t wfm_source_dsss_nchips(const wfm_source_t *src);

const char *wfm_source_frame_error(const wfm_source_t *src);

int wfm_source_attach_frame(wfm_synth_state_t *syn, const wfm_source_t *src);

wfm_synth_state_t *wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance,
                                           size_t instance);

typedef struct wfm_render wfm_render_t;

wfm_render_t *wfm_compose_build_render(const wfm_source_t *src, double fs,
                                       size_t on_len, double freq, double snr,
                                       double f_end, double doppler,
                                       double doppler_rate, unsigned epoch,
                                       int seed_advance, size_t instance,
                                       doppler_channel_state_t *borrow);

void wfm_render_steps(wfm_render_t *r, float _Complex *dst, size_t n);

void wfm_render_noise_steps(wfm_render_t *r, float _Complex *dst, size_t n);

void wfm_render_destroy(wfm_render_t *r);

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

int wfm_compose_seed_advance(const wfm_compose_state_t *state);

size_t wfm_compose_execute(
    wfm_compose_state_t *state, float _Complex *out, size_t max);

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
                       int continuous, int seed_advance, double headroom);

double wfm_spec_headroom(const char *json);

char *wfm_spec_template_json(void);

wfm_compose_state_t *wfm_compose_from_json(const char *json);

wfm_compose_state_t *wfm_compose_from_json_why(const char *json,
                                               const char **why);

wfm_compose_state_t *wfm_compose_from_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
```


