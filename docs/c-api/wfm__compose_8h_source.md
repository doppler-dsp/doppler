

# File wfm\_compose.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_compose.h**](wfm__compose_8h.md)

[Go to the documentation of this file](wfm__compose_8h.md)


```C++

#ifndef WFM_COMPOSE_H
#define WFM_COMPOSE_H

#include "doppler/clib_common.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "doppler/wfm/wfm_frame.h" /* wfm_frame_desc_t — a source's frame, described */
#include "doppler/wfm/wfm_data.h" /* a data:LEN payload's source */
#include "doppler/doppler_channel/doppler_channel_core.h" /* a source's clock Doppler */

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
     them: the bit index is the draw's stream selector (dp_wfm_draw_range), so
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
    int type;          /* Waveform type. tone: complex sinusoid. noise:
                          AWGN. pn: PN sequence (LFSR). bpsk/qpsk: PN-driven
                          modulation. chirp: linear-FM sweep. bits: a
                          caller's bit pattern, with selectable modulation.
                          symbols: a caller's complex constellation stream.
                          dsss: spread spectrum -- a two-code burst
                          (repeated preamble + data-code-spread frame) by
                          default, or a continuous asynchronous stream when
                          symbol_rate is set. */
    double freq;       /* Carrier or offset frequency in Hz; for chirp, the
                          sweep start. With fs = 1 it is in normalised
                          cycles per sample. */
    double snr;        /* Signal-to-noise ratio in dB, interpreted per
                          snr_mode. 100 or more is clean: no AWGN is added. */
    int snr_mode;      /* How snr is interpreted. auto: Es/N0 for
                          bpsk/qpsk/dsss, and relative to full scale for
                          tone/noise/pn/chirp/bits/symbols -- bits included,
                          because a bits frame has no symbol rate the engine
                          can infer. fs: dB relative to full scale. ebno:
                          Eb/N0, per bit. esno: Es/N0, per symbol; for a
                          dsss burst the outer data symbol of len(data_code)
                          chips x sps samples, for a continuous dsss stream
                          the fs/symbol_rate samples the async symbol spans. */
    uint32_t seed;     /* PRNG and LFSR seed for the noise and PN streams.
                          Deterministic: vary it for run-to-run change. For
                          a PN-sourced type it starts the pn_length-bit
                          register, masked: a seed whose low pn_length bits
                          are zero starts the register at 1, as seed 0
                          does. */
    int sps;           /* Samples per symbol (PSK) or per chip (PN): the
                          oversampling factor. Unused by noise, which
                          records it as 0. */
    int pn_length;     /* PN LFSR register length in bits, 2 to 64 for the
                          PN-bearing types; the sequence period is
                          2^pn_length - 1. Unused by noise, which records it
                          as 0. */
    uint64_t pn_poly;  /* PN generator polynomial, in the Galois bit-vector
                          convention; 0 selects a maximal-length (MLS)
                          polynomial for pn_length. It must fit the
                          pn_length-bit register: a bit at or above it is
                          refused, not masked. A polynomial above 2^53
                          does not survive a JSON number, so a scene file
                          needs 0 (auto) for such a register. */
    int lfsr;          /* PN LFSR realisation. Both give the same period;
                          fibonacci's chips are galois's in reverse order. */
    double level;      /* Source power in dBFS (<= 0; 0 is unit power).
                          Applies when summed in a Segment or Composer, as a
                          gain of 10^(level/20); a standalone Synth.steps()
                          ignores it. */
    int background;    /* Mark this source as part of the static background
                          field (0/1). Plan.prepare() folds a contiguous
                          leading run of background sources into ONE
                          pre-summed cache entry instead of caching each
                          separately, so a scene of many fixed emitters
                          costs one buffer rather than hundreds. The
                          composite is overridable as a unit: it takes a
                          single slot in gains/phases/enable and counts as
                          one in n_sources(), so scaling it trims the whole
                          field while its members keep their relative
                          levels. Background sources must come first in the
                          segment (a non-prefix ordering is rejected by
                          prepare, since the fold would no longer reproduce
                          compose bit-for-bit). Ignored by compose() and by
                          standalone Synth.steps(). */
    double f_end;      /* Chirp end frequency in Hz; ignored by other types. */
    size_t span;       /* Chirp sweep length in samples: the frequency ramps
                          from freq to f_end over this many samples, then
                          holds at f_end. 0 means the enclosing Segment's
                          num_samples. A standalone chirp (f_end != freq)
                          must declare it, so step(), steps(N) and any
                          chunking of reads produce the same waveform;
                          generating one without it raises. Ignored by
                          non-chirp types. */
    int modulation;    /* Symbol mapping of a bits pattern. none: the
                          pattern shaped and output as-is (NRZ). bpsk: +/-1
                          symbols. qpsk: Gray-coded symbols from pairs of
                          bits. */
    float _Complex *symbols; /* For type=symbols: a complex constellation
                                stream. Each element is the output point
                                itself, oversampled by sps, cycled, and
                                RRC-shaped with pulse=rrc, which generalises
                                any modulation (pi/4-QPSK, QAM, ...). */
    size_t n_symbols;        /* type=symbols: stream length */
    int pulse;         /* Pulse shape per symbol or chip, for
                          pn/bpsk/qpsk/bits/symbols/dsss. rect: rectangular,
                          no ISI filtering. rrc: root-raised cosine; see
                          rrc_beta and rrc_span. */
    double rrc_beta;   /* RRC roll-off factor, in (0, 1], when pulse=rrc. */
    int rrc_span;      /* RRC filter support in symbols when pulse=rrc,
                          ONE-SIDED: the filter has 2*rrc_span*sps + 1 taps,
                          unit energy (sum of h^2 = 1). */
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
    double doppler;      /* Clock Doppler in ppm: the received time base is
                            rescaled by 1 + doppler*1e-6, so the symbol and
                            chip rates move with the carrier and a timing
                            loop sees the error a carrier-only `freq` offset
                            hides. Accepts a (lo, hi) tuple drawn uniformly
                            per repeat, like freq/snr. Zero doppler AND zero
                            doppler_rate means no channel is built at all,
                            so a source that does not ask for Doppler
                            renders exactly as it always did. */
    double doppler_rate; /* Linear ramp on `doppler`, in ppm per second of
                            elapsed stream time. The channel runs through a
                            segment's gaps as well as its on-time -- an
                            emitter does not stop moving because its burst
                            ended -- so this is per second, not per second
                            of on-time. Accepts a (lo, hi) tuple drawn
                            uniformly per repeat. */
    double carrier_hz;   /* RF carrier in Hz that the ppm figures are
                            referred to. It gives the coherent carrier
                            rotation that accompanies the time-base warp; 0
                            warps the clock alone, with no carrier rotation
                            -- a legitimate scene, not an unset field.
                            Independent of doppler/doppler_rate. */
    double doppler_hi;      /* upper bound when WFM_RANGE_DOPPLER is set */
    double doppler_rate_hi; /* upper bound when WFM_RANGE_DOPPLER_RATE */
    int doppler_lifetime;   /* How long this source's Doppler channel lives.
                               per_instance: the channel dies with each
                               `repeats` instance, so the geometry restarts
                               -- the repeated-trial shape, which composes
                               with a ranged doppler re-drawn per instance.
                               persist: one continuous pass carries across
                               the segment's gaps and repeat instances,
                               keyed by (segment, source) position -- the
                               only lifetime under which doppler_rate
                               accumulates across a multi-burst scene.
                               Plan.prepare() REFUSES a persist source,
                               because its cache renders each source
                               independently and concurrently; compose() and
                               stream() honour both. */
    /* A frame DESCRIPTION, the whole frame: fields in wire order, and
       stages that each name the span they cover (crc16, rs, randomise,
       interleave, conv, or a kind of your own). It is the only way a source
       says anything but the common frame: a coding stage, a field of the
       caller's own bits at a position of their choosing, a stage covering a
       span they name. `wfmgen --frame FILE`, a scene's
       `frame` key and Python's `frame=` (a FrameDesc or a Frame) all land
       here. When it is set it IS the frame, and an unspread acq_code
       beside it is refused. NULL means the common frame, `[preamble x
       reps | data]`, which `dp_wfm_frame_fixed()` builds from acq_code and
       the data source; a sync word or a CRC is a field or a stage of a
       description, never a flat field.

       A C caller's description is borrowed, exactly as `wfm_seq_t` is
       borrowed elsewhere here, so it must outlive the source. The composer
       and a Python source hold their own copy (`dp_wfm_frame_copy()`), so
       a later change to the FrameDesc does not reach them. On the Python
       face `frame=` is an input: read it back from the composer's JSON
       (its getter is jm's, pending removal: doppler#1694).

       KERNELS stay in C by design. A description names a stage's KIND; the
       code that runs it is a `wfm_frame_ops_t` entry, and a caller adding a
       genuinely new transform (convolutional interleaving, say) writes that
       kernel in C and hands it to `dp_wfm_frame_assemble` directly. */
    const wfm_frame_desc_t *frame;

    /* type=dsss: the two-code burst geometry (dp_wfm_dsss_desc_chips). The
       payload is a data source (`data` / `data_from_file`, below). */
    /* The three sequences a framed source carries. `wfm_seq_t` already names
       "a run of bits, however produced" -- LITERAL plus the generated PN /
       GOLD / DOTTED kinds and their parameters -- so carrying it here is
       what lets a face spell a generated one (gh-762). The literal case is
       `kind = WFM_SEQ_LITERAL`, which is what every one of these was before,
       so today's callers describe exactly what they described.

       OWNERSHIP: a source OWNS its `.bits`. `wfm_seq_t` declares them
       `const uint8_t *` because a frame DESCRIPTOR borrows them, and the
       borrowing consumer is the common one; the owner casts to free. */
    wfm_seq_t acq_code;  /* The preamble code, sent acq_reps times at the
                            head of the frame (a Field's *REPS on the
                            command line and in a scene) -- the coherent
                            pull-in target BurstDespreader.set_acq and
                            BurstDemod.set_preamble lock to. For type=dsss
                            it is unmodulated chips ahead of the spread
                            frame; for type=bits it is the head of the bit
                            pattern. Setting it is what makes a source
                            FRAMED. */
    size_t acq_reps;     /* Preamble repetitions: periods of acq_code before
                            the sync word. On the command line and in a
                            scene it is acq_code's *REPS. */
    wfm_seq_t data_code; /* For type=dsss: the payload spreading code, a
                            second code distinct from acq_code. Every frame
                            bit (sync, payload, crc) is XOR-spread across
                            its full length, so len(data_code) is the
                            spreading factor. */
    /* type=dsss, CONTINUOUS mode: a data-symbol rate independent of the code
       epoch rate selects the continuous form (dp_wfm_synth_set_dsss_cont) over
       the burst form above -- one waveform type, one discriminator, rather
       than a tenth entry in five hand-maintained name tables. 0 = burst.
       The frame fields (acq_code/frame/data) are meaningless when this is
       set and are rejected by the caller rather than silently ignored. */
    double symbol_rate;  /* For type=dsss: > 0 selects CONTINUOUS
                            asynchronous mode. The spreading code repeats
                            endlessly and data rides on it at this symbol
                            rate (Hz), independent of the code-epoch rate
                            (chips/symbol = fs/sps/symbol_rate,
                            non-integer). No preamble/sync/CRC frame; data
                            comes from the payload when supplied, else a
                            seeded PN a receiver regenerates. Absent/0 =
                            burst. */
    int dsss_code_only;  /* Continuous dsss data source: 1 = code-only (the
                            pure spreading code, no data modulation); 0 =
                            data-modulated (the payload when supplied, else
                            the seeded PN). Ignored for burst dsss and
                            non-dsss types. */
    /* The data source a frame's payload is drawn from, `data_len` bits per
       frame (docs/design/payload-data-source.md; wfm/wfm_data.h is the one
       table of what a source is). It is EITHER `data` (a Field, or a bit
       array in Python) OR `data_from_file` (a file, or `-` for stdin):
       one declaration makes the pair exclusive on every face. */
    wfm_seq_t data;    /* A frame's payload drawn from a data source: a
                          Field on the command line and in a scene, a bit
                          array in Python. The source is split into
                          data_len-bit frames, one chunk per frame, and its
                          last chunk is padded from fill. For type=bits,
                          bpsk/qpsk/pn framed, a dsss burst (one burst per
                          frame), and continuous dsss (one bit per data
                          symbol, no frame); not with data_from_file. */
    size_t data_len;   /* Bits of the data source per frame: the data:LEN
                          of the common frame [preamble x reps | sync |
                          data:LEN | crc]. 0 takes a finite source whole, as
                          one frame. A carried frame names its own data
                          field, and this is then 0 or that field's LEN.
                          Continuous dsss has no frame, and refuses it. */
    wfm_seq_t fill;    /* The bits that pad a data source's last frame when
                          it does not divide into data_len-bit frames, tiled
                          from their first bit; stdin on a framed source
                          always needs them. Without them such a source is
                          refused before the first sample. A Field on the
                          command line and in a scene, a bit array in
                          Python. Continuous dsss has no frame to pad, and
                          refuses it. */
    const char *data_from_file; /* A data source read from a file of packed
                          octets, MSB first, or `-` for stdin, instead of
                          data. Not on the Python face: there a file is
                          cvt.bytes_to_bin of its bytes, passed as data
                          (payload-data-source.md section 4.9). Borrowed,
                          so it must outlive the source. */
    wfm_seq_t retired_bits; /* RETIRED (doppler#1718): nothing reads it but
                          the refusal. A payload is drawn from a data
                          source, so bits=, and its aliases payload= and
                          pattern=, are refused naming data=; the CLI and a
                          scene refuse --bits and "payload" the same way. */
    wfm_seq_t retired_sync; /* RETIRED (doppler#1617): nothing reads it but
                          the refusal. The frame-sync word is a field of the
                          frame DESCRIPTION, so sync= is refused naming
                          frame=; the CLI's --sync builds that description
                          for you, and a scene refuses the "sync" key. */
    wfm_seq_t retired_crc; /* RETIRED (doppler#1617): nothing reads it but
                          the refusal. A CRC is a stage of the frame
                          DESCRIPTION, so crc= is refused naming frame=,
                          whatever its value (crc="none" included); the
                          CLI's --crc builds that description for you, and
                          a scene refuses the "crc" key. */
    wfm_data_stats_t data_sent; /* What this source's data sent in the
                          composer's latest instance: its frames, the fill
                          bits padding the last, its idle frames, the source
                          bits read and, for data_from_file, the dp_hash64
                          of the octets read. The truth --record and SigMF
                          carry (payload-data-source.md section 4.8), kept
                          by the composer, which clears it at create; zero
                          until an instance has sent a frame. Not an input:
                          no face sets it. */
} wfm_source_t;

typedef struct {
    wfm_source_t *sources; /* n_sources sources summed at the same time */
    size_t n_sources;
    double fs;             /* Sample rate in Hz, one per segment and shared
                              by all its sources. At the default 1.0 every
                              frequency is normalised (cycles per sample);
                              state it whenever a scene is in real Hz. */
    size_t num_samples;    /* Segment on-time in samples, before the
                              trailing gap: 0 derives it from the sources,
                              or 1024 when they set none. A finite data
                              source sets its frames, a lone dsss burst one
                              burst, and a stream runs to its end. A count beside a finite
                              data source or a lone dsss burst is refused,
                              since they set the length; give repeats for
                              more. */
    size_t off_samples;    /* Trailing gap after the on-time, in samples. It
                              carries the noise floor or hard zeros, per
                              gap_noise. */
    unsigned ranged;       /* WFM_RANGE_{NUM,OFF}_SAMPLES bitmask */
    size_t num_samples_hi; /* upper bound when WFM_RANGE_NUM_SAMPLES is set */
    size_t off_samples_hi; /* upper bound when WFM_RANGE_OFF_SAMPLES is set */
    /* Play the segment this many times back-to-back (each instance = delay
       + on-time + trailing gap) before advancing. Ranged fields re-draw and
       the AWGN is fresh per instance; the signal (codes, payload, PN phase)
       stays fixed. 0 and 1 both mean one instance. */
    size_t repeats;
    /* Leading gap before the on-time, in samples: the burst arrives after
       this delay, and the gap carries the noise floor like off_samples.
       Ranged like off_samples and re-drawn per repeats instance, so a (lo,
       hi) delay is per-burst arrival jitter. Use off_samples for
       inter-burst spacing, delay_samples for arrival jitter. */
    size_t delay_samples;
    size_t delay_samples_hi; /* upper bound when WFM_RANGE_DELAY_SAMPLES */
    /* Gap policy for this segment's delay and trailing gap. auto: gaps
       carry the segment's noise floor -- the sources' AWGN keeps running
       while the signal stops (clean scenes still get exact-zero gaps). off:
       gaps are hard zeros. */
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

size_t dp_wfm_compose_spans(const wfm_segment_t *segs, size_t n_segs,
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

size_t dp_wfm_compose_draws(const wfm_segment_t *segs, size_t n_segs,
                         wfm_draw_t *out, size_t cap);

char *dp_wfm_draws_json(const wfm_segment_t *segs, size_t n_segs);

int dp_wfm_resolve_noise(wfm_segment_t *segs, size_t n);

double dp_wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf,
                       double sym_span, double snr);

double dp_wfm_source_create_snr(const wfm_source_t *src, double fs, double snr,
                             int *snr_mode);

int dp_wfm_source_attach_dsss(dp_wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs);

int dp_wfm_source_has_frame(const wfm_source_t *src);

size_t dp_wfm_source_dsss_nchips(const wfm_source_t *src);

double dp_wfm_source_dsss_cps(const wfm_source_t *src, double fs);

const char *dp_wfm_source_frame_error(const wfm_source_t *src);

const char *dp_wfm_source_error(const wfm_source_t *src);

extern const char dp_wfm_why_pn_poly[];

extern const char dp_wfm_why_dsss_frame_no_data_code[];
extern const char dp_wfm_why_dsss_empty[];
extern const char dp_wfm_why_dsss_cont_no_data_code[];

extern const char dp_wfm_why_retired_bits[];

extern const char dp_wfm_why_retired_sync[];
extern const char dp_wfm_why_retired_crc[];

const char *dp_wfm_source_to_synth_error(const wfm_source_t *src, double fs);

int dp_wfm_source_attach_frame(dp_wfm_synth_state_t *syn, const wfm_source_t *src);

int dp_wfm_synth_attach_data(dp_wfm_synth_state_t *syn, const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, wfm_data_src_t *src,
                             wfm_data_pacing_t pacing, int modulation);

void dp_wfm_synth_set_data_pacing(dp_wfm_synth_state_t *syn,
                                  wfm_data_pacing_t pacing);

const char *dp_wfm_scene_error(const wfm_segment_t *segs, size_t n_segs,
                               int repeat, int continuous);

extern const char dp_wfm_why_dsss_cont_rate[];

#define WFM_NUM_SAMPLES_PLAIN ((size_t)1024)

int dp_wfm_segment_sets_length(const wfm_segment_t *seg);

extern const char dp_wfm_why_count_derived[];


int dp_wfm_source_data_is_stream(const wfm_source_t *src);

uint64_t dp_wfm_source_data_samples(const wfm_source_t *src, double fs,
                                    uint64_t frames);

uint64_t dp_wfm_source_data_frames(const wfm_source_t *src);

const wfm_data_src_t *dp_wfm_synth_data_source(const dp_wfm_synth_state_t *syn);

int dp_wfm_source_synth_type(const wfm_source_t *src);

dp_wfm_synth_state_t *dp_wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance,
                                           size_t instance);

typedef struct wfm_render wfm_render_t;

wfm_render_t *dp_wfm_compose_build_render(const wfm_source_t *src, double fs,
                                       size_t on_len, double freq, double snr,
                                       double f_end, double doppler,
                                       double doppler_rate, unsigned epoch,
                                       int seed_advance, size_t instance,
                                       dp_doppler_channel_state_t *borrow);

void dp_wfm_render_steps(wfm_render_t *r, float _Complex *dst, size_t n);

void dp_wfm_render_noise_steps(wfm_render_t *r, float _Complex *dst, size_t n);

void dp_wfm_render_destroy(wfm_render_t *r);

typedef enum
{
  WFM_SEED_ADVANCE_NONE  = 0, /* byte-identical repeats (default) */
  WFM_SEED_ADVANCE_NOISE = 1, /* signal fixed, AWGN fresh per repeat */
  WFM_SEED_ADVANCE_ALL   = 2, /* whole seed advances (code+data+noise) */
} wfm_seed_advance_t;

typedef struct wfm_compose_state dp_wfm_compose_state_t;

dp_wfm_compose_state_t *dp_wfm_compose_create(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

dp_wfm_compose_state_t *dp_wfm_compose_create_why(const wfm_segment_t *segs,
                                                  size_t n_segs, int repeat,
                                                  int continuous,
                                                  const char **why);

void dp_wfm_compose_set_seed_advance(dp_wfm_compose_state_t *state, int mode);

void dp_wfm_compose_set_data_pacing(dp_wfm_compose_state_t *state,
                                    wfm_data_pacing_t pacing);

int dp_wfm_compose_seed_advance(const dp_wfm_compose_state_t *state);

size_t dp_wfm_compose_execute(
    dp_wfm_compose_state_t *state, float _Complex *out, size_t max);

size_t dp_wfm_compose_execute_rate(dp_wfm_compose_state_t *state,
                                   float _Complex *out, size_t max,
                                   double *fs);

static inline double dp_wfm_scene_fs(const wfm_segment_t *segs,
                                     size_t n_segs)
{
    if (!segs || n_segs == 0)
        return 0.0;
    for (size_t i = 1; i < n_segs; i++)
        if (segs[i].fs != segs[0].fs)
            return 0.0;
    return segs[0].fs;
}

void dp_wfm_compose_destroy(dp_wfm_compose_state_t *state);

const wfm_segment_t *dp_wfm_compose_segments(const dp_wfm_compose_state_t *state,
                                          size_t *n_out, int *repeat,
                                          int *continuous);

/* ── JSON spec (the shared --from-file / --record format) ─────────────────── */
/*
 * Canonical schema: docs/schema/wfmgen.schema.json (JSON Schema 2020-12).
 * A recorded run reproduces byte-for-byte when fed back via --from-file.
 * Use `wfmgen json-template` for a ready-to-edit example covering all fields.
 */

char *dp_wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                       int continuous, int seed_advance, double headroom);

double dp_wfm_spec_headroom(const char *json);

char *dp_wfm_spec_template_json(void);

wfm_frame_desc_t *dp_wfm_frame_from_json(const char *json, const char **why);

void dp_wfm_frame_free(wfm_frame_desc_t *d);

size_t dp_wfm_source_sync_refuse_text(const char *text, uint8_t *out,
                                      size_t max_out, const char **why);

size_t dp_wfm_source_crc_refuse_text(const char *text, uint8_t *out,
                                     size_t max_out, const char **why);

const char *dp_wfm_framing_flags_error(const wfm_source_t *src);

int dp_wfm_source_common_frame(const wfm_source_t *src, const wfm_seq_t *sync,
                               int crc, wfm_frame_desc_t *d);

size_t dp_wfm_source_bits_refuse_text(const char *text, uint8_t *out,
                                      size_t max_out, const char **why);

wfm_frame_desc_t *dp_wfm_frame_refuse_text(const char *text, const char **why);

char *dp_wfm_frame_to_json(const wfm_frame_desc_t *d);

wfm_frame_desc_t *dp_wfm_frame_copy(const wfm_frame_desc_t *d);

dp_wfm_compose_state_t *dp_wfm_compose_from_json(const char *json);

dp_wfm_compose_state_t *dp_wfm_compose_from_json_why(const char *json,
                                               const char **why);

dp_wfm_compose_state_t *dp_wfm_compose_from_json_at(const char *json,
                                                    const char *base,
                                                    const char **why);

dp_wfm_compose_state_t *dp_wfm_compose_from_json_data(const char *json,
                                                      const char *base,
                                                      const char *data_file,
                                                      const char **why);

dp_wfm_compose_state_t *dp_wfm_compose_from_file(const char *path);

dp_wfm_compose_state_t *dp_wfm_compose_from_file_why(const char *path,
                                                     const char **why);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
```


