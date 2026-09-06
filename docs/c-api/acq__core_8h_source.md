

# File acq\_core.h

[**File List**](files.md) **>** [**acq**](dir_25a1e6db36731e5901b5cfb158eaa462.md) **>** [**acq\_core.h**](acq__core_8h.md)

[Go to the documentation of this file](acq__core_8h.md)


```C++

#ifndef ACQ_CORE_H
#define ACQ_CORE_H

#include "buffer/buffer.h"
#include "clib_common.h"
#include "corr2d/corr2d_core.h"
#include "detection/detection_core.h"
#include "dp_state.h"
#include "fft/fft_core.h"
#include "jm_perf.h"
/* detector2d_core.h supplies det_noise_mode_t (guarded typedef). */
#include "detector2d/detector2d_core.h"
#include "fft2d/fft2d_core.h"
#include "dp_parallel.h"
#include "dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t doppler_bin; 
    size_t code_phase; 
    float  peak_mag;   
    float  noise_est;  
    float  test_stat;  
    float cn0_dbhz_est;        
    uint64_t samples_consumed; 
  } acq_result_t;

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
  } acq_tlm_t;

  typedef void (*acq_surface_sink_fn) (void *ctx, const float *surface,
                                       size_t rows, size_t cols,
                                       uint64_t samples_consumed);

  typedef struct
  {
    float  ref;  
    size_t best; 
  } acq_part_t;

  typedef struct
  {
    corr2d_state_t *corr; 
    fft_state_t *slow_fft; 
    dp_f32_t    *ring;  
    float _Complex *ref; 
    float _Complex *yframe;  
    float _Complex *colbuf;  
    float _Complex *colout;  
    float _Complex *out_buf; 
    float *mag_buf;       
    float *noise_scratch; 
    float *nc_surface;    
    /* Wideband mode only (window_bins > 1) — see the file doc comment.
     * Independent of corr/slow_fft/yframe/colbuf/colout above (unused, but
     * left allocated at their trivial coherent_bins=1 size, in this mode). */
    fft_state_t *wide_fwd; 
    fft_state_t *wide_inv; 
    float _Complex
        *wide_ref_spec; 
    float _Complex
        *wide_spec; 
    float _Complex *wide_prod; 
    size_t
        coherent_bins; 
    size_t window_bins; 
    size_t code_bins; 
    size_t n; 
    size_t n_surf; 
    size_t interp; 
    size_t frame_n; 
    size_t sf;      
    size_t spc;     
    size_t reps;    
    size_t
        searched_bins; 
    size_t n_noncoh;   
    size_t nc_count; 
    size_t ring_cap; 
    size_t noise_lo; 
    size_t noise_hi; 
    det_noise_mode_t noise_mode; 
    double chip_rate; 
    double fs;        
    double cn0_dbhz;  
    double
        doppler_span_hz; 
    double
        doppler_res_hz; 
    double pfa;         
    double doppler_uncertainty; 
    double symbol_rate; 
    double epochs_per_symbol;  
    size_t code_only_epochs; 
    double doppler_rate; 
    float _Complex *blk; 
    size_t blk_epoch;    
    /* The roll per thread (design §2.3): the tiles are independent after
       the one forward transform, so the per-epoch tile loop and the
       block-end column loop run through a persistent pool. The scratch is
       PER TILE, not per thread -- a pocketfft plan carries its own work
       buffers, and a tile lands on whichever worker takes it -- so the
       serial and the fanned paths run the same code on the same buffers
       and the surface is bit-identical either way. */
    dp_pool_t       *pool;      
    int              threads;   
    fft_state_t    **tile_inv;  
    float _Complex **tile_prod; 
    fft_state_t    **tile_slow; 
    float _Complex **tile_col;  
    size_t **tile_rows;         
    acq_part_t *parts;          
    float  threshold; 
    float  eta;       
    float  eta_nc;    
    double pfa_cell;  
    double pd;        
    double pd_predicted;  
    double straddle_loss; 
    uint8_t underpowered; 
    uint64_t
        samples_consumed; 
    /* The peak list (docs/design/async-dsss-receiver.md §7.1): up to
       `max_peaks` peaks per dwell, each above the same gate, strongest
       first, with an exclusion zone of one Doppler row (`interp` surface
       rows) by one chip (`spc` columns), circular, around each; every
       listed peak is one acq_result_t. `band_mask` marks the cells outside
       the searched Doppler band (rebuilt with the thresholds); `peak_mask`
       is the per-dwell working copy the list marks its zones into. The
       two-epoch rule: a peak within a chip of an already-listed peak's code
       phase, at any row, is that emitter's candidate twin -- held, not
       listed, unless it was there at the same row on the previous dwell,
       listed or held. `twin_*` are the previous dwell's picks (native
       rows). */
    size_t      max_peaks; 
    size_t      n_peaks;   
    det_peak_t *peaks;     
    uint8_t    *band_mask; 
    uint8_t    *peak_mask; 
    uint32_t   *twin_row;  
    uint32_t   *twin_col;  
    size_t      n_twins;   
    /* Observability (design §2.4): attach-on-demand, nothing in blobs. */
    acq_tlm_t tlm;       
    int       keep_surface; 
    float *stat_surface; 
    uint64_t surface_at; 
    uint64_t dwells;     
    acq_surface_sink_fn sink; 
    void    *sink_ctx;
    uint32_t sink_decim; 
    size_t   n_held;     
    float    peak_conc;  
    /* Last-dump bookkeeping (for inspection): the strongest pick. */
    size_t peak_row;
    size_t peak_col;
    float  peak_mag;
    float  noise_est;
    float  test_stat;
  } acq_state_t;

  typedef struct
  {
    uint16_t has_nc; 
    uint16_t _pad;
    uint32_t n_noncoh;         
    uint64_t n;                
    uint64_t samples_consumed; 
    uint32_t nc_count;     
    uint32_t n_unconsumed; 
    uint32_t max_peaks;    
    uint32_t n_twins;      
    uint32_t blk_epoch; 
  } acq_extra_t;

#define ACQ_STATE_MAGIC DP_FOURCC ('A', 'C', 'Q', 'R')
#define ACQ_STATE_VERSION 3u /* v3: the block-coherent accumulator rides along */

#define ACQ_MAX_PEAKS 64u

#define ACQ_COL_CHUNK 32u

#define ACQ_N_NONCOH_SAFETY_CEILING 256u

  acq_state_t *acq_create_burst (const uint8_t *code, size_t code_len,
                                 size_t reps, size_t spc, double chip_rate,
                                 double cn0_dbhz, double doppler_uncertainty,
                                 double pfa, double pd, int noise_mode);

  acq_state_t *acq_create_continuous (const uint8_t *code, size_t code_len,
                                      size_t spc, double chip_rate,
                                      double symbol_rate, double cn0_dbhz,
                                      double doppler_uncertainty, double pfa,
                                      double pd, int noise_mode,
                                      size_t code_only_epochs,
                                      double doppler_rate);

  void acq_destroy (acq_state_t *state);

  void acq_reset (acq_state_t *state);

  int acq_configure_search_raw (acq_state_t *state, size_t doppler_bins,
                                size_t n_noncoh);

  int acq_set_max_peaks (acq_state_t *state, size_t n);

  int acq_set_threads (acq_state_t *state, int n);
  int acq_set_telemetry (acq_state_t *state, dp_tlm_t *tlm,
                         const char *prefix, uint32_t decim);

  size_t acq_surface (acq_state_t *state, float *out, size_t n_out);

  size_t acq_surface_doppler_hz (acq_state_t *state, double *out,
                                 size_t n_out);

  size_t acq_surface_chip_phase (acq_state_t *state, double *out,
                                 size_t n_out);

  void acq_set_surface_sink (acq_state_t *state, acq_surface_sink_fn fn,
                             void *ctx, uint32_t decim);

  size_t acq_push (acq_state_t *state, const float _Complex *x, size_t n_in,
                   acq_result_t *result, size_t max_results);

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
  } acq_handoff_t;

  /* The FFT-bin convention this engine reports in -- `0 = DC`, ascending
   * positive, then wrapping negative -- is `dp_fftfreq_index()` in
   * clib_common.h, and its doc comment there is the one definition. It was
   * declared here, and four call sites outside C restated the fold in three
   * mutually inconsistent ways; the engine's wideband search and its own
   * hand-off were two of them, which surfaced as a receiver reporting
   * `tracking == 1` while decoding noise. Every consumer -- this engine's
   * search, its hand-off, and any composing receiver -- now includes the
   * SAME inline rather than restating the formula. */

  void acq_build_handoff (const acq_state_t *state, const acq_result_t *hit,
                          size_t code_len, size_t spc, acq_handoff_t *out);

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

  size_t acq_state_bytes (const acq_state_t *state);

  void acq_get_state (const acq_state_t *state, void *blob);

  int acq_set_state (acq_state_t *state, const void *blob);

  size_t acq_run (acq_state_t *state, const void *state_in, void *state_out,
                  const float _Complex *in, size_t n_in, acq_result_t *result,
                  size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* ACQ_CORE_H */
```


