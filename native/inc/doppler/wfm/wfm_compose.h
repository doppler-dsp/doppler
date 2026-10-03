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
 * Lifecycle: dp_wfm_compose_create -> dp_wfm_compose_execute* -> dp_wfm_compose_destroy
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
 * dp_wfm_compose_state_t *c = dp_wfm_compose_create(segs, 2, 0, 0);
 * float _Complex buf[4096];
 * size_t n;
 * while ((n = dp_wfm_compose_execute(c, buf, 4096)) > 0) { ... }
 * dp_wfm_compose_destroy(c);
 * @endcode
 */
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
 * the same sequence byte-for-byte. Bits 0–3 and 7–8 live on
 * `wfm_source_t.ranged`; bits 4–6 on `wfm_segment_t.ranged`.
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
  /* Source again, continuing after the segment bits rather than renumbering
     them: the bit index is the draw's stream selector (dp_wfm_draw_range), so
     moving one would change every drawn value in every existing scene. */
  WFM_RANGE_DOPPLER      = 1u << 7, /* source.doppler → [lo, doppler_hi] */
  WFM_RANGE_DOPPLER_RATE = 1u << 8, /* source.doppler_rate → [lo, hi]    */
};

/**
 * @brief When a source's Doppler channel restarts.
 *
 * Neither is a superset of the other, so it is declared rather than defaulted
 * into an argument:
 *
 * - `PER_INSTANCE` (default) restarts the geometry for every burst instance,
 *   which is the repeated-trial shape — every burst sees the same pass, and
 *   it composes with the per-instance re-draw of a ranged `doppler`.
 * - `PERSIST` carries one emitter's motion across every REPEAT INSTANCE of
 *   its segment, and across the gaps between them, so burst *k* sees where
 *   the pass has got to. It is the only lifetime under which `doppler_rate`
 *   means anything over a multi-burst scene.
 *
 * The channel is keyed by (segment, source), because that is the only source
 * identity the composer has — a position. So a PERSIST source persists over
 * its own segment's instances; two DIFFERENT segments each get their own
 * pass, even where a reader might call them the same emitter. Sharing one
 * across segments needs a declared source id, which nothing in the scene
 * format carries yet; gh-942 says as much ("no per-source identity that
 * survives it ... the repeats/epoch machinery is where one would hang").
 */
typedef enum
{
  WFM_DOPPLER_PER_INSTANCE = 0,
  WFM_DOPPLER_PERSIST      = 1,
} wfm_doppler_lifetime_t;

/**
 * @brief What a source's `snr` is measured against.
 *
 * The scale a number in dB is quoted on is not a detail a caller can infer,
 * and it changes the noise by 10log10(sps) between `fs` and `esno`. Naming
 * the modes is what lets a downstream write the mode it means instead of a
 * literal whose meaning lives in a comment. Order IS the wire value — the
 * `[[enum]] snr_mode` manifest and `MODE_NAMES[]` in wfm_names.h are held to
 * this by `make lint-wfm-enum-tables`.
 */
typedef enum
{
  WFM_SNR_AUTO = 0, /* the type's own convention (esno for modulated) */
  WFM_SNR_FS   = 1, /* against the noise in the WHOLE sampled band    */
  WFM_SNR_EBNO = 2, /* per information bit                           */
  WFM_SNR_ESNO = 3, /* per transmitted symbol                        */
} wfm_snr_mode_t;

/**
 * @brief How a `WFM_SYNTH_BITS` source maps its payload to symbols.
 *
 * Order IS the wire value; `BITMOD_NAMES[]` and the `[[enum]] bitmod`
 * manifest are held to this by `make lint-wfm-enum-tables`.
 */
typedef enum
{
  WFM_BITMOD_NONE = 0, /* the payload is not modulated */
  WFM_BITMOD_BPSK = 1,
  WFM_BITMOD_QPSK = 2,
} wfm_bitmod_t;

/**
 * @brief One additive source within a segment: a `synth` config + its level.
 *
 * The nine synth fields mirror `dp_wfm_synth_create()` (minus `fs`, which is the
 * segment's — one receiver, one sample rate). `level` is the source's average
 * power in dBFS (≤0); the segment sums its sources, each scaled by
 * `10^(level/20)`.
 *
 * Any of `freq`/`snr`/`level`/`f_end` may be a per-repeat uniform draw: set the
 * matching `WFM_RANGE_*` bit in `ranged`, leave the scalar as `lo`, and put `hi`
 * in the `*_hi` companion (see the `ranged` enum).
 */
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

/**
 * @brief One rendered segment instance's exact timing: where it lands in the
 * composed stream and how its `delay | on | off` spans divide it.
 *
 * Produced by dp_wfm_compose_spans() — the deterministic replay of the ranged
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
 * then again with a buffer. Pass the RESOLVED segments (dp_wfm_compose_segments()
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
size_t dp_wfm_compose_spans(const wfm_segment_t *segs, size_t n_segs,
                         wfm_span_t *out, size_t cap);

/**
 * @brief One rendered source instance: its timing AND the values it was
 * actually rendered with.
 *
 * A `wfm_span_t` answers *when*; this answers *when and what*, for one
 * source of one instance. The distinction is not academic. The SigMF
 * sidecar used to build each annotation from two provenances -- timing
 * replayed through dp_wfm_compose_spans(), frequency and SNR read straight off
 * the source struct, which for a ranged field still holds `lo` -- so every
 * annotation of a `--freq 11200:12800 --snr 8:14` scene claimed 11200 Hz and
 * 8 dB beside a sample-accurate start. Measured against the capture itself:
 * up to 1224 Hz and 6.0 dB out (doppler#1086). A 6 dB error is a different
 * operating point, and nothing in the file revealed it.
 *
 * An un-ranged field reports its scalar, so a consumer never branches on the
 * `ranged` bitmask.
 *
 * The spec keeps storing `(lo, hi)`: replay is guaranteed by re-deriving the
 * draw hash, not by recording the draw. "What does this spec permit" and
 * "what did this run do" are different questions and one field cannot answer
 * both -- which is why this is a separate call rather than a resolved spec.
 */
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

/**
 * @brief Replay the (epoch 0) instance timeline AND its drawn source values.
 *
 * Same size-then-fill protocol as dp_wfm_compose_spans(): call once with `cap`
 * 0 to size, then again with a buffer. Emits one row per SOURCE per
 * instance, in stream order, because that is the granularity a per-source
 * annotation or a scoring pipeline needs. Pass the RESOLVED segments
 * (dp_wfm_compose_segments() on a live composer) so intrinsic on-times are
 * already folded in.
 *
 * The rows are produced by the same dp_wfm_draw_segment()/dp_wfm_draw_source()
 * calls the renderer resolves through, so a field added to the draw reaches
 * both by construction rather than by a reviewer noticing.
 *
 * @param segs   Resolved segment array.
 * @param n_segs Segment count.
 * @param out    Row buffer (may be NULL when cap is 0).
 * @param cap    Capacity of out in rows.
 * @return Total rows in one pass of the spec (sum of n_sources over
 *         instances), regardless of `cap`.
 */
size_t dp_wfm_compose_draws(const wfm_segment_t *segs, size_t n_segs,
                         wfm_draw_t *out, size_t cap);

/**
 * @brief The same rows dp_wfm_compose_draws() reports, as a JSON array.
 *
 * One object per source per instance, in stream order, with the keys named
 * after the `wfm_draw_t` fields. Exists so a binding can hand a caller its
 * GROUND TRUTH without marshalling a struct array itself: a ranged field is
 * only usable if what it drew can be read back, and scoring a receiver
 * against a scene whose `freq` re-draws per instance means scoring against a
 * number the caller does not otherwise have (doppler#1112).
 *
 * Reads through dp_wfm_compose_draws(), so it cannot disagree with the SigMF
 * annotations, which read through it too.
 *
 * @param segs   Resolved segment array (dp_wfm_compose_segments()).
 * @param n_segs Segment count.
 * @return Heap JSON string the caller free()s; never NULL — the allocations
 *         go through the abort-on-OOM helpers. A spec with no rows yields
 *         `[]`.
 *
 * @code
 * size_t n; int rp, ct;
 * const wfm_segment_t *segs = dp_wfm_compose_segments(c, &n, &rp, &ct);
 * char *js = dp_wfm_draws_json(segs, n);
 * puts(js);
 * free(js);
 * @endcode
 */
char *dp_wfm_draws_json(const wfm_segment_t *segs, size_t n_segs);

/**
 * @brief Resolve a segment list's noise model in place (Phase 4b).
 *
 * No-op for 1-source segments (keeps the bundled-synth path byte-identical).
 * For a multi-source segment it sets one shared noise floor (from an explicit
 * WFM_SYNTH_NOISE source, else the first snr-bearing source), cleans the signal
 * sources, and appends a WFM_SYNTH_NOISE source at the floor — so the composer's
 * accumulator just sums. May `realloc` each segment's `sources`. Idempotent.
 *
 * `dp_wfm_compose_create()` calls this on its private copy, so every face (CLI,
 * JSON, Python) resolves identically.
 *
 * @return 0 on success; -1 if a non-anchor source over-specifies (snr + level)
 *         or on allocation failure.
 */
int dp_wfm_resolve_noise(wfm_segment_t *segs, size_t n);

/**
 * @brief SNR (dB) referred to fs, from a source's snr/snr_mode/sps/type.
 *
 * The single source of truth for the Es/No, Eb/No, and over-fs conventions
 * (`snr_mode` 0 auto / 1 fs / 2 ebno / 3 esno). `dp_wfm_resolve_noise()` uses it to
 * place the shared noise floor at `level(anchor) − dp_wfm_snr_over_fs(anchor)`, and
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
double dp_wfm_snr_over_fs(int snr_mode, int type, int sps, size_t sf,
                       double sym_span, double snr);

/**
 * @brief Resolve a source's (snr, snr_mode) into the pair to hand to
 * `dp_wfm_synth_create()`.
 *
 * `dp_wfm_synth_create()` runs before a dsss source's codes are attached, so it
 * cannot know the spreading factor its own esno would need. This helper — the
 * one create-time entry point shared by the composer (`dp_wfm_compose_build_synth`)
 * and the standalone-Synth bridge (`dp_wfm_source_to_synth`), so every face agrees
 * to the bit — converts a dsss source's SNR to the over-fs reference (via
 * `dp_wfm_snr_over_fs`; the burst span is `sf = n_data_code`, a continuous stream
 * uses `fs/symbol_rate`) and returns `snr_mode=fs`. A framed `bpsk`/`qpsk`/`pn`
 * is referred the same way, because its synth is created as BITS
 * (dp_wfm_source_synth_type()). Every other source passes through unchanged.
 *
 * @param src      The source (supplies type/sps/snr_mode/n_data_code/
 *                 symbol_rate).
 * @param fs       Segment sample rate (Hz) — needed for a continuous dsss
 *                 source's `fs/symbol_rate` span; ignored otherwise.
 * @param snr      The declared SNR in dB, already ranged-resolved.
 * @param snr_mode Receives the snr_mode for create.
 * @return The SNR in dB for create.
 */
double dp_wfm_source_create_snr(const wfm_source_t *src, double fs, double snr,
                             int *snr_mode);

/**
 * @brief Attach a dsss source's data to a freshly-created synth.
 *
 * The single dsss-attach path, called by BOTH synth-construction faces
 * (`dp_wfm_compose_build_synth` and the standalone `dp_wfm_source_to_synth`), so the
 * two cannot drift on how a dsss stream is configured. Selects on
 * `symbol_rate`: 0 → the burst form (`dp_wfm_synth_set_dsss_chips`); > 0 → the
 * continuous form (`dp_wfm_synth_set_dsss_cont`) with `chips_per_symbol =
 * (fs/sps)/symbol_rate`, taking the data from the payload when one is supplied
 * (`bits`) and otherwise from the seeded PN. A no-op for a non-dsss source.
 *
 * @param syn  A synth from dp_wfm_synth_create() with `wtype == WFM_SYNTH_DSSS`.
 * @param src  The source (codes, payload, symbol_rate, pn config).
 * @param fs   Segment sample rate (Hz) — the continuous chip rate is fs/sps.
 * @return 0 on success (or non-dsss no-op); -1 on invalid geometry.
 */
int dp_wfm_source_attach_dsss(dp_wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs);

/**
 * @brief Non-zero when this source describes a FRAME.
 *
 * A carried description, a preamble or a data source is what says "framed".
 *
 * @param src  The source; NULL reads as unframed.
 */
int dp_wfm_source_has_frame(const wfm_source_t *src);

/**
 * @brief Chips one DSSS BURST from this source occupies, description and all.
 *
 * What sizes a lone dsss segment's intrinsic on-time. It reads the same
 * description the burst is assembled from, so a stage that lengthens the frame
 * -- a rate-1/2 inner code doubles it -- lengthens the segment by the same
 * arithmetic instead of by a second copy of it.
 *
 * @param src  the source.
 * @return burst chips, or 0 for a non-dsss source, a CONTINUOUS dsss source
 *         (which has no intrinsic length), or an empty/refused geometry.
 */
size_t dp_wfm_source_dsss_nchips(const wfm_source_t *src);

/**
 * @brief Chips per data symbol of a CONTINUOUS dsss source at @p fs.
 *
 * `sps` is samples per CHIP for dsss, so the chip rate is `fs / sps`, and
 * the data clock is `symbol_rate`: their ratio, non-integer in general,
 * which is the asynchronicity. It is the number the builder hands
 * dp_wfm_synth_set_dsss_cont(), and the one dp_wfm_scene_error() holds to
 * `>= 1` -- so the rule and the synth read the same value.
 *
 * @param src  the source.
 * @param fs   its segment's sample rate, in Hz.
 * @return chips per data symbol, or 0 for a source that is not continuous
 *         dsss (or has no `sps`).
 *
 * @code
 * wfm_source_t s = { .type = WFM_SYNTH_DSSS, .sps = 2,
 *                    .symbol_rate = 1000.0 };
 * if (dp_wfm_source_dsss_cps (&s, 1e6) != 500.0)   // 500 chips per symbol
 *   return 1;
 * @endcode
 */
double dp_wfm_source_dsss_cps(const wfm_source_t *src, double fs);

/**
 * @brief NULL when this source's frame fields can be honoured; else why not.
 *
 * ONE rule, asked by all three faces — the wfmgen CLI before it generates, the
 * standalone `Synth` through `dp_wfm_source_to_synth`, and the composer through
 * `dp_wfm_compose_create` — because the alternative is what shipped: the flags
 * were accepted, stored and readable back on every face, and applied on none
 * of them, so a caller who asked for a framed waveform silently got an
 * unframed one.
 *
 * A frame needs a payload. `type=bits` and the PN-sourced `bpsk`/`qpsk`/`pn`
 * carry one when a payload is given (literal or generated, gh-762); the
 * latter are then built as a BITS synth (dp_wfm_source_synth_type()).
 * Types with no bit stream (tone, noise, chirp, symbols) are refused with a
 * reason.
 *
 * @param src  The source.
 * @return NULL if there is nothing wrong, else a static message.
 */
const char *dp_wfm_source_frame_error(const wfm_source_t *src);

/**
 * @brief NULL when this source can be built; else why not, as a sentence.
 *
 * The question every face asks before it builds -- the wfmgen CLI, a scene
 * read by dp_wfm_compose_from_json_why(), the standalone `Synth` through
 * dp_wfm_source_to_synth() and the composer through dp_wfm_compose_create()
 * -- so all four refuse the same sources for the same reason. It checks the
 * source's own parameters, then asks dp_wfm_source_frame_error() about its
 * frame:
 *
 * - `pn_poly` must fit the `pn_length`-bit register (pn_fits_register()).
 *   The generator masks a wider one, silently, so `pn_poly = 0x40` on a
 *   5-bit register is a register with no feedback: the seed, then zeros,
 *   a constant waveform that still looks like a PN source (doppler#1636).
 *   0 selects the maximal-length polynomial and always fits.
 * - No two members the surface table declares exclusive may both be set
 *   (`WFM_SURFACE_EXCLUSIVE`, wfm_surface.h): a carried `frame` is the whole
 *   frame, so a `payload` beside it would be dropped (doppler#1683). The
 *   CLI and a scene refuse the same pair first, naming their own spelling.
 * - A dsss source has the codes it needs (doppler#1696): a burst whose
 *   frame has bits to spread needs `data_code`
 *   (dp_wfm_why_dsss_frame_no_data_code), a burst needs a preamble or a
 *   frame (dp_wfm_why_dsss_empty), and a continuous stream needs
 *   `data_code` (dp_wfm_why_dsss_cont_no_data_code). A preamble alone is a
 *   valid burst: an acquisition stimulus.
 *
 * A rule that needs the segment's sample rate is not here -- a source does
 * not carry it -- but in dp_wfm_scene_error(), which asks this first.
 *
 * @param src  The source.
 * @return NULL if there is nothing wrong, else a static message.
 *
 * @code
 * wfm_source_t s = { .type = WFM_SYNTH_PN, .sps = 1, .pn_length = 5,
 *                    .pn_poly = 0x40 };
 * dp_wfm_source_error (&s);   // "pn_poly has a bit above ..."
 * s.pn_poly = 0x12;
 * dp_wfm_source_error (&s);   // NULL: x^5 + x^2 + 1 fits
 * @endcode
 */
const char *dp_wfm_source_error(const wfm_source_t *src);

/**
 * @brief The reason dp_wfm_source_error() gives for a `pn_poly` wider than
 *        its register -- exported so a face that knows the values (the
 *        wfmgen CLI) can name them beside it, by identity rather than by
 *        matching text.
 */
extern const char dp_wfm_why_pn_poly[];

/**
 * @brief The reasons dp_wfm_source_error() gives a dsss source missing a
 *        code (doppler#1696): a burst whose frame has no data_code to
 *        spread it, a burst with neither a preamble nor a frame, and a
 *        continuous stream with no data_code. Exported so a test or a face
 *        can hold a refusal to its reason by identity.
 */
extern const char dp_wfm_why_dsss_frame_no_data_code[];
/** @copydoc dp_wfm_why_dsss_frame_no_data_code */
extern const char dp_wfm_why_dsss_empty[];
/** @copydoc dp_wfm_why_dsss_frame_no_data_code */
extern const char dp_wfm_why_dsss_cont_no_data_code[];

/**
 * @brief The reason dp_wfm_source_error() gives for `retired_bits` set:
 *        Python's retired `bits=` (and `payload=`, `pattern=`), named once.
 *        The CLI's and a scene's RETIRED tables say the same in their own
 *        spelling (doppler#1718).
 */
extern const char dp_wfm_why_retired_bits[];

/**
 * @brief The reasons dp_wfm_source_error() gives for `retired_sync` and
 *        `retired_crc` set.
 *
 * One sentence each, naming `frame=`; the CLI and a scene say the same in
 * their own spelling. Exposed so a test can pin the wording once.
 */
extern const char dp_wfm_why_retired_sync[];
extern const char dp_wfm_why_retired_crc[];

/**
 * @brief Why dp_wfm_source_to_synth() refused this source, or NULL.
 *
 * The standalone `Synth`'s reason channel (just-makeit's `bridge_error_fn`,
 * which takes the bridge's own arguments): dp_wfm_scene_error() of a scene
 * of one segment at @p fs, so a refused Synth raises the same sentence as
 * every other face, including a rule that needs the rate. NULL leaves the
 * binding's generic error, for a refusal that is not the source's.
 *
 * @param src  The source.
 * @param fs   The sample rate the bridge was given.
 * @return A static sentence, or NULL.
 *
 * @code
 * wfm_source_t s = { .type = WFM_SYNTH_PN, .sps = 1, .pn_length = 5,
 *                    .pn_poly = 0x40 };
 * dp_wfm_source_to_synth_error (&s, 1e6);   // dp_wfm_why_pn_poly
 * @endcode
 */
const char *dp_wfm_source_to_synth_error(const wfm_source_t *src, double fs);

/**
 * @brief Attach an unspread source's bit pattern, framed or not.
 *
 * The `type=bits` counterpart of dp_wfm_source_attach_dsss(), and called from the
 * same two places for the same reason. When the source carries a frame, the
 * pattern handed to `dp_wfm_synth_set_bits()` is `dp_wfm_frame_assemble()` of
 * `[preamble x reps | sync | payload | crc]` rather than the payload alone —
 * so the layout, the CRC's position and its bit order come from the one
 * descriptor that the DSSS path and the receiver already read.
 *
 * The frame CYCLES, exactly as an unframed pattern does: one descriptor fills
 * whatever length is asked for, which is what turns a one-frame description
 * into a multi-frame record.
 *
 * @param syn  A synth from dp_wfm_synth_create() with `wtype == WFM_SYNTH_BITS`.
 * @param src  The source (pattern, modulation, and any frame fields).
 * @return 0 on success (or a non-bits/no-pattern no-op); -1 on failure.
 */
int dp_wfm_source_attach_frame(dp_wfm_synth_state_t *syn, const wfm_source_t *src);

/**
 * @brief Drive a type=bits synth from a frame whose payload is a data source.
 *
 * The pull that replaces the cycle (docs/design/payload-data-source.md §7).
 * Each frame is @p d assembled over the next chunk of @p src
 * (dp_wfm_frame_assemble_data): the first now, and each one after at the
 * previous frame's last bit, so every stage over the payload covers its own
 * frame's chunk. Under @p pacing, a source with nothing yet sends an idle
 * frame (dp_wfm_data_frame, the one rule). When @p src ends, the synth goes
 * silent and dp_wfm_synth_data_ended() says so.
 *
 * @param syn         a synth created with type=bits.
 * @param d           the frame; exactly one field is `data:LEN`, and LEN is
 *                    the source's.
 * @param ops         stage kernels, as dp_wfm_frame_assemble(); may be NULL.
 * @param src         the data source; the synth OWNS it from here, success or
 *                    not, and frees it with the synth.
 * @param pacing      WFM_DATA_PACED for `--realtime`, else WFM_DATA_UNPACED.
 * @param modulation  as dp_wfm_synth_set_bits().
 * @return 0, or -1: not a bits synth, no data field in @p d, a frame that
 *         does not assemble, or a source that failed its first read.
 *
 * @code
 * wfm_frame_desc_t d;
 * const wfm_seq_t  data = { .kind = WFM_SEQ_DATA, .len = 16 };
 * dp_wfm_frame_fixed (&d, NULL, 0, NULL, &data, 1);        // [data:16 | crc]
 * wfm_data_src_t *src = dp_wfm_data_create ("0x0123456789AB", NULL, 16, NULL,
 *                                           NULL);
 * dp_wfm_synth_state_t *s
 *     = dp_wfm_synth_create (WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0,
 *                            0, 0.0);
 * dp_wfm_synth_attach_data (s, &d, NULL, src, WFM_DATA_UNPACED, 1); // bpsk
 * // three 32-bit frames, each over its own 16-bit chunk, then silence
 * dp_wfm_synth_destroy (s);                 // frees src too
 * @endcode
 */
int dp_wfm_synth_attach_data(dp_wfm_synth_state_t *syn, const wfm_frame_desc_t *d,
                             const wfm_frame_ops_t *ops, wfm_data_src_t *src,
                             wfm_data_pacing_t pacing, int modulation);

/**
 * @brief Pace a synth's data source: WFM_DATA_PACED for `--realtime`.
 *
 * A synth attached by dp_wfm_source_attach_frame() pulls UNPACED -- it
 * waits for its data, as `cat` does. A paced caller (the composer under
 * `--realtime`) sets this after the build, so a source with nothing yet
 * sends an idle frame instead (dp_wfm_data_frame, the one rule). A synth
 * with no data source ignores it.
 *
 * @code
 * dp_wfm_synth_state_t *s = dp_wfm_synth_create (
 *     WFM_SYNTH_BITS, 1e6, 0.0, 100.0, 0, 1, 8, 7, 0, 0, 0.0);
 * dp_wfm_synth_set_data_pacing (s, WFM_DATA_PACED); // no source: no-op
 * dp_wfm_synth_destroy (s);
 * @endcode
 */
void dp_wfm_synth_set_data_pacing(dp_wfm_synth_state_t *syn,
                                  wfm_data_pacing_t pacing);

/**
 * @brief Why a scene cannot be composed, or NULL: the one validator.
 *
 * Every source's dp_wfm_source_error(), then what only the scene can say:
 *
 * - A continuous dsss source's chip rate `fs / sps`, at its segment's
 *   `fs`, is at least its `symbol_rate` -- one chip per data symbol, the
 *   synth's own floor (dp_wfm_source_dsss_cps()). The default `fs = 1.0`
 *   with a `symbol_rate` in Hz is the case that finds it (doppler#1706);
 *   the reason is dp_wfm_why_dsss_cont_rate.
 * - A count (`num_samples`, non-zero or ranged) beside sources that set
 *   the segment's length (dp_wfm_segment_sets_length()) is refused with
 *   dp_wfm_why_count_derived: the length is theirs, so it would be dropped
 *   (doppler#1729).
 * - A data STREAM (`--data-from-file -`) has no end to repeat, so
 *   `repeat`, `continuous` and a segment's `repeats > 1` are refused, and
 *   stdin feeds at most one source.
 *
 * dp_wfm_compose_create() refuses exactly these, and
 * dp_wfm_compose_create_why() says which; a face calls this to say why.
 *
 * @return a static sentence naming the fault and its fix, or NULL.
 */
const char *dp_wfm_scene_error(const wfm_segment_t *segs, size_t n_segs,
                               int repeat, int continuous);

/**
 * @brief The reason dp_wfm_scene_error() gives a continuous dsss source
 *        whose chip rate is below its symbol rate -- exported so the wfmgen
 *        CLI can name the values beside it, by identity.
 */
extern const char dp_wfm_why_dsss_cont_rate[];

/**
 * @brief A plain segment's on-time when its `num_samples` is 0 and no
 *        source sets one: what `--count`, a scene and `Segment` default
 *        to.
 */
#define WFM_NUM_SAMPLES_PLAIN ((size_t)1024)

/**
 * @brief Whether a segment's sources SET its on-time, so its
 *        `num_samples` is derived and a count given beside them refused.
 *
 * Two kinds of source set a length (payload-data-source.md 4.6):
 *
 * - **A finite data source** -- `data`, a finite file, or a carried frame
 *   of fixed bits -- is its frames (dp_wfm_source_data_frames()); the
 *   longest of a segment's sets it.
 * - **A lone dsss burst** -- one dsss source, no `symbol_rate`, no data
 *   source -- is one burst (dp_wfm_source_dsss_nchips() times `sps`).
 *
 * A stream sets none: it runs to its end, and a count may bound it. A
 * non-zero `num_samples` (or a ranged one) beside a segment this answers
 * 1 for is refused by dp_wfm_scene_error() with
 * dp_wfm_why_count_derived, on every face.
 *
 * @code
 * static const uint8_t bits[16] = { 1 };
 * wfm_source_t  src  = { .type       = WFM_SYNTH_BITS,
 *                        .modulation = 1, // bpsk
 *                        .sps        = 1,
 *                        .pn_length  = 7 };
 * src.data = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = bits,
 *                         .len = 16 };
 * wfm_segment_t seg = { .sources = &src, .n_sources = 1, .fs = 1e6 };
 * if (!dp_wfm_segment_sets_length (&seg)) // its frames are the run
 *   return 1;
 * seg.num_samples = 1000;                 // so a count is refused
 * if (dp_wfm_scene_error (&seg, 1, 0, 0) != dp_wfm_why_count_derived)
 *   return 1;
 * @endcode
 *
 * @param seg  the segment, as the caller gave it.
 * @return 1 when its sources set its on-time, else 0.
 */
int dp_wfm_segment_sets_length(const wfm_segment_t *seg);

/**
 * @brief The reason dp_wfm_scene_error() gives a count beside a segment
 *        whose sources set its length (dp_wfm_segment_sets_length()) --
 *        exported so the wfmgen CLI can name its flag beside it.
 */
extern const char dp_wfm_why_count_derived[];


/**
 * @brief Whether a source's data is a stream: `data_from_file` is `-`.
 *
 * A stream has no length up front and no end to repeat, so a scene refuses
 * it with `repeat`, `continuous` or `repeats > 1`, Plan refuses it, and a
 * segment carrying one runs until it ends (dp_wfm_scene_error()).
 */
int dp_wfm_source_data_is_stream(const wfm_source_t *src);

/**
 * @brief Samples the first @p frames frames of a source's data occupy; 0
 *        with no data.
 *
 * The one length a run with a data source is measured in: a finite run is
 * this over dp_wfm_source_data_frames(), and a stream ends at this over the
 * frames it sent.
 *
 * - **A frame** (bits, bpsk/qpsk/pn): its output bits at the mapping's bits
 *   per symbol (rounded up), times `sps`, per frame.
 * - **A dsss burst**: its chips times `sps` (samples per chip), per burst.
 * - **Continuous dsss** has no frame: a "frame" is one data bit, one per
 *   data symbol, and the run is every chip of the first @p frames symbols
 *   at `fs / sps / symbol_rate` chips per symbol -- not a whole number, so
 *   not a product -- times `sps`.
 *
 * @param src     the source.
 * @param fs      the segment's sample rate (only continuous dsss reads it).
 * @param frames  frames (data bits, for continuous dsss) sent.
 */
uint64_t dp_wfm_source_data_samples(const wfm_source_t *src, double fs,
                                    uint64_t frames);

/**
 * @brief Frames a FINITE data source makes, `ceil(bits / LEN)`; 0 for a
 *        stream or none. On continuous dsss, which has no frame, its bits.
 *
 * Known before the first sample (a file's length from fstat), which is
 * what lets a finite run's length be derived rather than given (§4.6).
 *
 * @code
 * static const uint8_t bits[40] = { 1 };
 * wfm_source_t src = { .type = WFM_SYNTH_BPSK, .sps = 4 };
 * src.data      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = bits,
 *                              .len = 40 };
 * src.data_len  = 16;
 * src.fill      = (wfm_seq_t){ .kind = WFM_SEQ_DOTTED, .len = 2 };
 * if (dp_wfm_source_data_frames (&src) != 3) // 40 bits in 16-bit frames
 *   return 1;
 * if (dp_wfm_source_data_samples (&src, 1e6, 3) != 3 * 16 * 4) // bpsk
 *   return 1;
 * @endcode
 */
uint64_t dp_wfm_source_data_frames(const wfm_source_t *src);

/** @brief The data source a synth pulls from (dp_wfm_synth_attach_data), or
 *  NULL: its stats are the run's truth for scoring. */
const wfm_data_src_t *dp_wfm_synth_data_source(const dp_wfm_synth_state_t *syn);

/**
 * @brief The synth type to create this source with.
 *
 * The source's own type, except for a FRAMED `bpsk`/`qpsk`/`pn`: that one
 * transmits its frame, and the only synth that plays a bit pattern is a
 * `WFM_SYNTH_BITS` one (`dp_wfm_synth_set_bits()` is a no-op on any other),
 * so it is created as BITS and dp_wfm_source_attach_frame() hands it the
 * frame with the mapping the type names (bpsk for `bpsk`/`pn`, Gray QPSK for
 * `qpsk`). Created with its own type it played the LFSR stream and dropped
 * the frame (doppler#1616).
 *
 * Asked by both construction faces (`dp_wfm_compose_build_synth` and the
 * standalone `dp_wfm_source_to_synth`) and by dp_wfm_source_create_snr(),
 * which refers such a source's SNR to fs so its noise stays in the reference
 * its type names.
 *
 * @code
 * wfm_source_t s = { .type = WFM_SYNTH_BPSK };
 * int t = dp_wfm_source_synth_type (&s); // unframed: WFM_SYNTH_BPSK
 * @endcode
 *
 * @param src  The source.
 * @return A `WFM_SYNTH_*` type.
 */
int dp_wfm_source_synth_type(const wfm_source_t *src);

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
 * @return A heap synth (caller dp_wfm_synth_destroy()s it), or NULL on failure.
 */
dp_wfm_synth_state_t *dp_wfm_compose_build_synth(const wfm_source_t *src, double fs,
                                           size_t on_len, double freq,
                                           double snr, double f_end,
                                           unsigned epoch, int seed_advance,
                                           size_t instance);

/** @brief One source's renderer: its synth, plus its Doppler channel. */
typedef struct wfm_render wfm_render_t;

/**
 * @brief Build a source's renderer — `dp_wfm_compose_build_synth` plus the
 * clock-Doppler channel the source declares, if it declares one.
 *
 * THE pull path. Both faces go through `dp_wfm_render_steps()` rather than
 * calling `dp_wfm_synth_steps()` themselves, because a Doppler channel is a
 * RESAMPLER: it consumes about `n*(1+d)` inputs per `n` outputs, so "pull
 * `k`, get `k`" only holds if something keeps the remainder. Two
 * implementations that agreed today would drift the moment either grew a
 * holdover the other did not.
 *
 * A source with `doppler == 0 && doppler_rate == 0` gets no channel and
 * `dp_wfm_render_steps()` is then literally `dp_wfm_synth_steps()`, so every scene
 * that does not ask for Doppler renders through exactly the path it always
 * did — byte-identical, not merely equivalent.
 *
 * `doppler`/`doppler_rate` arrive ranged-resolved, like `freq`/`snr`/`f_end`.
 *
 * @p borrow is the channel a `WFM_DOPPLER_PERSIST` source keeps ACROSS
 * segments: the composer owns it for the life of the scene and passes it in
 * here, so the renderer uses it without adopting it and the geometry does not
 * restart when the synth is torn down at a segment boundary. NULL means the
 * ordinary case — the renderer creates and owns a channel if the source
 * declares Doppler, and destroys it with itself.
 *
 * @return A heap renderer (caller dp_wfm_render_destroy()s it), or NULL.
 */
wfm_render_t *dp_wfm_compose_build_render(const wfm_source_t *src, double fs,
                                       size_t on_len, double freq, double snr,
                                       double f_end, double doppler,
                                       double doppler_rate, unsigned epoch,
                                       int seed_advance, size_t instance,
                                       dp_doppler_channel_state_t *borrow);

/** @brief Pull exactly @p n samples from @p r, through its channel if any. */
void dp_wfm_render_steps(wfm_render_t *r, float _Complex *dst, size_t n);

/**
 * @brief Pull @p n samples of the source's NOISE FLOOR only, through the
 * same channel.
 *
 * What a gap renders (gh-409). The channel runs here too, and deliberately:
 * an emitter does not stop moving because its burst ended, so a pass is
 * continuous and during a gap the thing propagating is the noise floor. Skip
 * the channel over gaps and `doppler_rate` across a multi-burst scene
 * quietly means "rate per unit of ON time" instead of per second.
 */
void dp_wfm_render_noise_steps(wfm_render_t *r, float _Complex *dst, size_t n);

/** @brief Free a renderer and everything it owns. NULL-safe. */
void dp_wfm_render_destroy(wfm_render_t *r);

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
typedef struct wfm_compose_state dp_wfm_compose_state_t;

/**
 * @brief Build a composer over a copy of `segs`.
 *
 * @param segs        Segment list (copied; caller keeps ownership).
 * @param n_segs      Number of segments (>= 1).
 * @param repeat      Non-zero: loop the whole sequence after the last segment.
 * @param continuous  Non-zero: never finish (implies repeat); execute always
 *                    returns `max`.
 * @return Heap state, or NULL on bad args / allocation / synth failure.
 * @note Caller must dp_wfm_compose_destroy() when done.
 */
dp_wfm_compose_state_t *dp_wfm_compose_create(
    const wfm_segment_t *segs, size_t n_segs, int repeat, int continuous);

/**
 * @brief dp_wfm_compose_create(), able to say why the scene was refused.
 *
 * The scene is asked dp_wfm_scene_error() before anything is built, and its
 * sentence is what @p why receives -- the same sentence the wfmgen CLI, a
 * scene read by dp_wfm_compose_from_json_why() and the standalone `Synth`
 * report, because it is the same validator. It is the create the generated
 * `Composer([...])` calls (just-makeit's `create_why`), so a refused
 * composer raises `ValueError(<the reason>)`.
 *
 * @param segs        as for dp_wfm_compose_create().
 * @param n_segs      as for dp_wfm_compose_create().
 * @param repeat      as for dp_wfm_compose_create().
 * @param continuous  as for dp_wfm_compose_create().
 * @param why         optional; receives a STATIC reason when the scene is
 *                    refused, and is left as it was in every other case
 *                    (success, bad arguments, an allocation or synth
 *                    failure).
 * @return Heap state, or NULL as for dp_wfm_compose_create().
 *
 * @code
 * wfm_source_t  src = { .type = WFM_SYNTH_DSSS, .sps = 2 };   // no codes
 * wfm_segment_t seg = { .sources = &src, .n_sources = 1, .fs = 1e6,
 *                       .num_samples = 64 };
 * const char   *why = NULL;
 * dp_wfm_compose_state_t *c = dp_wfm_compose_create_why (&seg, 1, 0, 0, &why);
 * if (c != NULL || why != dp_wfm_why_dsss_empty)   // refused, and says why
 *   return 1;
 * @endcode
 */
dp_wfm_compose_state_t *dp_wfm_compose_create_why(const wfm_segment_t *segs,
                                                  size_t n_segs, int repeat,
                                                  int continuous,
                                                  const char **why);

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
void dp_wfm_compose_set_seed_advance(dp_wfm_compose_state_t *state, int mode);

/**
 * @brief Pace a composer's data sources: WFM_DATA_PACED under `--realtime`.
 *
 * Applied to the synths already built -- create builds the first
 * segment's, before this can be called -- and to every synth built from here
 * on, so a data stream with nothing yet sends an idle frame of fill rather
 * than waiting (dp_wfm_data_frame, the one rule). The default,
 * WFM_DATA_UNPACED, waits.
 */
void dp_wfm_compose_set_data_pacing(dp_wfm_compose_state_t *state,
                                    wfm_data_pacing_t pacing);

/**
 * @brief The composer's current seed-advance mode (a `wfm_seed_advance_t`).
 *
 * The composer is the SSOT for it: `--from-file` sets it from the spec and the
 * flag path sets it from `--seed-advance`, so a serialiser must read it back
 * from here rather than from whichever half happened to supply it.
 * @param state  Compose state (may be NULL → `WFM_SEED_ADVANCE_NONE`).
 */
int dp_wfm_compose_seed_advance(const dp_wfm_compose_state_t *state);

/**
 * @brief Emit up to `max` samples of the composed stream.
 * @return Number of samples written: < `max` (or 0) signals the sequence
 *         finished (never, when `continuous`).
 */
size_t dp_wfm_compose_execute(
    dp_wfm_compose_state_t *state, float _Complex *out, size_t max);

/**
 * @brief Emit up to `max` samples, all at ONE sample rate, and say which.
 *
 * dp_wfm_compose_execute() for an output that states a rate per block: it
 * stops early where the next segment's `fs` differs from the samples
 * already written, so every block it returns has one rate. A scene whose
 * segments share an `fs` never stops early, and its samples are the same,
 * byte for byte, as dp_wfm_compose_execute()'s. A short return therefore
 * does NOT mean the scene finished; 0 does.
 *
 * @code
 * wfm_source_t  src     = { .type = WFM_SYNTH_TONE, .snr = 100.0 };
 * wfm_segment_t segs[2] = {
 *   { .sources = &src, .n_sources = 1, .fs = 6e6, .num_samples = 8 },
 *   { .sources = &src, .n_sources = 1, .fs = 2e6, .num_samples = 8 },
 * };
 * dp_wfm_compose_state_t *c = dp_wfm_compose_create (segs, 2, 0, 0);
 * float _Complex buf[64];
 * double         fs = 0.0;
 * size_t a = dp_wfm_compose_execute_rate (c, buf, 64, &fs); // 8 at 6e6
 * int    ok = a == 8 && fs == 6e6;
 * size_t b = dp_wfm_compose_execute_rate (c, buf, 64, &fs); // 8 at 2e6
 * ok = ok && b == 8 && fs == 2e6;
 * ok = ok && dp_wfm_compose_execute_rate (c, buf, 64, &fs) == 0;
 * dp_wfm_compose_destroy (c);
 * return ok ? 0 : 1;
 * @endcode
 *
 * @param state the composer.
 * @param out   destination, `max` samples.
 * @param max   capacity of @p out.
 * @param fs    receives the rate of the samples written (left untouched
 *              when none are).
 * @return samples written; 0 when the scene has finished.
 */
size_t dp_wfm_compose_execute_rate(dp_wfm_compose_state_t *state,
                                   float _Complex *out, size_t max,
                                   double *fs);

/**
 * @brief The ONE answer to "what is this stream's sample rate": the `fs`
 *        every segment shares, or 0.0 when they differ.
 *
 * `fs` is per segment, and a scene whose segments differ is legal: no
 * single rate is true of it. 0.0 is the library's "not stated" (a Writer
 * opened at `fs=0.0`, a SigMF document without `core:sample_rate`), so an
 * output asks this and either states the rate it returns or, given 0.0,
 * says nothing -- or refuses, if its format cannot say nothing (a BLUE
 * header has one `xdelta`). Every output asks here rather than reading
 * `segs[0].fs`, which is a rate only when they agree (doppler#1733).
 *
 * @code
 * wfm_segment_t s[2] = { { .fs = 6e6 }, { .fs = 6e6 } };
 * int ok = dp_wfm_scene_fs (s, 2) == 6e6;
 * s[1].fs = 2e6;
 * ok = ok && dp_wfm_scene_fs (s, 2) == 0.0 && dp_wfm_scene_fs (s, 0) == 0.0;
 * return ok ? 0 : 1;
 * @endcode
 *
 * @param segs   the segments; may be NULL when @p n_segs is 0.
 * @param n_segs their count.
 * @return the shared fs, or 0.0 when the segments differ or there are none.
 */
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

/** @brief Destroy a composer and its active synth. @param state May be NULL. */
void dp_wfm_compose_destroy(dp_wfm_compose_state_t *state);

/**
 * @brief Borrow the composer's stored segment list (for --record / SigMF).
 *
 * Each source's `data_sent` holds what its data source has sent so far in
 * the latest instance, brought up to date by this call, so a record or a
 * SigMF sidecar written after a run carries the run's truth.
 * @param state      the composer.
 * @param n_out      receives the segment count.
 * @param repeat     receives the repeat flag (may be NULL).
 * @param continuous receives the continuous flag (may be NULL).
 * @return Pointer to the internal segments (owned by the composer; valid until
 *         dp_wfm_compose_destroy).
 */
const wfm_segment_t *dp_wfm_compose_segments(const dp_wfm_compose_state_t *state,
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
 * `seed_advance` (a `wfm_seed_advance_t`) and `headroom` (dB of output backoff
 * applied at the writer, not the composer) are each emitted as a top-level
 * field only when non-default, so an unrecorded run and any older spec stay
 * byte-identical. Read `headroom` back with dp_wfm_spec_headroom(); the parser
 * reads `seed_advance` straight onto the composer.
 *
 * `seed_advance` is a parameter rather than something read from `segs` because
 * it is a property of the whole stream, like `repeat`/`continuous`. Omitting it
 * is what made a recorded run replay a DIFFERENT waveform (doppler#978): the
 * key was parsed and never written, so the round-trip silently fell back to
 * NONE and every loop after the first came out identical.
 *
 * @return malloc'd JSON (caller frees), or NULL on allocation failure.
 */
char *dp_wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                       int continuous, int seed_advance, double headroom);

/**
 * @brief The top-level `headroom` (dB) from a spec JSON, or 0 if absent.
 *
 * Lets `--from-file` reproduce a recorded `--headroom`; the value is a writer
 * gain, so it lives outside the composer state.
 */
double dp_wfm_spec_headroom(const char *json);

/**
 * @brief A ready-to-edit example spec in the canonical --from-file schema.
 *
 * Returns a representative multi-segment template — an inline tone, an
 * RRC-shaped QPSK-from-bits burst with a trailing gap, and a two-source
 * additive `sum` mix — serialised with dp_wfm_spec_to_json(), so it is valid by
 * construction and round-trips through dp_wfm_compose_from_json() unchanged. It
 * therefore doubles as a working starting point for `wfmgen --from-file`, not
 * just documentation: dump it, edit the fields, feed it back.
 *
 * @return malloc'd JSON (caller frees), or NULL on allocation failure.
 */
char *dp_wfm_spec_template_json(void);

/**
 * @brief Read a frame description from its JSON form.
 *
 * The form a scene's `"frame"` key holds, and what `wfmgen --frame FILE`
 * reads — one reader for both: `{"fields": [...], "stages": [...]}`. A field
 * with bits is its Field text, `"spec"` (`"0x1ACFFC1D"`, `"pn:31:5*4"`); a
 * derived field is its `"bits"` and the `"derived_by"` stage (index plus
 * one). A stage names its `"kind"` (`"crc16"`, `"rs"`, `"randomise"`,
 * `"conv"`, `"interleave"`, or a number from `WFM_STAGE_USER` up) and its
 * cover as `"first_field"`/`"n_fields"`, plus `"depth"`, `"unit_bits"` and
 * `"emit_num"`/`"emit_den"` where the kind uses them.
 *
 * A malformed description is REFUSED, never salvaged: a frame read wrong
 * builds a waveform that looks fine and is not the one described. Whether
 * it lays out is a separate question, asked by `dp_wfm_source_frame_error()`
 * once a source carries it.
 *
 * @param json  the frame object, NUL-terminated.
 * @param why   receives a static reason on failure; may be NULL.
 * @return the description, owning its literal bits (free it with
 *         dp_wfm_frame_free()), or NULL if the text is not a frame object.
 *
 * @code
 * const char       *why;
 * wfm_frame_desc_t *d = dp_wfm_frame_from_json (
 *     "{\"fields\": [{\"name\": \"sync\", \"spec\": \"0x1ACFFC1D\"},"
 *     "              {\"name\": \"data\", \"spec\": \"pn:31:5*4\"}]}",
 *     &why);
 * if (!d)
 *   fprintf (stderr, "error: %s\n", why);
 * // d->n_fields == 2; d->field[1].reps == 4
 * dp_wfm_frame_free (d);
 * @endcode
 */
wfm_frame_desc_t *dp_wfm_frame_from_json(const char *json, const char **why);

/**
 * @brief Free a description returned by dp_wfm_frame_from_json(), bits and
 *        all. NULL is a no-op.
 *
 * @code
 * dp_wfm_frame_free (dp_wfm_frame_from_json ("{\"fields\": []}", NULL));
 * dp_wfm_frame_free (NULL);   // nothing to free
 * @endcode
 */
void dp_wfm_frame_free(wfm_frame_desc_t *d);

/**
 * @brief Refuse text for the retired `sync=`: a sync word is a field of the
 *        frame description.
 *
 * The coercion hook of the `sync` tombstone: ANY `str` reaching it is
 * refused, and a non-empty array is refused by @ref dp_wfm_source_error, so
 * `sync=` fails whatever it is given and the sentence names `frame=`.
 *
 * @param text    the str (unused: every str is refused).
 * @param out     unused.
 * @param max_out unused.
 * @param why     receives the sentence.
 * @return 0.
 */
size_t dp_wfm_source_sync_refuse_text(const char *text, uint8_t *out,
                                      size_t max_out, const char **why);

/**
 * @brief Refuse text for the retired `crc=`: a CRC is a stage of the frame
 *        description.
 *
 * As @ref dp_wfm_source_sync_refuse_text, for `crc=`. `crc="none"` is
 * refused too: it would otherwise be a spelling that works forever with no
 * way to retire it.
 *
 * @param text    the str (unused: every str is refused).
 * @param out     unused.
 * @param max_out unused.
 * @param why     receives the sentence.
 * @return 0.
 */
size_t dp_wfm_source_crc_refuse_text(const char *text, uint8_t *out,
                                     size_t max_out, const char **why);

/**
 * @brief Why `--sync` / `--crc` / `--acq-code` cannot frame this source, or
 *        NULL.
 *
 * The CLI's flags spell the common frame, which needs a waveform that carries
 * a bit stream and, off dsss, a data source to fill it. Said once here and
 * used by the same refusal inside @ref dp_wfm_source_frame_error, so the
 * flags and a bare source give one answer. A dsss burst needs neither.
 *
 * @param src  the source.
 * @return a static sentence naming the fix, or NULL.
 */
const char *dp_wfm_framing_flags_error(const wfm_source_t *src);

/**
 * @brief The common frame as a description: `[preamble x reps | sync |
 *        data | crc]`, built by the ONE function that builds it.
 *
 * Used by the bridge (a scene's or Python's `acq_code` + data, with no sync
 * word and no CRC) and by `wfmgen` for `--sync` and `--crc`, which are sugar
 * for these fields. The preamble is a field for an unspread source and is
 * NOT one for a spread burst: a DSSS preamble is sent unspread outside the
 * description. The description borrows `src`'s and `sync`'s sequences.
 *
 * @param src   the source (its `acq_code`, `acq_reps`, `data`,
 *              `data_from_file` and `data_len` are read).
 * @param sync  the sync word, or NULL for none.
 * @param crc   non-zero appends a CRC-16 over the payload.
 * @param d     receives the description.
 * @return 0, or -1 when it does not lay out.
 */
int dp_wfm_source_common_frame(const wfm_source_t *src, const wfm_seq_t *sync,
                               int crc, wfm_frame_desc_t *d);

/**
 * @brief Refuse text for a source's bit field: an object takes bits.
 *
 * A composer source's bit fields (`payload`, `sync`, `acq_code`,
 * `data_code`) take BITS on the Python face -- a `uint8` array, bytes or a
 * sequence of 0/1 -- and module helpers make them from other forms:
 * `field_bits(text)` for the Field grammar, `cvt.hex_to_bin`,
 * `cvt.bytes_to_bin`. A `str` is refused rather than read, so the object
 * has one shape and the Field grammar one door. The text faces (the CLI, a
 * scene) keep reading a Field through dp_wfm_field_parse().
 *
 * It has the shape of dp_wfm_field_bits() because the binding calls it
 * where that would be called (just-makeit's `coerce_str_fn`), and it always
 * refuses: it returns 0 and sets @p why to its one static reason.
 *
 * @param text     ignored.
 * @param out      never written.
 * @param max_out  ignored.
 * @param why      receives the static reason; may be NULL.
 * @return 0, always.
 *
 * @code
 * const char *why;
 * size_t      n = dp_wfm_source_bits_refuse_text ("0101", NULL, 0, &why);
 * // n == 0; why names field_bits()
 * @endcode
 */
size_t dp_wfm_source_bits_refuse_text(const char *text, uint8_t *out,
                                      size_t max_out, const char **why);

/**
 * @brief Refuse text for a source's `frame=`: it takes a description.
 *
 * The frame face of dp_wfm_source_bits_refuse_text(). A source's `frame=`
 * takes a `FrameDesc` or a `Frame` on the Python face, and a `str` is
 * refused rather than read as JSON; a description written as JSON is a
 * scene's `"frame"` key, read by dp_wfm_frame_from_json(). It has the shape
 * of that reader because the binding calls it where the reader would be
 * called (just-makeit's owned-pointer `parse_fn` with `parse_why`).
 *
 * @param text  ignored.
 * @param why   receives the static reason; may be NULL.
 * @return NULL, always.
 *
 * @code
 * const char *why;
 * wfm_frame_desc_t *d = dp_wfm_frame_refuse_text ("{\"fields\": []}", &why);
 * // d == NULL; why names FrameDesc
 * @endcode
 */
wfm_frame_desc_t *dp_wfm_frame_refuse_text(const char *text, const char **why);

/**
 * @brief Write a description as its JSON frame object, the text
 *        dp_wfm_frame_from_json() reads back.
 *
 * The same writer a scene's `"frame"` key uses, so the two cannot spell a
 * description differently: a literal field is its Field text, a derived
 * field its `bits` and `derived_by`, a stage its kind and cover.
 *
 * @param d  the description; NULL gives NULL.
 * @return a NUL-terminated string the caller releases with free().
 *
 * @code
 * wfm_frame_desc_t *d
 *     = dp_wfm_frame_from_json ("{\"fields\": [{\"spec\": \"1010\"}]}", NULL);
 * char *text = dp_wfm_frame_to_json (d);
 * // text: {"fields":[{"spec":"0xa"}],"stages":[]}
 * free (text);
 * dp_wfm_frame_free (d);
 * @endcode
 */
char *dp_wfm_frame_to_json(const wfm_frame_desc_t *d);

/**
 * @brief Deep-copy a description: the struct and each literal field's bits.
 *
 * A source borrows its description; a holder that must outlive the
 * caller's (the composer, a Python `Synth`) takes a copy instead, and
 * releases it with dp_wfm_frame_free(). A later change to the original,
 * or freeing it, does not reach the copy.
 *
 * @param d  the description to copy; NULL gives NULL.
 * @return the owned copy.
 *
 * @code
 * wfm_frame_desc_t *a
 *     = dp_wfm_frame_from_json ("{\"fields\": [{\"spec\": \"1010\"}]}", NULL);
 * wfm_frame_desc_t *b = dp_wfm_frame_copy (a);
 * dp_wfm_frame_free (a);   // b is unaffected
 * // b->field[0].seq.len == 4
 * dp_wfm_frame_free (b);
 * @endcode
 */
wfm_frame_desc_t *dp_wfm_frame_copy(const wfm_frame_desc_t *d);

/**
 * @brief Build a composer from a JSON spec string (for --from-file).
 * @return Composer state, or NULL on parse error / bad type / no segments.
 */
dp_wfm_compose_state_t *dp_wfm_compose_from_json(const char *json);

/**
 * @brief The same, but able to say why a FRAME was refused.
 *
 * A spec is the interface most likely to be hand-written, and a NULL return
 * is the one answer that cannot teach anything. This runs
 * @ref dp_wfm_source_frame_error over every parsed source before handing them to
 * the composer — which asks the same question and would refuse either way —
 * so the reason survives the boundary as a sentence instead of a pointer.
 *
 * Only the frame rule reports this way. A parse error or a bad type is still
 * a bare NULL, because those are cJSON's to describe and duplicating its
 * diagnostics here would be a second opinion about the same text.
 *
 * @param json  the spec.
 * @param why   optional; receives a STATIC message when a source's frame is
 *              refused, or NULL in every other case (including success).
 *              Passing NULL makes this exactly @ref dp_wfm_compose_from_json.
 * @return Composer state, or NULL on parse error / bad type / no segments /
 *         a refused frame.
 */
dp_wfm_compose_state_t *dp_wfm_compose_from_json_why(const char *json,
                                               const char **why);

/**
 * @brief Build a composer from a JSON spec file.
 * @return Composer state, or NULL on read/parse error.
 */
/**
 * @brief dp_wfm_compose_from_json_why(), reading a scene that lives in @p base.
 *
 * A source's relative `"data_from_file"` is the scene's: it resolves against
 * @p base, the directory the scene was read from, so a scene and its data
 * move together and replay from anywhere. NULL (as from_json_why passes)
 * leaves a relative path relative to the caller's working directory.
 *
 * @param json  the scene's text.
 * @param base  the scene's directory, or NULL.
 * @param why   optional; receives a static reason for a refusal.
 * @return the composer, or NULL.
 *
 * @code
 * const char *why = NULL;
 * dp_wfm_compose_state_t *c = dp_wfm_compose_from_json_at (
 *     "{\"segments\":[{\"type\":\"tone\",\"num_samples\":8}]}", ".",
 *     &why);
 * if (!c)
 *   return 1;
 * dp_wfm_compose_destroy (c);
 * @endcode
 */
dp_wfm_compose_state_t *dp_wfm_compose_from_json_at(const char *json,
                                                    const char *base,
                                                    const char **why);

/**
 * @brief dp_wfm_compose_from_json_at(), replaying a record by its data.
 *
 * A `--record` stores each data source's truth as `"data_sent"`: the
 * frames, the fill bits padding the last, the idle frames, and for a file
 * or stdin the bits read and their `dp_hash64` (payload-data-source.md
 * §4.8). A replay identifies a file by that content, not by its name:
 *
 * - a `"data_from_file"` whose `"data_sent"` carries a hash is refused
 *   unless the file's length in bits and hash are the record's. The reason
 *   names the file and both hashes;
 * - a `"data_from_file": "-"` is a run read from stdin, whose octets are
 *   gone. It is refused unless @p data_file names the file that held
 *   them, which is then checked the same way. `-` again is refused: a pipe
 *   could only be checked after it had been sent;
 * - a @p data_file that no `"-"` source takes is refused: a scene carries
 *   its own data.
 *
 * Every check is made before anything is built. A scene with no hash (one
 * written by hand) replays its files unchecked.
 *
 * @param json       the scene's text.
 * @param base       the scene's directory, or NULL.
 * @param data_file  the file a record's stdin is replayed from
 *                   (`--data-from-file` given again), as typed: relative to
 *                   the working directory, not to @p base. NULL for none.
 * @param why        optional; receives the reason for a refusal. A
 *                   mismatch's names the numbers, so it is formatted into
 *                   a thread-local buffer, valid until this thread reads
 *                   another scene; never freed by the caller.
 * @return the composer, or NULL.
 *
 * @code
 * const char *why = NULL;
 * dp_wfm_compose_state_t *c = dp_wfm_compose_from_json_data (
 *     "{\"segments\":[{\"type\":\"bits\",\"data_from_file\":\"-\"}]}",
 *     NULL, NULL, &why);
 * if (c || !why) // stdin with no file given again: refused, with a reason
 *   return 1;
 * @endcode
 */
dp_wfm_compose_state_t *dp_wfm_compose_from_json_data(const char *json,
                                                      const char *base,
                                                      const char *data_file,
                                                      const char **why);

dp_wfm_compose_state_t *dp_wfm_compose_from_file(const char *path);

/**
 * @brief @ref dp_wfm_compose_from_file, able to say why a scene was refused.
 *
 * Reads @p path and hands its text to @ref dp_wfm_compose_from_json_why, so
 * the reason is that function's: a retired key or a refused frame, named. A
 * file that cannot be read gives NULL and leaves @p why NULL.
 *
 * @param path  the spec file.
 * @param why   optional; as for @ref dp_wfm_compose_from_json_why.
 * @return Composer state, or NULL on read/parse error / a refused scene.
 */
dp_wfm_compose_state_t *dp_wfm_compose_from_file_why(const char *path,
                                                     const char **why);

#ifdef __cplusplus
}
#endif

#endif /* WFM_COMPOSE_H */
