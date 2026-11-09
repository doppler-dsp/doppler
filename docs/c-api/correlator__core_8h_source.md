

# File correlator\_core.h

[**File List**](files.md) **>** [**correlator**](dir_296d11a08af6c23b555f7cd87cd10cbd.md) **>** [**correlator\_core.h**](correlator__core_8h.md)

[Go to the documentation of this file](correlator__core_8h.md)


```C++

#ifndef DP_CORRELATOR_CORE_H
#define DP_CORRELATOR_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/fft2d/fft2d_core.h"
#include "doppler/cvt/cvt_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SSOT: enum=correlator_flavor */
typedef enum
{
  DP_CORRELATOR_1D            = 1, 
  DP_CORRELATOR_2D            = 2, 
  DP_CORRELATOR_DELAY_DOPPLER = 3, 
  DP_CORRELATOR_DETECTOR_1D   = 4, 
  DP_CORRELATOR_DETECTOR_2D   = 5, 
  DP_CORRELATOR_SEARCH_CONTINUOUS = 6, 
  DP_CORRELATOR_SEARCH_BURST  = 7  
} dp_correlator_flavor_t;

typedef enum
{
  DET_NOISE_MEAN   = 0, 
  DET_NOISE_MEDIAN = 1, 
  DET_NOISE_MIN    = 2, 
  DET_NOISE_MAX    = 3, 
} det_noise_mode_t;

typedef struct
{
  size_t row;   
  size_t col;   
  float  value; 
} det_peak_t;

typedef struct
{
  size_t row; 
  size_t col; 
  float  peak_mag;  
  float  noise_est; 
  float  test_stat; 
  float cn0_dbhz_est;        
  uint64_t samples_consumed; 
  double chip_phase; 
  double doppler_rate_hz_s; 
  float test_stat_peak; 
  uint32_t _pad; 
} dp_correlator_hit_t;


typedef struct
{
  dp_tlm_t *ctx;      
  int32_t id_stat;    
  int32_t id_gate;    
  int32_t id_noise;   
  int32_t id_peak;    
  int32_t id_row;     
  int32_t id_col;     
  int32_t id_n_peaks; 
  int32_t id_n_held;  
  int32_t id_conc;    
  int32_t id_hit;     
} dp_correlator_tlm_t;

typedef void (*dp_correlator_surface_sink_fn) (void *ctx, const float *surface,
                                     size_t rows, size_t cols,
                                     uint64_t samples_consumed);

typedef struct
{
  float  ref;  
  size_t best; 
} dp_correlator_part_t;

#define DP_CORRELATOR_DELAY_LOSS_NODES 4

typedef struct
{
  size_t zone; 
  double delay_loss_mean; 
  double delay_loss[DP_CORRELATOR_DELAY_LOSS_NODES]; 
  double off_peak; 
  double off_peak_amp; 
  double psl; 
  double dsl; 
} dp_correlator_shape_t;

#define DP_CORRELATOR_SIDELOBE_DOPPLER_PTS 9
#define DP_CORRELATOR_SIDELOBE_LAG_PTS 5
#define DP_CORRELATOR_SIDELOBE_SLOTS 32

typedef struct dp_correlator_search
{
  float _Complex *ref; 
  float _Complex *out_buf; 
  float *nc_surface;    
  float _Complex *ref_spec; 
  float _Complex *map_spec; 
  float _Complex *map_work; 
  struct dp__dd_tile *map_tiles; 
  size_t  *map_rows; 
  int32_t *map_off;  
  float _Complex *map_wt; 
  size_t map_f;      
  size_t coherent_bins; 
  size_t window_bins; 
  size_t code_bins; 
  size_t lag_bins;  
  size_t n; 
  size_t n_surf; 
  size_t interp; 
  size_t frame_n; 
  size_t sf;      
  dp_correlator_shape_t shape; 
  size_t reps;    
  size_t searched_bins; 
  size_t n_noncoh;   
  double chip_rate; 
  double fs;        
  double cn0_dbhz;  
  double doppler_span_hz; 
  double doppler_res_hz; 
  double pfa;         
  double doppler_uncertainty; 
  size_t dsl_window_bins; 
  double symbol_rate; 
  double epochs_per_symbol;  
  size_t code_only_epochs; 
  double doppler_rate; 
  size_t rate_hyps;    
  float _Complex *rate_chirp; 
  size_t rate_chirp_d; 
  double carrier_freq_hz; 
  float _Complex *blk_raw; 
  int              threads;   
  int threads_req; 
  int epoch_fan; 
  dp_correlator_part_t *parts;          
  float  eta;       
  float  eta_nc;    
  double pfa_cell;  
  double pd;        
  double pd_predicted;  
  double pd_burst; 
  double straddle_loss; 
  uint8_t underpowered; 
  uint8_t burst; 
  /* Observability (docs/design/async-dsss-receiver.md §2.4):
     attach-on-demand, nothing in blobs. */
  dp_correlator_tlm_t tlm;       
  int       keep_surface; 
  float *stat_surface; 
  dp_correlator_surface_sink_fn sink; 
  void    *sink_ctx;   
  uint32_t sink_decim; 
  /* Everything above is written by create, regrid and the setters;
   * the threaded tile stages read it. The fields below are what a
   * push writes, so they sit behind a full cache line, as the
   * state's hot fields do (dp_correlator_state_t's _hot_pad). */
  unsigned char _hot_pad[64]; 
  float _Complex *prompt_scratch; 
  size_t nc_count; 
  int dsl_ready; 
  float _Complex *rate_best; 
  size_t rate_best_cap;      
  size_t rate_best_h;  
  size_t blk_epoch;    
  struct dp__dd_map *sl_map; 
  struct dp__dd_map *sl_map_cb; 
  float _Complex *sl_scratch; 
  double *sl_cmax; 
  size_t sl_crow[DP_CORRELATOR_SIDELOBE_SLOTS]; 
  size_t sl_ccol[DP_CORRELATOR_SIDELOBE_SLOTS]; 
  double sl_ctile[DP_CORRELATOR_SIDELOBE_SLOTS]; 
  size_t sl_cn;    
  size_t sl_cnext; 
  uint64_t samples_consumed; 
  uint64_t surface_at; 
  uint64_t dwells;     
  size_t   n_held;     
  float    peak_conc;  
} dp_correlator_search_t;

typedef struct {
  dp_correlator_flavor_t flavor; 
  dp_fft2d_state_t *fwd;       
  dp_fft2d_state_t *inv;       
  float _Complex *ref_spec;  
  float _Complex *work_fft;  
  float _Complex *accum;     
  /* Decoupled-inverse scratch — allocated only when (ny_out,nx_out) differ
   * from (ny,nx); NULL on the native path.  General path: zeropad goes
   * accum -> ztmp (rows) -> work_pad (cols), then inverse(work_pad).  Fast
   * path (nx_out != nx only, ny_out == ny is required for fast_path at all):
   * zeropad goes accum -> work_pad directly, one row at a time, via
   * dp_fft_pad_spectrum(); ztmp/zcol/zcolout are unused (2-axis-pad only). */
  float _Complex *work_pad;  
  float _Complex *ztmp;      
  float _Complex *zcol;      
  float _Complex *zcolout;   
  /* Single-row-reference fast path (see the file doc comment for the
   * identity this relies on).  fast_path is decided once at create() and
   * fixed for the object's lifetime; set_ref() may only refresh within the
   * same mode (see dp_correlator_set_ref doc comment). */
  int             fast_path;    
  dp_fft_state_t    *fwd1d;         
  dp_fft_state_t    *inv1d;         
  float _Complex  *row_ref_spec;  
  size_t ny;                
  size_t nx;                
  size_t n;                 
  size_t ny_out;            
  size_t nx_out;            
  size_t n_out;             
  size_t dwell;             
  /* Known-column output (see dp_correlator_create_2d's @p col_out).  A caller that
   * already knows the correlation lag it wants does not need the other
   * nx_out-1 columns, and evaluating the inverse at one bin is a dot
   * product against the conjugated reference, with NO transform in either
   * direction -- O(nx) per row against the full map's O(nx log nx), which
   * is what makes the kernel affordable for a caller that would otherwise
   * hand-roll the lag sum beside it. */
  int             col_out;   
  float _Complex *col_ref;   
  float _Complex *work_trunc;
  /* Worker fan-out (#2666).  The fast path's per-row stage and each stage
   * of the delay-Doppler flavor's map are each a set of independent
   * transforms, run over `nworkers` contiguous chunks on `pool`.  A pocketfft
   * plan keeps scratch of its own, so it cannot be shared between threads:
   * every worker owns one plan per stage, and chunk t always runs on plan t,
   * which is what keeps the output bit-identical across thread counts. */
  int              nworkers;  
  dp_pool_t       *pool;      
  dp_fft_state_t **row_fwd;   
  dp_fft_state_t **row_inv;   
  /* Delay-Doppler flavor (dp_correlator_create_delay_doppler; `flavor` is
   * DP_CORRELATOR_DELAY_DOPPLER, the one record of it, #2901). The flavor
   * is the fast path's accumulation plus the library's one delay-Doppler
   * map at dump time (docs/design/acq-surface.md §18); it is fixed at
   * create() and changes what the state blob means, so the blob carries
   * its own magic. */
  double           doppler_step;  
  struct dp__dd_map *dd_map;  
  float _Complex  *dd_fine;   
  size_t          *dd_rows;   
  int              dd_fused;  
  /* Batch scratch (dp_correlator_execute_fold and the typed batch calls),
   * allocated on the first batch, one slot per worker.  A batch makes its
   * maps here and never touches accum or count, so it is not state and is
   * not in the blob. */
  float _Complex  *bat_acc;   
  float _Complex  *bat_work;  
  float _Complex  *bat_pad;   
  float _Complex  *bat_map;   
  float _Complex  *bat_fine;  
  /* Detection layer (the detector flavors, docs/design/correlator.md
   * section 4): what create sets and push only reads.  Zero / NULL in the
   * map flavors.  Appended here, after every field the map reads, so that
   * no field the threaded map was measured at moved (#2752 step 3.3). */
  dp_f32_t       *ring;          
  size_t          ring_cap;      
  float _Complex *det_out;       
  float          *mag_buf;       
  float          *noise_scratch; 
  size_t          noise_lo;      
  size_t          noise_hi;      
  det_noise_mode_t noise_mode;   
  /* The search flavors' (#2752 3.4b): NULL / 0 in every other flavor. */
  size_t      max_peaks; 
  det_peak_t *peaks;     
  uint8_t    *band_mask; 
  uint8_t    *peak_mask; 
  uint32_t   *twin_row;  
  uint32_t   *twin_col;  
  dp_correlator_search_t *search; 
  /* Everything above is written by create (and set_ref) only, and the
   * threaded map's workers read it on every call. `count` is the one field
   * execute() writes every frame, so it lives behind a full cache line of
   * padding: sharing a line with `n` (as it did once `flavor` was added
   * first) made every frame invalidate that line in each worker's cache
   * (#2886). A detector flavor's push writes the fields after it every
   * call, and a caller may set the threshold every push (carrier_acq
   * does), so they sit with `count` (#2752 step 3.3). A new field goes
   * ABOVE this comment unless execute() or push() writes it, in which case
   * it goes below. */
  unsigned char _hot_pad[64]; 
  size_t count;             
  /* Detection layer, written by every push (detector flavors only). */
  dp_f32_framer_t framer;    
  float  threshold;          
  size_t peak_row;           
  size_t peak_col;           
  float  peak_mag;           
  float  noise_est;          
  float  test_stat;          
  int    _last_corr_valid;   
  size_t consumed;           
  size_t n_peaks;            
  size_t n_twins;            
  /* The search flavors' policy record, embedded rather than allocated
     (#2752 step 3.4b, docs/design/correlator.md section 7): behind a
     pointer to its own allocation, a small burst push ran 3% slower than
     acq's (bench-ab twice, then aligned with heap placement pinned), and
     here it ties. Last, so no field 3.2 or 3.3 measured moves; every
     flavor carries its 1.5 KB, unused outside the search. */
  dp_correlator_search_t search_rec; 
} dp_correlator_state_t;

dp_correlator_flavor_t
dp_correlator_flavor (const dp_correlator_state_t *state);

dp_correlator_state_t *dp_correlator_create_1d (const float _Complex *ref,
                                                size_t ref_len, size_t dwell,
                                                int nthreads, size_t n_out);

dp_correlator_state_t *dp_correlator_create_2d(const float _Complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out, int col_out);

dp_correlator_state_t *dp_correlator_create_delay_doppler (const float _Complex *code,
                                                   size_t code_len, size_t ny,
                                                   size_t dwell, int nthreads,
                                                   size_t ny_out);

void dp_correlator_destroy(dp_correlator_state_t *state);

void dp_correlator_reset(dp_correlator_state_t *state);

int dp_correlator_set_ref(dp_correlator_state_t *state, const float _Complex *ref);

size_t dp_correlator_execute_max_out(dp_correlator_state_t *state);

size_t dp_correlator_execute(dp_correlator_state_t *state, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out, const char **why);

/* ── Batches: many frames in one call (#2683) ──────────────────────────────
 *
 * A batch is H frames back to back, n_in = H * ny * nx: frame h is samples
 * h*ny*nx .. (h+1)*ny*nx - 1, each laid out as execute() takes it.  What a
 * hypothesis or a frame IS, is the caller's: the batch only knows H.  Each
 * map is n_out values (reshape to (ny_out, nx_out); Doppler is axis 0 and
 * delay axis 1 on the delay-Doppler map), and map h is row h of the (H, n_out)
 * result.  Map h is exactly
 * what a fresh object with dwell 1 returns for frame h -- the same
 * arithmetic, bit for bit, on every path (general 2-D, single-row fast
 * path, col_out, delay-Doppler) and at every thread count.  A batch never
 * reads or writes the dwell: accum, count and the state blob are the same
 * after it as before, so a batch may run in the middle of a dwell.
 *
 * Each typed call folds the H maps into a caller-owned result, in h order.
 * Every one refuses, with nothing written and SIZE_MAX returned, an empty
 * batch (n_in == 0), a length that is not a whole number of frames, or an
 * output with room for less than the whole result: a truncated reduction
 * has no meaning, so a batch never truncates the way execute() does.
 *
 * Threads: with two frames or more, each worker makes whole maps on its own
 * plans and scratch, and the maps are folded in h order, because splitting
 * one short frame across a pool costs more than it saves
 * (corr2d-hypotheses.md section 5).  A batch of one frame is split as
 * execute() splits it.  The coherent sum gives each worker a fixed range of
 * rows through every frame.  The result is the same at every thread
 * count. */

typedef struct
{
  void (*fold) (void *ctx, size_t h, const float _Complex *map, size_t n_out);
  void *ctx; 
} dp_correlator_fold_t;

size_t dp_correlator_execute_fold (dp_correlator_state_t *state,
                               const float _Complex *in, size_t n_in,
                               const dp_correlator_fold_t *fold);

size_t dp_correlator_execute_all_max_out (const dp_correlator_state_t *state,
                                      size_t n_in);

size_t dp_correlator_execute_all (dp_correlator_state_t *state,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out, const char **why);

size_t dp_correlator_execute_coherent_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_coherent (dp_correlator_state_t *state,
                                   const float _Complex *in, size_t n_in,
                                   float _Complex *out, size_t max_out, const char **why);

size_t dp_correlator_execute_power_sum_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_power_sum (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, size_t max_out, const char **why);

size_t dp_correlator_execute_power_max_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_power_max (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, uint32_t *out1,
                                    size_t max_out, const char **why);

size_t dp_correlator_execute_peaks_max_out (const dp_correlator_state_t *state,
                                        size_t n_in);

size_t dp_correlator_execute_peaks (dp_correlator_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float *out, uint32_t *out1, uint32_t *out2,
                                float *out3, size_t max_out, const char **why);

/* ── The hypothesis bank (#2683 phase 3) ───────────────────────────────── */

size_t dp_correlator_bank_fold (const dp_correlator_state_t *state, const float _Complex *x,
                            size_t x_len, size_t L, int g_min, int g_max,
                            const float _Complex *rates, size_t rates_len,
                            const dp_correlator_fold_t *fold);

size_t dp_correlator_bank_power_max_max_out (dp_correlator_state_t *state,
                                         size_t x_len);

size_t dp_correlator_bank_power_max (const dp_correlator_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 size_t L, int g_min, int g_max,
                                 const float _Complex *rates,
                                 size_t rates_len, float *out, uint32_t *out1,
                                 size_t max_out, int *status,
                                 const char **why);

size_t dp_correlator_bank_detect_max_out (dp_correlator_state_t *state,
                                      size_t x_len);

size_t dp_correlator_bank_detect (const dp_correlator_state_t *state,
                              const float _Complex *x, size_t x_len,
                              size_t L_d, size_t L_r, int g_min, int g_max,
                              float t_candidate, float t_confirm,
                              const float _Complex *rates, size_t rates_len,
                              float *out, int32_t *out1, uint32_t *out2,
                              uint32_t *out3, uint32_t *out4, size_t max_out,
                              int *status, const char **why);

/* ── Detector flavors ──────────────────────────────────────────────────────
 * The correlator with the stream and detection layers on top
 * (docs/design/correlator.md section 4): any chunk in, the ring's framer
 * cutting n-sample frames (n = ny*nx), each frame correlated and
 * integrated over dwell frames, and on every dump a test statistic
 *
 *   test_stat = peak_mag / noise_est
 *
 * where peak_mag is the largest |R| (a NaN cell is never the peak) and
 * noise_est an aggregate of |R| over the flat bins noise_lo..noise_hi.  A
 * hit is emitted when test_stat > threshold, or on every dump when
 * threshold is 0.  The detections are a function of the input stream, not
 * of how it was split into calls.  One object is single-threaded: never
 * push it from two threads at once.
 *
 * A detector takes its frames through dp_correlator_push() alone: every
 * execute, batch and bank call refuses it with SIZE_MAX and nothing
 * changed, since a frame from outside would put the dwell off the framer's
 * grid.  dp_correlator_reset() also drops its carry (the partial frame held
 * between pushes), and its last dump's fields (the peak, noise_est,
 * test_stat) read 0 until the next dump, so the blob after a reset matches
 * a fresh object's (doppler#2642); dp_correlator_consumed() reads 0.
 *
 * @code
 * // header-code: skip=a stream-loop fragment: recv() and ref are the caller's
 * dp_correlator_state_t *det = dp_correlator_create_detector_1d (
 *     ref, N, 1, 1, N - 1, DET_NOISE_MEAN, 0.0f, 1);
 * dp_correlator_hit_t hits[64];
 * // a full hits[] stops a push, and the input it did not take is offered
 * // again: dp_correlator_consumed(det) says where it stopped
 * while (recv (chunk, CHUNK_SZ)) {
 *     for (size_t off = 0; off < CHUNK_SZ; off += dp_correlator_consumed (det)) {
 *         size_t n = dp_correlator_push (det, chunk + off, CHUNK_SZ - off,
 *                                        hits, 64);
 *         for (size_t i = 0; i < n; i++)
 *             printf ("lag=%zu stat=%.2f\n", hits[i].col, hits[i].test_stat);
 *     }
 * }
 * dp_correlator_destroy (det);
 * @endcode
 */

dp_correlator_state_t *dp_correlator_create_detector_2d (
    const float _Complex *ref, size_t ny, size_t nx, size_t dwell,
    size_t noise_lo, size_t noise_hi, det_noise_mode_t noise_mode,
    float threshold, int nthreads);

dp_correlator_state_t *dp_correlator_create_detector_1d (
    const float _Complex *ref, size_t ref_len, size_t dwell,
    size_t noise_lo, size_t noise_hi, det_noise_mode_t noise_mode,
    float threshold, int nthreads);

size_t dp_correlator_push (dp_correlator_state_t *state,
                           const float _Complex *in, size_t n_in,
                           dp_correlator_hit_t *result, size_t max_results);

size_t dp_correlator_push_max_out (const dp_correlator_state_t *state,
                                   size_t n_in);

size_t dp_correlator_consumed (const dp_correlator_state_t *state);

int dp_correlator_set_threshold (dp_correlator_state_t *state,
                                 float threshold);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * One blob for every flavor (docs/design/correlator.md section 4): the
 * flavor, the grid, the frame count and the product-spectrum accumulator.
 * The flavor, ny, ny_out, fast_path, col_out and nx_out are reject keys:
 * each changes what the accumulator holds, so a blob that differs in any
 * of them is refused rather than reinterpreted.
 * A detector flavor's blob goes on: noise_lo, noise_hi and noise_mode
 * (u64 each; reject keys, since they decide what the stored noise_est and
 * test_stat mean), the threshold (f32; a mutator's value, restored), the
 * framer's carry as its own child blob (fewer than n samples), then the
 * last dump's peak_row and peak_col (u64) and peak_mag, noise_est and
 * test_stat (f32).  The last correlation map (det_out) is not in the blob,
 * so last_corr is None after set_state, and dp_correlator_consumed() reads
 * 0.  The retired detector magics 'DET1' and 'DET2' are not reused.
 * The FFT plans and the reference spectrum are config, rebuilt by create.
 * The retired magics 'CORR', 'CR2D' and 'DDMP' are not reused, so a blob
 * from a release before the one correlator fails dp_state_validate.
 * A search flavor's blob is the header, its flavor (u64; a reject key),
 * then the search payload: a fixed block of the counts that are reject
 * keys (the grid size, the non-coherent looks, the peak-list capacity,
 * the rate hypotheses, the carrier) and the stream position, the framer's
 * carry as its own child blob, the non-coherent power accumulator when
 * n_noncoh > 1, the last dwell's picks (the twins), and a tiled block's
 * raw epochs.  The carry, the position, the block's epoch and the look
 * count must name one stream point, or the blob is refused.  acq's
 * retired magic 'ACQR' is not reused. */
#define CORRELATOR_STATE_MAGIC DP_FOURCC ('C','O','R','L')
#define CORRELATOR_STATE_VERSION 2u
size_t dp_correlator_state_bytes (const dp_correlator_state_t *state);
void dp_correlator_get_state (const dp_correlator_state_t *state, void *blob);
int dp_correlator_set_state (dp_correlator_state_t *state, const void *blob);
int dp_correlator_state_check (const dp_correlator_state_t *state,
                               const void *blob);

/* ── Search flavors ──────────────────────────────────────────────────────────
 * #2752 3.4b: the acquisition engine, as the correlator's sixth and
 * seventh flavors: a streaming DSSS acquisition engine, burst and
 * continuous, acq until that step.
 *
 *
 * Acquires a signal built from a repeated sequence -- a spread-spectrum
 * code (the continuous door) or any repeated complex preamble (the burst
 * door) -- arriving with an unknown integer code phase and an unknown
 * carrier-frequency (Doppler) offset, buried in AWGN.  It
 * jointly estimates the (Doppler bin, code phase) and declares a detection
 * whenever the CFAR test statistic crosses an automatically configured
 * threshold.
 *
 * Pipeline (owned end to end, one object):
 *   push(raw cf32) -> ring buffer -> reframe to (coherent_bins, code_bins) ->
 *   slow-time Doppler FFT (FFT along the segment axis) -> 2-D code correlation
 *   against a single-row reference (corr2d) -> argmax + CFAR noise estimate
 *   -> threshold gate -> dp_correlator_hit_t.
 *
 * The fast-time axis (code_bins samples, one code period) is the circular
 * code matched filter, read on lag_bins columns: code_bins itself, or, on a
 * continuous engine whose period is not 7-smooth, the next power of two --
 * the cross-spectrum zero-padded before the inverse, an exact interpolation
 * of the same circular correlation at a fraction of the cost of inverses of
 * an awkward length (lag_bins' doc). The slow-time axis (coherent_bins
 * rows, one row per code repetition) is the coherent Doppler search.  A
 * carrier offset f (cycles/sample) lands the peak at
 * row = round(f*code_bins*coherent_bins) mod coherent_bins, column = code
 * phase.
 *
 * **Two mode-fixed public constructors, one shared engine.** A coherent
 * slow-time Doppler FFT can only ever resolve frequency *within* one native
 * span `chip_rate/(2*sf)` — more coherent depth subdivides that SAME fixed
 * range more finely, it never widens it — and for a continuous (async,
 * data-modulated) signal, a multi-epoch coherent axis wide enough to matter
 * aliases the data's own bit transitions across the whole Doppler-bin axis (a
 * structural mislock, not a graceful SNR loss — see
 * docs/design/acq-surface.md §9).  So the two constructors fix a mode each,
 * never a per-call knob:
 *
 * - dp_correlator_create_burst() — the smallest coherent depth `coherent_bins` in
 *   `[1, reps]` whose BURST Pd meets @p pd: the preamble lands at any
 *   offset against stream-aligned dwells and is detected when any one of
 *   them is (see dp_correlator_create_burst()); never a non-coherent look. Minimum
 *   latency for a strong signal, unmodulated bursts/preambles only.  A tighter
 *   @p doppler_uncertainty shrinks the searched cell count, lowering the
 *   Sidak threshold (more sensitive).  When @p doppler_uncertainty
 *   exceeds the native span, the wideband window-tiling mechanism below
 *   tiles it, and the coherent depth is chosen INSIDE every tile -- the
 *   smallest that meets @p pd at a stated design C/N0, and 1 with none (a
 *   default-built tiled search has never carried a block).
 * - dp_correlator_create_continuous() — for a continuous, data-modulated signal:
 *   ALWAYS uses the wideband window-tiling mechanism below, unconditionally
 *   (never attempts coherent multi-epoch combining, even when
 *   @p doppler_uncertainty is narrower than one native span) — closes the
 *   aliasing footgun structurally rather than pricing it as a tunable loss.
 *   Sensitivity margin comes entirely from auto-selected non-coherent looks.
 *
 * Both convert C/N0 to a per-sample amplitude SNR
 * (snr = sqrt(10^(cn0_dbhz/10) / fs)), where fs is the sample rate: the
 * burst door's @p fs, the continuous door's @p sample_rate.  Every reported
 * detection inverts this same relationship to report an estimated C/N0
 * (dp_correlator_hit_t::cn0_dbhz_est) — a bandwidth/integration-time-independent
 * figure of merit directly comparable to @p cn0_dbhz, unlike a raw per-sample
 * or coherently-integrated ratio (both scale with the sample rate and the
 * coherent depth, and so aren't portable across configurations).
 *
 * **Wideband window-tiling mode**: instead of coherent combining, tiles the
 * requested uncertainty with `window_bins = 2*ceil((du - span)/(2*span)) + 1`
 * (`du = doppler_uncertainty`, `span = chip_rate/(2*sf)`; always odd, so the
 * coverage is symmetric) parallel frequency-window hypotheses, each one native
 * span wide, searched every epoch from a SINGLE shared forward FFT of that
 * epoch: hypothesis r's spectrum is the shared FFT circularly rolled by r
 * bins (exact — the window spacing IS this code_bins-point FFT's own bin
 * spacing) against one fixed precomputed replica spectrum, then
 * inverse-FFT'd — `window_bins` inverse FFTs plus the one shared forward FFT
 * per epoch, not `window_bins` independent down-conversions. Empirically the
 * cheaper of the two realizations benchmarked for this
 * (a frequency-bank benchmark): ~1.2x-1.55x faster than
 * an equivalent tuned-mixer bank, measured with real doppler.spectral.FFT.
 * SNR margin in this mode comes entirely from auto-selected non-coherent
 * looks (magnitude-squared accumulation, immune to data-modulation sign
 * flips) rather than coherent depth, sized against an internal safety-valve
 * ceiling rather than a caller-supplied cap (the semi-analytical Pd model
 * this engine sizes against grows unreliable past a few hundred looks — not
 * a public knob to tune around that). `row` in @ref dp_correlator_hit_t
 * reports the frequency-window index (0 … window_bins-1, native FFT-bin
 * ordering) instead of a slow-time-FFT row when this mode is active at
 * coherent_bins=1, where `doppler_res_hz` is the per-window spacing
 * (chip_rate/sf). A burst engine sized to a coherent depth D > 1 inside the
 * tiles (a stated design C/N0) indexes the combined grid instead:
 * `window_bins*D` bins of `chip_rate/(sf*D)`, folded over dp_correlator_grid_bins().
 *
 * **Block-coherent depth inside the tiles** (docs/design/async-dsss-receiver.md
 * §2.3): the engine allows a coherent depth, to accommodate waveforms with
 * code-only windows. Given `code_only_epochs > 1` -- the whole code-only
 * epochs such a window holds at any chip phase -- it runs a coherent depth
 * `D` inside every tile: the per-tile epoch
 * correlations are gathered for `D` epochs, then a zero-padded slow-time
 * FFT per code-phase column turns each tile into `D` Doppler rows
 * `chip_rate/(sf*D)` apart, detected per block. Blocks are non-overlapping
 * and the engine does not know any emitter's window phase, so `D` is at
 * most `(code_only_epochs+1)/2` (a whole block always lands inside the
 * window) and, when `doppler_rate` is given, at most
 * `f_epoch/sqrt(2*doppler_rate)` (the drift over a block stays inside half a
 * row), rounded down to a 7-smooth length (dp_fft_smooth_floor()): `D` is a
 * slow-time FFT length, and a prime factor of 13 or more costs pocketfft its
 * slow path (doppler#2457). The Doppler axis is then ONE uniform grid of
 * `window_bins*coherent_bins` bins of `doppler_res_hz = chip_rate/(sf*D)`
 * over the tiled span, in native FFT-bin order (0 = DC, ascending, then
 * wrapping negative): `row` indexes it, and dp_correlator_build_handoff()
 * folds it with dp_fftfreq_index() over that count. A block that straddles
 * data spreads that emitter over its rows, `10*log10(D)` below an aligned
 * block, at its own code phase -- the `conc` probe
 * (docs/design/async-dsss-receiver.md §2.4) reads it.
 * `code_only_epochs = 1` (the default) is `D = 1` and the engine exactly as
 * described above. A tile de-rotates by its own centre, so an emitter on
 * the edge between two tiles reads the same in both (within 0.03 dB) and
 * the slow-time transform folds it to the same row index of each: every
 * listed peak is therefore asked at its ROW's frequency before it is
 * reported -- the block's raw epochs correlated with the replica at the
 * pick's code phase, mixed by the row's frequency and by that one span
 * down and up, the winner reported (docs/design/acq-surface.md §6,
 * docs/design/async-dsss-receiver-measurements.md §12.18,
 * doppler#1270).
 *
 * **A roll per thread** (docs/design/acq-surface.md §13): the tiles are
 * independent after the one forward transform, so the per-epoch tile loop
 * and, at `D > 1`, the
 * block-end column loop run across a persistent pool of workers
 * (dp_parallel.h's `dp_pool_*`), created once with the engine and parked
 * between pushes. Each tile owns its inverse plan and scratch, so the
 * result is bit-identical at any thread count. The per-cell passes that
 * decide a surface -- the magnitude, the CFAR reference, the working mask
 * and every scan of the peak list -- run per tile too, each into a slot of
 * its own, and are merged serially in tile order (a mean of the tiles'
 * means over equal cells, the first of the tiles' first maxima), so the
 * serial remainder is per tile, not per cell. dp_correlator_set_threads() sets the
 * count.
 *
 * @code
 * // header-code: skip=a fragment: samples is the caller's stream
 * // 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
 * uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
 * float nrz[31];
 * bin_to_nrz(code, 31, nrz, 31);           // chip 0 -> +1, chip 1 -> -1
 * float _Complex pre[124];
 * for (size_t i = 0; i < 124; i++)
 *   pre[i] = nrz[i / 4];                   // each chip held 4 samples
 * dp_correlator_state_t *a = dp_correlator_create_burst(pre, 124, 16, 4.0e6, 45.0,
 *                                   0.0, 1e-3, 0.9, 0, 0.0, 1);
 * dp_correlator_hit_t hits[64];
 * size_t nh = dp_correlator_push(a, samples, n_samples, hits, 64);
 * for (size_t i = 0; i < nh; i++)
 *   printf("Doppler %zu, code phase %zu, C/N0 %.1f dB-Hz\n",
 *          hits[i].row, hits[i].col,
 *          hits[i].cn0_dbhz_est);
 * dp_correlator_destroy(a);
 * @endcode
 *
 * The functions every flavor shares, on a search flavor (kept here rather
 * than in their own doc blocks, which every Python class of the
 * correlator renders):
 *
 * - dp_correlator_push() frames the search's own grid.  For every complete
 *   frame it applies the slow-time Doppler FFT, correlates against the
 *   reference, dumps the coherent surface (or, when n_noncoh > 1,
 *   accumulates |.|^2 over n_noncoh looks first), lists up to max_peaks
 *   picks above the auto-configured gate and writes one hit each, with
 *   every field filled: the Doppler bin and code phase as row and col, the
 *   statistic, the C/N0 estimate, the stream position, the hand-off chip
 *   phase and the rate hypothesis.  A frame that ends a dwell may write up
 *   to max_peaks hits, so it is taken only while at least
 *   min(max_peaks, max_results) slots are left (at least one); a frame
 *   that ends none needs no room while any slot is left.  A max_results
 *   under max_peaks still takes the dwell and keeps its strongest
 *   max_results picks: that loses results, never input.
 * - dp_correlator_consumed() is not a hit's samples_consumed, which is
 *   CUMULATIVE and counts only framed samples (the stream position a hit's
 *   epoch ended at): resuming from that would re-feed the carry.  It reads
 *   0 after a regrid too.
 * - dp_correlator_reset() drains the ring and clears the non-coherent
 *   power accumulator, the block in progress and the dwell bookkeeping, so
 *   the next push begins a fresh search from an empty ring; the grid, the
 *   thresholds and the reference are untouched.
 * - dp_correlator_set_threshold() refuses it: the gate is sized from Pfa
 *   and Pd at create, with the raw and non-coherent thresholds it implies.
 * - The state triplet: the blob is described with the others, under
 *   "Serializable state".
 */


#define DP_CORRELATOR_MAX_PEAKS 64u


#define DP_CORRELATOR_RATE_HYPS_MAX 64u

#define DP_CORRELATOR_N_NONCOH_SAFETY_CEILING 256u

#define DP_CORRELATOR_CN0_NONE NAN

dp_correlator_state_t *dp_correlator_create_burst (const float _Complex *tmpl, size_t n,
                               size_t reps, double fs, double cn0_dbhz,
                               double doppler_uncertainty, double pfa,
                               double pd, int noise_mode,
                               double doppler_rate,
                               size_t rate_hypotheses);

size_t dp_correlator_period_samples (size_t code_len);

size_t dp_correlator_search_period (size_t code_len, double sample_rate,
                             double chip_rate);

dp_correlator_state_t *dp_correlator_create_continuous (const uint8_t *code, size_t code_len,
                                    double sample_rate, double chip_rate,
                                    double symbol_rate, double cn0_dbhz,
                                    double doppler_uncertainty, double pfa,
                                    double pd, int noise_mode,
                                    size_t code_only_epochs,
                                    double doppler_rate);

typedef enum
{
  DP_CORRELATOR_FRAMING_NONE       = 0, 
  DP_CORRELATOR_FRAMING_CONTINUOUS = 1, 
  DP_CORRELATOR_FRAMING_BURST      = 2  
} dp_correlator_framing_t;

dp_correlator_state_t *dp_correlator_create (
    const float _Complex *ref, size_t ny, size_t nx,
    dp_correlator_framing_t framing, size_t doppler_rows, int detect,
    size_t dwell, size_t ny_out, size_t nx_out, int col_out,
    float threshold, size_t noise_lo, size_t noise_hi, int noise_mode,
    double sample_rate, double chip_rate, double symbol_rate, size_t reps,
    double cn0_dbhz, double doppler_uncertainty, double pfa, double pd,
    double doppler_rate, size_t rate_hypotheses, size_t code_only_epochs,
    int nthreads, const char **why);

double dp_correlator_rate_hypothesis (double span, size_t n, size_t h);

double _Complex dp_correlator_rate_phasor (double rate, double t);

size_t dp_correlator_rate_hypotheses_for (double f_epoch, double span,
                                   size_t depth);



int dp_correlator_configure_search_raw (dp_correlator_state_t *state, size_t doppler_bins,
                              size_t n_noncoh, const char **why);

int dp_correlator_set_max_peaks (dp_correlator_state_t *state, size_t n, const char **why);

int dp_correlator_set_carrier_freq_hz (dp_correlator_state_t *state, double carrier_freq_hz);

int dp_correlator_carrier_freq_ok (double carrier_freq_hz, double sample_rate_hz);

int dp_correlator_set_threads (dp_correlator_state_t *state, int n);
int dp_correlator_set_telemetry (dp_correlator_state_t *state, dp_tlm_t *tlm,
                       const char *prefix, uint32_t decim, const char **why);

size_t dp_correlator_surface (dp_correlator_state_t *state, float *out, size_t n_out);

size_t dp_correlator_surface_doppler_hz (dp_correlator_state_t *state, double *out,
                               size_t n_out);

size_t dp_correlator_surface_chip_phase (dp_correlator_state_t *state, double *out,
                               size_t n_out);

size_t dp_correlator_surface_complex (dp_correlator_state_t *state, float _Complex *out,
                            size_t n_out);

double _Complex dp_correlator_cell_corr (const dp_correlator_state_t *state,
                               const float _Complex *x, size_t col,
                               double f_hz, double t0);

void dp_correlator_cell_corr_grid (const dp_correlator_state_t *state, const float _Complex *x,
                         size_t n_epochs, size_t col, const double *f_hz,
                         size_t n_f, double t0, double _Complex *out);

size_t dp_correlator_block_prompt (dp_correlator_state_t *state, size_t tile, size_t col,
                         float _Complex *out, size_t n_out);

size_t dp_correlator_block_raw (dp_correlator_state_t *state, float _Complex *out,
                      size_t n_out);

int dp_correlator_set_surface_sink (dp_correlator_state_t *state, dp_correlator_surface_sink_fn fn,
                           void *ctx, uint32_t decim);

int dp_correlator_set_keep_surface (dp_correlator_state_t *state, int keep);



uint64_t dp_correlator_position (const dp_correlator_state_t *state);

typedef struct
{
  uint64_t samples_consumed; 
  double
      chip_phase; 
  double doppler_hz_est;  
  double doppler_res_hz;  
  double cn0_dbhz_est;    
  float  peak_mag;        
  float  noise_est;       
  float  test_stat;       
} dp_correlator_handoff_t;

/* The FFT-bin convention this engine reports in -- `0 = DC`, ascending
 * positive, then wrapping negative -- is `dp_fftfreq_index()` in
 * clib_common.h, and its doc comment there is the one definition. It was
 * declared here, and four call sites outside C restated the fold in three
 * mutually inconsistent ways; the engine's wideband search and its own
 * hand-off were two of them, which surfaced as a receiver reporting
 * `tracking == 1` while decoding noise. Every consumer -- this engine's
 * search, its hand-off, and any composing receiver -- now includes the
 * SAME inline rather than restating the formula. */

static inline size_t
dp_correlator_grid_bins (const dp_correlator_state_t *state)
{
  /* 0 off a search flavor, which has no Doppler grid (§18). */
  return state->search
             ? state->search->window_bins * state->search->coherent_bins
             : 0;
}

static inline double
dp_correlator_bin_doppler_hz (const dp_correlator_state_t *state, size_t doppler_bin)
{
  return (double)dp_fftfreq_index (doppler_bin, dp_correlator_grid_bins (state))
         * state->search->doppler_res_hz;
}

double dp_correlator_psl_db (const dp_correlator_state_t *state);

double dp_correlator_dsl_db (dp_correlator_state_t *state);

double dp_correlator_gate_stat (const dp_correlator_state_t *state);

void dp_correlator_build_handoff (const dp_correlator_state_t *state, const dp_correlator_hit_t *hit,
                        size_t code_len, dp_correlator_handoff_t *out);

double dp_correlator_sidelobe_ratio (dp_correlator_state_t     *state,
                              const dp_correlator_hit_t *from,
                              const dp_correlator_hit_t *to);

/* ── Serializable state — the elastic / pure-transducer face
 * ─────────────────
 *
 * The OO engine above is convenient but stateful.  These match the rest of
 * the library's serializable objects (lo/cic/fir/ddcr): serialize a
 * channel's cross-call state to a flat POD, ship (descriptor, state, input)
 * to any thread/process/pod, rebuild the engine from the descriptor
 * (acq_create), inject the state, and continue — bit-identical to an
 * uninterrupted run.
 */




int dp_correlator_state_position (const dp_correlator_state_t *state, const void *blob,
                           uint64_t *position);

size_t dp_correlator_run (dp_correlator_state_t *state, const void *state_in, void *state_out,
                const float _Complex *in, size_t n_in, dp_correlator_hit_t *result,
                size_t max_results);

int dp_correlator_get_keep_surface(const dp_correlator_state_t *state, int *out);

int dp_correlator_last_corr_status(const dp_correlator_state_t *state);
int dp_correlator_get_ny(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_nx(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_ny_out(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_nx_out(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_col_out(const dp_correlator_state_t *state, int *out);
int dp_correlator_get_doppler_step(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_n(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_dwell(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_count(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_ring_cap(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_noise_lo(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_noise_hi(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_threshold(const dp_correlator_state_t *state, float *out);
int dp_correlator_get_max_peaks(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_code_bins(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_doppler_bins(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_n_noncoh(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_eta(const dp_correlator_state_t *state, float *out);
int dp_correlator_get_eta_nc(const dp_correlator_state_t *state, float *out);
int dp_correlator_get_pfa_cell(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_pd_predicted(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_straddle_loss(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_sample_rate(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_cn0_dbhz(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_doppler_span_hz(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_doppler_res_hz(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_pd(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_underpowered(const dp_correlator_state_t *state, bool *out);
int dp_correlator_get_threads(const dp_correlator_state_t *state, int *out);
int dp_correlator_get_lag_bins(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_coherent_bins(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_sf(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_chip_rate(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_symbol_rate(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_epochs_per_symbol(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_n_peaks(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_n_held(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_peak_conc(const dp_correlator_state_t *state, float *out);
int dp_correlator_get_carrier_freq_hz(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_surface_rows(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_surface_at(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_reps(const dp_correlator_state_t *state, size_t *out);
int dp_correlator_get_pd_burst(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_psl_db(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_doppler_rate(const dp_correlator_state_t *state, double *out);
int dp_correlator_get_rate_hypotheses(const dp_correlator_state_t *state, size_t *out);
#ifdef __cplusplus
}
#endif

#endif /* DP_CORRELATOR_CORE_H */
```


