

# File burst\_capture\_core.h

[**File List**](files.md) **>** [**burst\_capture**](dir_8eab18aa96a66319f16718502165a0b6.md) **>** [**burst\_capture\_core.h**](burst__capture__core_8h.md)

[Go to the documentation of this file](burst__capture__core_8h.md)


```C++

#ifndef BURST_CAPTURE_CORE_H
#define BURST_CAPTURE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "buffer/buffer.h"
#include "dp_state.h"
#include "burst_acq/burst_acq_core.h"
#include "acq/acq_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "fft/fft_core.h"
#include "detection/detection_core.h"
#include "pn/pn_core.h"

#define BURST_CAPTURE_HITS 16u

#define BURST_CAPTURE_STATE_MAGIC DP_FOURCC ('B', 'C', 'A', 'P')
#define BURST_CAPTURE_STATE_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint64_t preamble_start; 
  double   doppler_hz_est; 
  double   doppler_res_hz; 
  double   cn0_dbhz_est;   
  double   refine_margin;  
} burst_capture_event_t;

typedef struct
{
  uint64_t epoch;      
  double   doppler_hz; 
  double   cn0_dbhz;   
  double   test_stat;  
  double   peak_mag;   
} burst_capture_detection_t;

typedef struct
{
  uint64_t anchor;     
  uint64_t start;      
  double   doppler_hz; 
  double   cn0_dbhz;   
  double   margin;     
  double   peak_mag;   
  int      refined;    
} burst_capture_pending_t;

typedef struct
{
  /* ── Configuration, copied at create() ──────────────────────────────── */
  uint8_t *acq_code;     
  size_t   acq_code_len; 
  size_t   reps;         
  size_t   spc;          
  double   chip_rate;    
  /* ── Derived geometry ───────────────────────────────────────────────── */
  size_t code_period; 
  size_t burst_len;   
  /* ── The composed child ─────────────────────────────────────────────── */
  burst_acq_state_t *acq; 
  /* ── Look-back (docs/design/dsss-burst-receiver.md §7.1) ────────────── */
  dp_f32_t *hist;      
  uint64_t samples_fed; 
  /* ── The event describing the most recent window emitted ────────────── */
  uint64_t preamble_start; 
  double   doppler_hz_est; 
  double   doppler_res_hz; 
  double   cn0_dbhz_est;   
  double   refine_margin;  
  /* ── Refine scratch (docs/design/dsss-burst-receiver.md §3.4) ───────── */
  float *ref_sign;   
  float _Complex *corr_buf; 
  size_t refine_span;  
  size_t corr_len;     
  size_t min_gap;      
  size_t retain_span;  
  size_t chunk_max;    
  size_t k_lo;         
  size_t k_hi;         
  /* ── Detections in flight ────────────────────────────────────────────
   * Only detections whose burst window has NOT yet arrived live here: every
   * one whose window HAS arrived is emitted before push() returns, which is
   * what bounds retention (see the trim rule in the implementation). */
  burst_capture_pending_t *q; 
  size_t q_cap;   
  size_t q_head;  
  size_t pending; 
  /* ── The windows of the LAST push ────────────────────────────────────
   * Scratch, deliberately NOT serialized: it describes the most recent
   * push() only, so keeping it out of the blob is what lets state_bytes()
   * stay a pure function of configuration.
   *
   * The windows are COPIED here rather than left in the ring. A window in
   * the ring is a borrow whose lifetime the retention rule would have to
   * extend across the whole call, and one push can complete several bursts
   * -- so the ring would have to hold every one of them at once, which its
   * derived capacity does not promise. The cost is one memcpy per BURST,
   * not per sample, which is a different order of magnitude from the copy
   * §6.1 weighs (that one is the whole stream). It is also what lets a C
   * consumer borrow a window through burst_capture_window() and hand it
   * onward with no further copy. */
  burst_capture_detection_t *det;     
  size_t                     det_cap; 
  size_t                     det_len; 
  float _Complex *win;     
  size_t          win_cap; 
  burst_capture_event_t *ev; 
  size_t ev_cap;           
  size_t ev_len;           
  uint64_t suppress_until; 
  size_t acq_blob_max;     
  /* ── Persistence (docs/design/burst-capture.md §9) ───────────────────── */
  int backed;   
  int recovered; 
  /* ── Diagnostics ────────────────────────────────────────────────────
   * Mirrored from the engine at create() rather than read through it on
   * demand, because jm's declared warning needs a bare bool field on THIS
   * struct -- the reason the sibling BurstAcquisition's copy of the same
   * warning has to be a hand-patch in its fragment (see the note at the top
   * of objects/burst_acq.toml). */
  int underpowered; 
  /* ── Bookkeeping ────────────────────────────────────────────────────── */
  uint64_t dropped;  
  uint64_t n_bursts; 
/*<<property_struct_fields>>*/
} burst_capture_state_t;

burst_capture_state_t *burst_capture_create (const uint8_t *acq_code,
                                             size_t acq_code_len,
                                             size_t burst_len, size_t reps,
                                             size_t spc, double chip_rate,
                                             double cn0_dbhz,
                                             double doppler_uncertainty,
                                             double pfa, double pd,
                                             int noise_mode);

burst_capture_state_t *
burst_capture_create_backed (const char *path, const uint8_t *acq_code,
                             size_t acq_code_len, size_t burst_len,
                             size_t reps, size_t spc, double chip_rate,
                             double cn0_dbhz, double doppler_uncertainty,
                             double pfa, double pd, int noise_mode);

void burst_capture_destroy (burst_capture_state_t *state);

void burst_capture_reset (burst_capture_state_t *state);

size_t burst_capture_push_max_out (burst_capture_state_t *state,
                                   size_t x_len);

size_t burst_capture_push (burst_capture_state_t *state,
                           const float complex *x, size_t x_len,
                           float complex *out, size_t max_out);

size_t burst_capture_detections_max_out (burst_capture_state_t *state,
                                         size_t n);

size_t burst_capture_detections (burst_capture_state_t *state, size_t n,
                                 burst_capture_detection_t *out,
                                 size_t max_out);

size_t burst_capture_events_max_out (burst_capture_state_t *state, size_t n);

size_t burst_capture_events (burst_capture_state_t *state, size_t n,
                             burst_capture_event_t *out, size_t max_out);

size_t burst_capture_ready (const burst_capture_state_t *state);

const float complex *burst_capture_window (const burst_capture_state_t *state,
                                           size_t i);

const burst_capture_event_t *
burst_capture_event_at (const burst_capture_state_t *state, size_t i);

int burst_capture_configure_search_raw (burst_capture_state_t *state,
                                        size_t doppler_bins,
                                        size_t n_noncoh);

/* ── Serializable state — the elastic / pure-transducer face ──────────── */

/* ── The search this capture will do, as numbers ──────────────────────
 *
 * A capture is only as good as the search under it, and a caller sizing a
 * link needs to see that search rather than infer it. These forward the
 * engine's own figures: what a detection must clear, how deep the sizer
 * went, and how wide in Doppler and code phase it will look.
 *
 * They are read-backs, not knobs -- every one is derived at create() from
 * the parameters above, and `configure_search_raw()` is the one call that
 * moves them. */

size_t burst_capture_get_min_gap (const burst_capture_state_t *state);

double burst_capture_get_eta (const burst_capture_state_t *state);
double burst_capture_get_eta_nc (const burst_capture_state_t *state);
double burst_capture_get_straddle_loss (const burst_capture_state_t *state);
double burst_capture_get_pd_predicted (const burst_capture_state_t *state);
size_t burst_capture_get_doppler_bins (const burst_capture_state_t *state);
size_t burst_capture_get_n_noncoh (const burst_capture_state_t *state);
size_t burst_capture_get_code_bins (const burst_capture_state_t *state);
double burst_capture_get_doppler_span_hz (const burst_capture_state_t *state);

size_t burst_capture_state_bytes (const burst_capture_state_t *state);
void burst_capture_get_state (const burst_capture_state_t *state, void *blob);
int burst_capture_set_state (burst_capture_state_t *state, const void *blob);

uint64_t burst_capture_get_preamble_start(const burst_capture_state_t *state);
double burst_capture_get_doppler_hz_est(const burst_capture_state_t *state);
double burst_capture_get_doppler_res_hz(const burst_capture_state_t *state);
double burst_capture_get_cn0_dbhz_est(const burst_capture_state_t *state);
double burst_capture_get_refine_margin(const burst_capture_state_t *state);
size_t burst_capture_get_pending(const burst_capture_state_t *state);
uint64_t burst_capture_get_dropped(const burst_capture_state_t *state);
uint64_t burst_capture_get_n_bursts(const burst_capture_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* BURST_CAPTURE_CORE_H */
```


