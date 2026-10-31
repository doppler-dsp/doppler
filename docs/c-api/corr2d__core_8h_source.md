

# File corr2d\_core.h

[**File List**](files.md) **>** [**corr2d**](dir_ac96ca94cfbb355eb7a82b081cfe387c.md) **>** [**corr2d\_core.h**](corr2d__core_8h.md)

[Go to the documentation of this file](corr2d__core_8h.md)


```C++

#ifndef DP_CORR2D_CORE_H
#define DP_CORR2D_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/fft/fft_core.h"
#include "doppler/fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
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
   * corr2d_zeropad_1d; ztmp/zcol/zcolout are unused (2-axis-pad only). */
  float _Complex *work_pad;  
  float _Complex *ztmp;      
  float _Complex *zcol;      
  float _Complex *zcolout;   
  /* Single-row-reference fast path (see the file doc comment for the
   * identity this relies on).  fast_path is decided once at create() and
   * fixed for the object's lifetime; set_ref() may only refresh within the
   * same mode (see dp_corr2d_set_ref doc comment). */
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
  size_t count;             
  /* Known-column output (see dp_corr2d_create's @p col_out).  A caller that
   * already knows the correlation lag it wants does not need the other
   * nx_out-1 columns, and evaluating the inverse at one bin is a dot
   * product against the conjugated reference, with NO transform in either
   * direction -- O(nx) per row against the full map's O(nx log nx), which
   * is what makes the kernel affordable for a caller that would otherwise
   * hand-roll the lag sum beside it. */
  int             col_out;   
  float _Complex *col_ref;   
  float _Complex *work_trunc;
  /* Worker fan-out (#2666).  The fast path's per-row stage and the
   * delay-Doppler flavor's per-column stage are each a set of independent
   * transforms, run over `nworkers` contiguous chunks on `pool`.  A pocketfft
   * plan keeps scratch of its own, so it cannot be shared between threads:
   * every worker owns one plan per stage, and chunk t always runs on plan t,
   * which is what keeps the output bit-identical across thread counts. */
  int              nworkers;  
  dp_pool_t       *pool;      
  dp_fft_state_t **row_fwd;   
  dp_fft_state_t **row_inv;   
  /* Delay-Doppler flavor (dp_corr2d_create_delay_doppler).  The flavor is
   * the fast path plus a slow-time FFT down every delay column at dump
   * time; it is fixed at create() and changes what the state blob means,
   * so the blob carries its own magic. */
  int              delay_doppler; 
  double           doppler_step;  
  dp_fft_state_t **slow_fft;  
  float _Complex  *col_gather;
  float _Complex  *col_spec;  
  /* Batch scratch (dp_corr2d_execute_fold and the typed batch calls),
   * allocated on the first batch, one slot per worker.  A batch makes its
   * maps here and never touches accum or count, so it is not state and is
   * not in the blob. */
  float _Complex  *bat_acc;   
  float _Complex  *bat_work;  
  float _Complex  *bat_pad;   
  float _Complex  *bat_map;   
} dp_corr2d_state_t;

dp_corr2d_state_t *dp_corr2d_create(const float _Complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out, int col_out);

dp_corr2d_state_t *dp_corr2d_create_delay_doppler (const float _Complex *code,
                                                   size_t code_len, size_t ny,
                                                   size_t dwell, int nthreads,
                                                   size_t ny_out);

void dp_corr2d_destroy(dp_corr2d_state_t *state);

void dp_corr2d_reset(dp_corr2d_state_t *state);

int dp_corr2d_set_ref(dp_corr2d_state_t *state, const float _Complex *ref);

size_t dp_corr2d_execute_max_out(dp_corr2d_state_t *state);

size_t dp_corr2d_execute(dp_corr2d_state_t *state, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out);

/* ── Batches: many frames in one call (#2683) ──────────────────────────────
 *
 * A batch is H frames back to back, n_in = H * ny * nx.  Map h is exactly
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
} dp_corr2d_fold_t;

size_t dp_corr2d_execute_fold (dp_corr2d_state_t *state,
                               const float _Complex *in, size_t n_in,
                               const dp_corr2d_fold_t *fold);

size_t dp_corr2d_execute_all_max_out (const dp_corr2d_state_t *state,
                                      size_t n_in);

size_t dp_corr2d_execute_all (dp_corr2d_state_t *state,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

size_t dp_corr2d_execute_coherent_max_out (const dp_corr2d_state_t *state);

size_t dp_corr2d_execute_coherent (dp_corr2d_state_t *state,
                                   const float _Complex *in, size_t n_in,
                                   float _Complex *out, size_t max_out);

size_t dp_corr2d_execute_power_sum_max_out (const dp_corr2d_state_t *state);

size_t dp_corr2d_execute_power_sum (dp_corr2d_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, size_t max_out);

size_t dp_corr2d_execute_power_max_max_out (const dp_corr2d_state_t *state);

size_t dp_corr2d_execute_power_max (dp_corr2d_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, uint32_t *out1,
                                    size_t max_out);

size_t dp_corr2d_execute_peaks_max_out (const dp_corr2d_state_t *state,
                                        size_t n_in);

size_t dp_corr2d_execute_peaks (dp_corr2d_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float *out, uint32_t *out1, uint32_t *out2,
                                float *out3, size_t max_out);

/* ── The hypothesis bank (#2683 phase 3) ───────────────────────────────── */

size_t dp_corr2d_bank_fold (const dp_corr2d_state_t *state, const float _Complex *x,
                            size_t x_len, size_t L, int g_min, int g_max,
                            const float _Complex *rates, size_t rates_len,
                            const dp_corr2d_fold_t *fold);

size_t dp_corr2d_bank_power_max_max_out (dp_corr2d_state_t *state,
                                         size_t x_len);

size_t dp_corr2d_bank_power_max (const dp_corr2d_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 size_t L, int g_min, int g_max,
                                 const float _Complex *rates,
                                 size_t rates_len, float *out, uint32_t *out1,
                                 size_t max_out);

size_t dp_corr2d_bank_detect_max_out (dp_corr2d_state_t *state,
                                      size_t x_len);

size_t dp_corr2d_bank_detect (const dp_corr2d_state_t *state,
                              const float _Complex *x, size_t x_len,
                              size_t L_d, size_t L_r, int g_min, int g_max,
                              float t_candidate, float t_confirm,
                              const float _Complex *rates, size_t rates_len,
                              float *out, int32_t *out1, uint32_t *out2,
                              uint32_t *out3, uint32_t *out4, size_t max_out);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * running 2-D product-spectrum accumulator + frame count;
 * FFT plans + ref_spec are config, rebuilt by create. */
#define CORR2D_STATE_MAGIC DP_FOURCC ('C','R','2','D')
#define CORR2D_STATE_VERSION 1u
#define DELAY_DOPPLER_STATE_MAGIC DP_FOURCC ('D','D','M','P')
#define DELAY_DOPPLER_STATE_VERSION 1u
size_t dp_corr2d_state_bytes (const dp_corr2d_state_t *state);
void dp_corr2d_get_state (const dp_corr2d_state_t *state, void *blob);
int dp_corr2d_set_state (dp_corr2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
```


