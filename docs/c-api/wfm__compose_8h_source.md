

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
  WFM_RANGE_FREQ          = 1u << 0, 
  WFM_RANGE_SNR           = 1u << 1, 
  WFM_RANGE_LEVEL         = 1u << 2, 
  WFM_RANGE_FEND          = 1u << 3, 
  WFM_RANGE_NUM_SAMPLES   = 1u << 4, 
  WFM_RANGE_OFF_SAMPLES   = 1u << 5, 
  WFM_RANGE_DELAY_SAMPLES = 1u << 6, 
  /* Source again, continuing after the segment bits rather than renumbering
     them: the bit index is the draw's stream selector (dp_wfm_draw_range), so
     moving one would change every drawn value in every existing scene. */
  WFM_RANGE_DOPPLER      = 1u << 7, 
  WFM_RANGE_DOPPLER_RATE = 1u << 8, 
};

typedef enum
{
  WFM_DOPPLER_PER_INSTANCE = 0, 
  WFM_DOPPLER_PERSIST      = 1, 
} wfm_doppler_lifetime_t;

typedef enum
{
  WFM_SNR_AUTO = 0, 
  WFM_SNR_FS   = 1, 
  WFM_SNR_EBNO = 2, 
  WFM_SNR_ESNO = 3, 
} wfm_snr_mode_t;

typedef enum
{
  WFM_BITMOD_NONE = 0, 
  WFM_BITMOD_BPSK = 1, 
  WFM_BITMOD_QPSK = 2, 
} wfm_bitmod_t;

typedef struct {
    int type;        
    double freq;     
    double snr;      
    int snr_mode;    
    uint32_t seed;   
    int sps;         
    int pn_length;   
    uint64_t pn_poly;  
    int lfsr;        
    double level;    
    int background;  
    double f_end;    
    size_t span;     
    int modulation;  
    float _Complex *symbols; 
    size_t n_symbols;      
    int pulse;       
    double rrc_beta; 
    int rrc_span;    
    unsigned ranged; 
    double freq_hi;  
    double snr_hi;   
    double level_hi; 
    double f_end_hi; 
    /* CLOCK DOPPLER, per source rather than per segment: it is a property of
       one emitter's motion, and two transmitters in a `sum` segment are on
       different geometries. `freq` cannot express it -- an offset moves the
       carrier alone, while Doppler rescales the whole received time base, so
       the symbol and chip rates move with it and a timing loop sees the error
       a carrier-only offset hides.

       Zero `doppler` AND zero `doppler_rate` means no channel is built at
       all, so a scene that does not ask for Doppler renders through exactly
       the code it always did. */
    double doppler;    
    double doppler_rate; 
    double carrier_hz; 
    double doppler_hi;    
    double doppler_rate_hi; 
    int doppler_lifetime; 
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
    wfm_seq_t acq_code;  
    size_t acq_reps;   
    wfm_seq_t data_code; 
    /* type=dsss, CONTINUOUS mode: a data-symbol rate independent of the code
       epoch rate selects the continuous form (dp_wfm_synth_set_dsss_cont) over
       the burst form above -- one waveform type, one discriminator, rather
       than a tenth entry in five hand-maintained name tables. 0 = burst.
       The frame fields (acq_code/frame/data) are meaningless when this is
       set and are rejected by the caller rather than silently ignored. */
    double symbol_rate;  
    int dsss_code_only;  
    /* The data source a frame's payload is drawn from, `data_len` bits per
       frame (docs/design/payload-data-source.md; wfm/wfm_data.h is the one
       table of what a source is). It is EITHER `data` (a Field, or a bit
       array in Python) OR `data_from_file` (a file, or `-` for stdin):
       one declaration makes the pair exclusive on every face. */
    wfm_seq_t data;  
    size_t data_len;   
    wfm_seq_t fill;  
    const char *data_from_file; 
    wfm_seq_t retired_bits; 
    wfm_seq_t retired_sync; 
    wfm_seq_t retired_crc; 
    wfm_data_stats_t data_sent; 
    size_t code_only_symbols; 
    size_t frame_symbols; 
} wfm_source_t;

typedef struct {
    wfm_source_t *sources; 
    size_t n_sources;      
    double fs;           
    size_t num_samples;  
    size_t off_samples;  
    unsigned ranged;     
    size_t num_samples_hi; 
    size_t off_samples_hi; 
    size_t repeats;
    size_t delay_samples;
    size_t delay_samples_hi; 
    int gap_noise;
} wfm_segment_t;

typedef struct {
    size_t seg;      
    size_t instance; 
    size_t start;    
    size_t delay;    
    size_t on;       
    size_t off;      
} wfm_span_t;

size_t dp_wfm_compose_spans(const wfm_segment_t *segs, size_t n_segs,
                         wfm_span_t *out, size_t cap);

typedef struct {
    size_t seg;      
    size_t instance; 
    size_t src;      
    size_t start;    
    size_t delay;    
    size_t on;       
    size_t off;      
    double freq;     
    double f_end;    
    double snr;      
    double level;    
    /* Clock Doppler is DRAWN like the four above, so it is reported like
       them. A ranged `doppler` recorded in the spec is a span; what this
       instance actually flew is only ever knowable here. */
    double doppler;      
    double doppler_rate; 
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

typedef void (*wfm_feed_fn)(void *ctx, float _Complex *dst, size_t n,
                            int signal);

wfm_render_t *dp_wfm_render_from_feed(
    const wfm_source_t *src, double fs, double doppler, double doppler_rate,
    dp_doppler_channel_state_t *borrow, size_t delay, size_t on,
    wfm_feed_fn feed, void *feed_ctx);

dp_doppler_channel_state_t *dp_wfm_compose_persist_channel(
    const wfm_source_t *src, double fs, double doppler, double doppler_rate,
    dp_doppler_channel_state_t **slot);

void dp_wfm_render_set_input_timeline(wfm_render_t *r, size_t delay,
                                      size_t on);

void dp_wfm_render_destroy(wfm_render_t *r);

typedef enum
{
  WFM_SEED_ADVANCE_NONE  = 0, 
  WFM_SEED_ADVANCE_NOISE = 1, 
  WFM_SEED_ADVANCE_ALL   = 2, 
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

typedef struct wfm_scene_ops
{
  const wfm_segment_t *(*segments)(const dp_wfm_compose_state_t *state,
                                   size_t *n_out, int *repeat,
                                   int *continuous);
} wfm_scene_ops_t;

static inline const wfm_segment_t *
dp_wfm_scene_segments(const dp_wfm_compose_state_t *scene, size_t *n_out)
{
  const wfm_scene_ops_t *ops
      = *(const wfm_scene_ops_t *const *)(const void *)scene;
  return ops->segments(scene, n_out, NULL, NULL);
}

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


