

# File async\_dsss\_receiver\_core.h

[**File List**](files.md) **>** [**async\_dsss\_receiver**](dir_565c73e0c8995e904663dd9fb6485ceb.md) **>** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md)

[Go to the documentation of this file](async__dsss__receiver__core_8h.md)


```C++

#ifndef DP_ASYNC_DSSS_RECEIVER_CORE_H
#define DP_ASYNC_DSSS_RECEIVER_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_state.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/dp_complex.h"
#include <stdbool.h>
#include <stddef.h>
#include "doppler/psd/psd_core.h"
#include "doppler/detector/detector_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/corr/corr_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* The pre-despread Costas's loop bandwidth (not a constructor parameter).
   * The refine stage's frozen carrier never calls costas_update(), so its
   * own bn/zeta are irrelevant -- only its tsamps and init_norm_freq matter.
   *
   * 0.04 is set by pull-in and dynamics, not jitter: the refined seed
   * leaves a residual the loop must pull in, and tracking SPEC's 500 Hz/s
   * ramp with a per-period Type-II loop needs the bandwidth to keep the
   * velocity lag (~ramp/omega_n^2) small. It rides the ramp with a clean
   * despread constellation; about 0.03 is the least that works. The phase
   * detector is |P|-normalized, so this is the true noise bandwidth, and
   * the ~5 dB Es/N0 floor is still met.
   *
   * The pull-in range is measured: native/validation/costas_pullin.c sweeps
   * it in multiples of the `bn/m` bound (`m = 2`, the squaring
   * discriminator), its spot check on every push (CI's sweep job, `make
   * test-sweep`). At bn = 0.04 over a 2046-sample code period the bound is
   * 9.775e-6 cycles/sample, 60 Hz at SPEC's 6.138 MHz front end; the loop
   * acquires 100% inside it on both signs, is reliable to 2x and dead by 4x.
   * The receiver's hand-off residual was measured at -54..+375 Hz, routinely
   * 2-6x the bound (doppler#982): check the hand-off against the bound
   * before widening bn.
   *
   * No FLL: the cross-product frequency discriminator is far too noisy on
   * this loop's data-modulated input, and the squaring phase discriminator
   * is immune to the transitions, so the plain PLL tracks the coupled
   * Doppler. The FLL is not exposed at all. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.04
#define ASYNC_DSSS_RX_CARRIER_PULLIN_HZ(chip_rate, code_len)                  \
  (ASYNC_DSSS_RX_BN_CARRIER * (chip_rate) / (2.0 * (double)(code_len)))
  /* The Dll's bn, per epoch whatever the steer rate (the symbol-period aid
   * steers once per symbol and keeps bn per epoch): DsssReceiver's value,
   * narrower than dp_dll_create()'s default 0.01 for noise immunity at the
   * low-Es/N0 floor. */
#define ASYNC_DSSS_RX_DLL_BN 0.002
  /* The tracking Dll's early-late spacing, chips. */
#define ASYNC_DSSS_RX_DLL_SPACING 0.5
  /* The damping of both Dlls this receiver builds, the tracker and the
   * refine collector, and the collector's early-late spacing, chips. Named
   * once because create() checks the very arguments each build site passes
   * (dp_dll_params_ok(), doppler#2103). */
#define ASYNC_DSSS_RX_DLL_ZETA 0.707
#define ASYNC_DSSS_RX_REFINE_DLL_SPACING 0.5
  /* The demodulator, MpskReceiver, as the track chain builds it: its
   * carrier loop's bn (symbol-rate normalised) and damping, its timing
   * loop's bn (chosen on steady-state EVM, see the build site in
   * async_dsss_receiver_core.c), its lock threshold, and MpskReceiver's
   * default RRC arguments, which the integrate-and-dump pulse does not
   * read. Named once so a harness that builds the same demodulator alone
   * (native/validation/async_dsss_receiver_loss.c) cannot drift from it. */
#define ASYNC_DSSS_RX_DEMOD_RRC_BETA 0.35
#define ASYNC_DSSS_RX_DEMOD_RRC_SPAN 8
#define ASYNC_DSSS_RX_DEMOD_BN_CARRIER 0.01
#define ASYNC_DSSS_RX_DEMOD_ZETA 0.707
#define ASYNC_DSSS_RX_DEMOD_BN_TIMING 0.005
#define ASYNC_DSSS_RX_DEMOD_LOCK_THRESH 0.3

#define ASYNC_DSSS_RX_REFINE_MIN_BLOCKS 7u

#define ASYNC_DSSS_RX_MAX_PEAKS 16u

#define ASYNC_DSSS_RX_MAX_EXCLUSIONS 64u

/* Symbol-lock detector on the emitted symbols. The lock signal is the
 * BPSK phase-lock statistic (I^2 - Q^2)/(I^2 + Q^2) = cos(2*phi) per
 * symbol (locked -> +1, 45deg-rotated/noise -> 0). The lock METRIC is that
 * signal integrated proportionally to SNR -- a power-weighted running mean,
 * EMA(I^2-Q^2)/EMA(I^2+Q^2), so high-|symbol| (high-SNR) symbols dominate
 * and noisy ones contribute little -- over a dwell of LOCK_DWELL symbols
 * (>= 30). A hysteretic lockdet (lockdet_core.h) then declares `locked`
 * after LOCK_N_UP consecutive symbols with the metric strictly above
 * LOCK_UP, and drops it after LOCK_N_DOWN below LOCK_DOWN. */
#define ASYNC_DSSS_RX_LOCK_DWELL 30u
#define ASYNC_DSSS_RX_LOCK_UP 0.5
#define ASYNC_DSSS_RX_LOCK_DOWN 0.3
#define ASYNC_DSSS_RX_LOCK_N_UP 30u
#define ASYNC_DSSS_RX_LOCK_N_DOWN 15u

/* The state machine's values (`state` below, also the first byte of a
 * serialized blob's extra record). Searching -> refining -> tracking, and
 * seed() puts a searching receiver at refining; lost is reached from tracking by the release rule and left
 * only by reset(). With reacquire the release skips lost and goes straight
 * back to searching. */
/* SSOT: enum=async_dsss_rx_state */
typedef enum
{
  ASYNC_DSSS_RX_SEARCHING = 0, 
  ASYNC_DSSS_RX_REFINING  = 1, 
  ASYNC_DSSS_RX_TRACKING  = 2, 
  /* 3 was the deleted cell mode's idle. The values are persisted (a blob,
     a telemetry capture), so it stays reserved: no state reaches it, and a
     blob that names it is refused. */
  ASYNC_DSSS_RX_RESERVED = 3,
  ASYNC_DSSS_RX_LOST     = 4, 
} async_dsss_rx_state_t;

  typedef struct
  {
    dp_acq_state_t *acq; 
    /* Refine stage: a frozen-carrier collection Dll feeding
     * CarrierAcquisition via a RateConverter. Rebuilt on every real
     * acquisition hit and on reset(); otherwise untouched. */
    dp_costas_state_t         car_frozen;
    dp_dll_state_t           *refine_dll;
    dp_RateConverter_state_t *refine_rc;
    dp_carrier_acq_state_t   *ca;
    size_t refine_segments; 
    uint64_t refine_samples_fed; 
    uint64_t refine_periods;     
    /* Scratch, not state -- sized once per refine-chain (re)build (no
     * allocation in the steps() hot path): refine_dll's own emitted
     * partials (capacity refine_segments), then refine_rc's resampled
     * output (capacity refine_segments*refine_rc->rate + margin). */
    float _Complex *refine_dll_out_buf;
    size_t         refine_dll_out_cap;
    float _Complex *refine_rc_out_buf;
    size_t         refine_rc_out_cap;

    /* Track stage: DsssReceiver's own composition. dp_costas_init()'s tsamps is
     * one whole code period, and the pre-despread carrier loop updates once
     * per period from a non-data-aided (squaring) combine of that period's
     * coherent-I&D partials (see adr_track_period() in the .c file). */
    dp_costas_state_t         car;
    dp_dll_state_t           *dll;
    dp_RateConverter_state_t *rc;
    dp_mpsk_receiver_state_t *rx;
    /* Scratch, not state -- sized at every track-chain (re)build and never
     * in the steps() hot path (#1192, docs/design/async-dsss-receiver.md
     * §11.5): the live
     * Dll's partials for ONE code period, then rc's resampled output for
     * it. The chain runs a period at a time, so both are fixed by tsamps
     * and rc->rate; they only ever grow. */
    float _Complex *track_dll_out_buf;
    size_t         track_dll_out_cap;
    float _Complex *track_rc_out_buf;
    size_t         track_rc_out_cap;

    /* Shared carrier-wipe scratch/carry -- refine and track stages never
     * run concurrently and both wipe in whole `tsamps`-sample periods, so
     * one buffer set serves either. */
    size_t         tsamps; 
    float _Complex *car_wiped_buf;
    float _Complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code (same rationale as DsssReceiver's
     * own copy -- neither dp_acq_create_continuous() nor dp_dll_create()'s
     * borrow-vs-copy semantics are part of their public contract). */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across every chain rebuild. */
    size_t spc;
    int    m;
    int    differential;
    size_t segments; 
    size_t sps;      
    int    n;        
    double chip_rate;
    double symbol_rate;
    double cn0_dbhz; 
    double pfa;      
    double pd;       
    /* Refine-stage tuning, fixed at construction (see each on
     * dp_async_dsss_receiver_create()). */
    double refine_max_error_db;
    size_t refine_samples_per_symbol;
    double refine_design_margin_db;
    size_t refine_n_fft;
    size_t refine_zero_pad;
    bool   refine_sequential;
    size_t refine_max_n_blocks;
    size_t refine_min_blocks; 
    double carrier_freq_hz; 
    int state; 
    double   lost_confirm_s;       
    uint64_t lost_confirm_samples; 
    bool     reacquire;            
    uint64_t state_samples;        
    double   code_only_s;  
    double   doppler_rate; 
    uint64_t samples_in;   
    bool     align_pending; 
    size_t   rank;   
    size_t   n_excl; 
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; 
    uint64_t refused_hits;         
    uint64_t both_down_samples;    
    int      had_lock;             
    int      car_coasting;         
    dp_costas_state_t car_held;       
    double   seed_chip_phase;     
    double   seed_doppler_hz_est; 
    double   doppler_hz_est;      
    double   cn0_dbhz_est;        
    /* Symbol-lock detector running state (see the ASYNC_DSSS_RX_LOCK_*
     * defines). lock_num/lock_den are the power-weighted EMAs of I^2-Q^2 and
     * I^2+Q^2; lock_metric = lock_num/lock_den = cos(2*phi); sym_lockdet is
     * the hysteretic up/down declare. Reset when the track chain is (re)built
     * (fresh lock per pass). */
    double          lock_num;
    double          lock_den;
    double          lock_metric;
    double          lock_alpha; 
    dp_lockdet_state_t sym_lockdet;

    /* Telemetry: the attach REQUEST, kept because every seed destroys and
     * rebuilds the tracking chain -- the loops it holds are re-attached
     * from here (dp_async_dsss_receiver_set_telemetry). Borrowed, and never
     * in a blob: a handle to the outside world is not state. */
    dp_tlm_t *tlm;                         
    char      tlm_prefix[DP_TLM_NAME_MAX]; 
    uint32_t  tlm_decim;                   
    int32_t   tlm_sym_lock;   
    int32_t   tlm_sym_locked; 
    int32_t   tlm_state;      
    int32_t   tlm_handover;   
    int32_t   tlm_refused;    
  } dp_async_dsss_receiver_state_t;

  dp_async_dsss_receiver_state_t *dp_async_dsss_receiver_create (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
      double pd, double doppler_uncertainty, size_t segments, size_t sps,
      int differential, double refine_max_error_db,
      size_t refine_samples_per_symbol, double refine_design_margin_db,
      size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s, bool reacquire, double code_only_s,
      double doppler_rate);

  void dp_async_dsss_receiver_destroy (dp_async_dsss_receiver_state_t *state);

  void dp_async_dsss_receiver_reset (dp_async_dsss_receiver_state_t *state);

  int dp_async_dsss_receiver_set_telemetry (
      dp_async_dsss_receiver_state_t *state, dp_tlm_t *tlm, const char *prefix,
      uint32_t decim);

  size_t dp_async_dsss_receiver_steps_max_out (dp_async_dsss_receiver_state_t *state);

  size_t dp_async_dsss_receiver_steps (dp_async_dsss_receiver_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    float _Complex *out, size_t max_out);

  int dp_async_dsss_receiver_seed (dp_async_dsss_receiver_state_t *state,
                                double chip_phase, double doppler_hz_est,
                                double cn0_dbhz_est);

  int dp_async_dsss_receiver_seed_ok (const dp_async_dsss_receiver_state_t *state,
                                      double chip_phase,
                                      double doppler_hz_est);

  typedef struct
  {
    int state; 
    double doppler_hz; 
    double chip_phase; 
    double code_rate;  
    double cn0_dbhz_est; 
    int    code_locked;  
    int    locked;       
    double lock_metric;  
    double lock_threshold; 
    double car_last_error; 
    double mpsk_last_error; 
    uint64_t state_samples; 
    uint64_t both_down_samples; 
    uint64_t refused_hits;      
  } async_dsss_receiver_status_t;

  async_dsss_receiver_status_t dp_async_dsss_receiver_status (
      const dp_async_dsss_receiver_state_t *state);

  int dp_async_dsss_receiver_get_lost (const dp_async_dsss_receiver_state_t *state);

  int dp_async_dsss_receiver_configure_search_raw (
      dp_async_dsss_receiver_state_t *state, size_t doppler_bins,
      size_t n_noncoh);

  int dp_async_dsss_receiver_set_refine_min_blocks (
      dp_async_dsss_receiver_state_t *state, size_t n_blocks);

  int dp_async_dsss_receiver_set_threads (
      dp_async_dsss_receiver_state_t *state, int n);

  int dp_async_dsss_receiver_set_rank (dp_async_dsss_receiver_state_t *state,
                                       size_t rank);

  size_t dp_async_dsss_receiver_get_rank (
      const dp_async_dsss_receiver_state_t *state);

  int dp_async_dsss_receiver_set_exclusions (
      dp_async_dsss_receiver_state_t *state, const double *phases,
      size_t phases_len);

  size_t dp_async_dsss_receiver_get_n_exclusions (
      const dp_async_dsss_receiver_state_t *state);

  int dp_async_dsss_receiver_get_threads (
      const dp_async_dsss_receiver_state_t *state);

  size_t dp_async_dsss_receiver_get_coherent_bins (
      const dp_async_dsss_receiver_state_t *state);

  void dp_async_dsss_receiver_configure_lock_raw (
      dp_async_dsss_receiver_state_t *state, double up_thresh,
      double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
      uint32_t n_down);

  int dp_async_dsss_receiver_configure_chain_raw (
      dp_async_dsss_receiver_state_t *state, size_t segments, size_t sps,
      int n);

  int    dp_async_dsss_receiver_get_tracking (
      const dp_async_dsss_receiver_state_t *state);
  int    dp_async_dsss_receiver_get_refining (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_doppler_hz (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_cn0_dbhz_est (
      const dp_async_dsss_receiver_state_t *state);
  size_t dp_async_dsss_receiver_get_segments (
      const dp_async_dsss_receiver_state_t *state);
  size_t dp_async_dsss_receiver_get_sps (const dp_async_dsss_receiver_state_t *state);
  int    dp_async_dsss_receiver_get_n (const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_chip_phase (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_code_rate (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_lock (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_norm_freq (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  int dp_async_dsss_receiver_get_locked (
      const dp_async_dsss_receiver_state_t *state);
  int dp_async_dsss_receiver_get_code_locked (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_car_last_error (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_car_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_mpsk_last_error (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_lock_metric (
      const dp_async_dsss_receiver_state_t *state);
  double dp_async_dsss_receiver_get_lock_threshold (
      const dp_async_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car_frozen + refine_dll + refine_rc + ca + car +
   * car_held + dll + rc + rx, a fixed shape; segments/sps/n/
   * refine_segments are the layout key. */

  typedef struct
  {
    uint8_t  state;
    uint8_t  had_lock;     
    uint8_t  car_coasting; 
    uint8_t  _pad[5]; 
    double   seed_chip_phase;
    double   seed_doppler_hz_est;
    double   doppler_hz_est;
    double   cn0_dbhz_est;
    uint64_t segments;
    uint64_t sps;
    uint64_t n;
    uint64_t refine_segments;
    uint64_t refine_samples_fed;
    uint64_t car_carry_len;
    uint64_t state_samples;
    uint64_t both_down_samples;
    double   lock_num;  
    double   lock_den;  
    double   lock_metric;         
    dp_lockdet_state_t sym_lockdet;  
    uint64_t refused_hits; 
    uint64_t refine_min_blocks; 
    uint64_t samples_in;    
    uint64_t align_pending; 
    int64_t  threads;       
    uint64_t rank;          
    uint64_t n_excl;        
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; 
  } async_dsss_receiver_extra_t;

#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 13u /* v13: rank, exclusions; v12: samples_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine_min_blocks; v8: refused_hits; v7: no hand-off flavor; v6: the cell pull-in */

  size_t dp_async_dsss_receiver_state_bytes (
      const dp_async_dsss_receiver_state_t *state);
  void   dp_async_dsss_receiver_get_state (
      const dp_async_dsss_receiver_state_t *state, void *blob);
  int dp_async_dsss_receiver_set_state (dp_async_dsss_receiver_state_t *state,
                                     const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_CORE_H */
```


