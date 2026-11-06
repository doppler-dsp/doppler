

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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  DP_CORRELATOR_1D            = 1, 
  DP_CORRELATOR_2D            = 2, 
  DP_CORRELATOR_DELAY_DOPPLER = 3, 
  DP_CORRELATOR_DETECTOR_1D   = 4, 
  DP_CORRELATOR_DETECTOR_2D   = 5  
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
} dp_correlator_hit_t;

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
  /* Delay-Doppler flavor (dp_correlator_create_delay_doppler).  The flavor is
   * the fast path's accumulation plus the library's one delay-Doppler map
   * at dump time (docs/design/acq-surface.md §18); it is fixed at create()
   * and changes what the state blob means, so the blob carries its own
   * magic. */
  int              delay_doppler; 
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
} dp_correlator_state_t;

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
                      size_t n_in, float _Complex *out, size_t max_out);

/* ── Batches: many frames in one call (#2683) ──────────────────────────────
 *
 * A batch is H frames back to back, n_in = H * ny * nx: frame h is samples
 * h*ny*nx .. (h+1)*ny*nx - 1, each laid out as execute() takes it.  What a
 * hypothesis or a frame IS, is the caller's: the batch only knows H.  Each
 * map is n_out values (reshape to (ny_out, nx_out); Doppler is axis 0 and
 * delay axis 1 on a DelayDoppler), and map h is row h of the (H, n_out)
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
                              float _Complex *out, size_t max_out);

size_t dp_correlator_execute_coherent_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_coherent (dp_correlator_state_t *state,
                                   const float _Complex *in, size_t n_in,
                                   float _Complex *out, size_t max_out);

size_t dp_correlator_execute_power_sum_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_power_sum (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, size_t max_out);

size_t dp_correlator_execute_power_max_max_out (const dp_correlator_state_t *state);

size_t dp_correlator_execute_power_max (dp_correlator_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, uint32_t *out1,
                                    size_t max_out);

size_t dp_correlator_execute_peaks_max_out (const dp_correlator_state_t *state,
                                        size_t n_in);

size_t dp_correlator_execute_peaks (dp_correlator_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float *out, uint32_t *out1, uint32_t *out2,
                                float *out3, size_t max_out);

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
                                 size_t max_out);

size_t dp_correlator_bank_detect_max_out (dp_correlator_state_t *state,
                                      size_t x_len);

size_t dp_correlator_bank_detect (const dp_correlator_state_t *state,
                              const float _Complex *x, size_t x_len,
                              size_t L_d, size_t L_r, int g_min, int g_max,
                              float t_candidate, float t_confirm,
                              const float _Complex *rates, size_t rates_len,
                              float *out, int32_t *out1, uint32_t *out2,
                              uint32_t *out3, uint32_t *out4, size_t max_out);

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
 * from a release before the one correlator fails dp_state_validate. */
#define CORRELATOR_STATE_MAGIC DP_FOURCC ('C','O','R','L')
#define CORRELATOR_STATE_VERSION 1u
size_t dp_correlator_state_bytes (const dp_correlator_state_t *state);
void dp_correlator_get_state (const dp_correlator_state_t *state, void *blob);
int dp_correlator_set_state (dp_correlator_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DP_CORRELATOR_CORE_H */
```


