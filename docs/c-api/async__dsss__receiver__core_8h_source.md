

# File async\_dsss\_receiver\_core.h

[**File List**](files.md) **>** [**async\_dsss\_receiver**](dir_385ab33ef0b6337dfa5d36daa80c4b8c.md) **>** [**async\_dsss\_receiver\_core.h**](async__dsss__receiver__core_8h.md)

[Go to the documentation of this file](async__dsss__receiver__core_8h.md)


```C++

#ifndef ASYNC_DSSS_RECEIVER_CORE_H
#define ASYNC_DSSS_RECEIVER_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "acq/acq_core.h"
#include "carrier_acq/carrier_acq_core.h"
#include "cic/cic_core.h"
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "dp_state.h"
#include "hbdecim/hbdecim_core.h"
#include "lockdet/lockdet_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "spectral/spectral_core.h"
#include "corr/corr_core.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"
#include "ber/ber_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* SPEC-derived carrier-loop bandwidth for the pre-despread Costas
   * (hardcoded, not a public constructor param, matching this object's
   * "just works" philosophy). The refine-stage's frozen carrier never
   * calls costas_update(), so its own bn/zeta are irrelevant -- only its
   * tsamps (the wipe period length) and init_norm_freq matter.
   *
   * Wider than the 0.01 first used, for two reasons the narrow value could
   * not meet: (1) PULL-IN -- the refined seed still leaves ~tens of Hz of
   * residual (PSDMF accuracy + the ramp drifting the carrier over the
   * refine->track latency), and a per-code-period PLL at 0.01 has a pull-in
   * range far under that, so the residual wraps as a zero-mean sinusoid and
   * the loop never locks; (2) DYNAMICS -- tracking SPEC's 500 Hz/s ramp with
   * a per-period Type-II loop needs the bandwidth to keep the velocity lag
   * (phase error ~ ramp/omega_n^2) small. 0.04 pulls the seed in and rides
   * the ramp with a clean (~0 deg) despread constellation; the minimum that
   * works is ~0.03 (0.02 under-pulls). Because the phase detector is
   * |P|-normalized (unit-slope), this is the true noise bandwidth, so it
   * costs some phase noise at the 0-Doppler floor -- but the ~5 dB Es/N0
   * floor is still met.
   *
   * THE PULL-IN NUMBER IS NOW MEASURED, and the "~tens of Hz" above is the
   * assumption rather than the observation. `native/validation/costas_pullin.c`
   * sweeps this loop's acquisition range in multiples of the `bn/m` bound
   * (`m = 2`, the squaring discriminator) and re-derives it on every `make
   * test`. At bn = 0.04 over a 2046-sample code period that bound is
   * 9.775e-6 cyc/sample -- 60 Hz at SPEC's 6.138 MHz front end -- and the
   * loop acquires 100% inside it on BOTH signs, is still reliable at 2x, and
   * is dead by 4x. (Tighter than the MPSK carrier's measured 4x/5x envelope;
   * the two are separate sweeps for that reason.)
   *
   * What the assumption cost: doppler#982. The receiver's own hand-off
   * residual was measured at -54..+375 Hz, i.e. routinely 2-6x this bound,
   * so the loop is asked to acquire from outside its range on most draws and
   * the resulting failures read as an acquisition problem. Before widening
   * bn again, check the hand-off against the bound -- this value was tuned
   * empirically against the residual assumption that measurement contradicts.
   *
   * NO FLL: the pre-despread Costas always passes bn_fll = 0 (a pure PLL).
   * The FLL cross-product frequency discriminator is far too noisy on this
   * loop's data-modulated despread-symbol input. The phase discriminator is
   * NON-DATA-AIDED (squaring the emitted coherent-I&D partials) so it is
   * immune to the async data transitions that land inside nearly every code
   * period (see adr_track_period()); with that clean, transition-robust error
   * the plain PLL tracks the coupled Doppler (offset AND 500 Hz/s ramp)
   * pre-despread. So the FLL is not exposed here at all, not merely
   * defaulted off. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.04
  /* Dll's own bn: the validated stable code-loop bandwidth for the
   * one-update-per-partial tracking geometry -- same value DsssReceiver's
   * own Dll uses, not dll_create()'s own default of 0.01. (A wider 0.005 was
   * tried to chase sustained code-rate Doppler, but with the FLL removed the
   * carrier-driven slips are gone and the narrower 0.002 keeps its noise
   * immunity at the low-Es/N0 floor.) */
#define ASYNC_DSSS_RX_DLL_BN 0.002

  /* Symbol-lock detector on the emitted symbols. The lock signal is the
   * BPSK phase-lock statistic (I^2 - Q^2)/(I^2 + Q^2) = cos(2*phi) per
   * symbol (locked -> +1, 45deg-rotated/noise -> 0). The lock METRIC is that
   * signal integrated proportionally to SNR -- a power-weighted running mean,
   * EMA(I^2-Q^2)/EMA(I^2+Q^2), so high-|symbol| (high-SNR) symbols dominate
   * and noisy ones contribute little -- over a dwell of LOCK_DWELL symbols
   * (>= 30). A hysteretic lockdet (lockdet_core.h) then declares `locked`
   * after LOCK_N_UP consecutive symbols with the metric >= LOCK_UP and drops
   * it after LOCK_N_DOWN below LOCK_DOWN. */
#define ASYNC_DSSS_RX_LOCK_DWELL 30u
#define ASYNC_DSSS_RX_LOCK_UP 0.5
#define ASYNC_DSSS_RX_LOCK_DOWN 0.3
#define ASYNC_DSSS_RX_LOCK_N_UP 30u
#define ASYNC_DSSS_RX_LOCK_N_DOWN 15u

/* The state machine's values (`state` below, also the first byte of a
 * serialized blob's extra record). Searching -> refining -> tracking is the
 * searching flavor's path; hand-off mode starts at idle and seed() puts it
 * at refining; lost is reached from tracking by the release rule and left
 * only by reset(). */
#define ASYNC_DSSS_RX_SEARCHING 0
#define ASYNC_DSSS_RX_REFINING 1
#define ASYNC_DSSS_RX_TRACKING 2
#define ASYNC_DSSS_RX_IDLE 3
#define ASYNC_DSSS_RX_LOST 4

  typedef struct
  {
    acq_state_t *acq; 
    /* Refine stage: a frozen-carrier collection Dll feeding
     * CarrierAcquisition via a RateConverter. Rebuilt on every real
     * acquisition hit and on reset(); otherwise untouched. */
    costas_state_t         car_frozen;
    dll_state_t           *refine_dll;
    RateConverter_state_t *refine_rc;
    carrier_acq_state_t   *ca;
    size_t refine_segments; 
    uint64_t refine_samples_fed; 
    /* Scratch, not state -- sized once per refine-chain (re)build (no
     * allocation in the steps() hot path): refine_dll's own emitted
     * partials (capacity refine_segments), then refine_rc's resampled
     * output (capacity refine_segments*refine_rc->rate + margin). */
    float _Complex *refine_dll_out_buf;
    size_t         refine_dll_out_cap;
    float _Complex *refine_rc_out_buf;
    size_t         refine_rc_out_cap;

    /* Track stage: DsssReceiver's own composition. costas_init()'s tsamps is
     * one whole code period, and the pre-despread carrier loop updates once
     * per period from a non-data-aided (squaring) combine of that period's
     * coherent-I&D partials (see adr_track_period() in the .c file). */
    costas_state_t         car;
    dll_state_t           *dll;
    RateConverter_state_t *rc;
    mpsk_receiver_state_t *rx;
    /* Scratch, not state -- sized at every track-chain (re)build and never
     * in the steps() hot path (#1192, the section 11.5 rule): the live
     * Dll's partials for ONE code period, then rc's resampled output for
     * it. The chain runs a period at a time, so both are fixed by tsamps
     * and rc->rate; they only ever grow. */
    float complex *track_dll_out_buf;
    size_t         track_dll_out_cap;
    float complex *track_rc_out_buf;
    size_t         track_rc_out_cap;

    /* Shared carrier-wipe scratch/carry -- refine and track stages never
     * run concurrently and both wipe in whole `tsamps`-sample periods, so
     * one buffer set serves either. */
    size_t         tsamps; 
    float _Complex *car_wiped_buf;
    float _Complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code (same rationale as DsssReceiver's
     * own copy -- neither acq_create_continuous() nor dll_create()'s
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
    /* Refine-stage tuning, fixed at construction (see objects/
     * async_dsss_receiver.toml for the rationale behind each default --
     * all mirror freq_refine.refine_seed_carrier_acq()'s own already-
     * validated defaults verbatim). */
    double refine_max_error_db;
    size_t refine_samples_per_symbol;
    double refine_design_margin_db;
    size_t refine_n_fft;
    size_t refine_zero_pad;
    bool   refine_sequential;
    size_t refine_max_n_blocks;
    double carrier_freq_hz; 
    int state; 
    double   lost_confirm_s;       
    uint64_t lost_confirm_samples; 
    uint64_t state_samples;        
    uint64_t both_down_samples;    
    double   seed_chip_phase;     
    double   seed_doppler_hz_est; 
    double   doppler_hz_est;      
    double   cn0_dbhz_est;        
    uint64_t samples_fed;         
    /* Symbol-lock detector running state (see the ASYNC_DSSS_RX_LOCK_*
     * defines). lock_num/lock_den are the power-weighted EMAs of I^2-Q^2 and
     * I^2+Q^2; lock_metric = lock_num/lock_den = cos(2*phi); sym_lockdet is
     * the hysteretic up/down declare. Reset when the track chain is (re)built
     * (fresh lock per pass). */
    double          lock_num;
    double          lock_den;
    double          lock_metric;
    double          lock_alpha; 
    lockdet_state_t sym_lockdet;
  } async_dsss_receiver_state_t;

  async_dsss_receiver_state_t *async_dsss_receiver_create (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
      double pd, double doppler_uncertainty, size_t segments, size_t sps,
      int differential, double refine_max_error_db,
      size_t refine_samples_per_symbol, double refine_design_margin_db,
      size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s);

  async_dsss_receiver_state_t *async_dsss_receiver_create_handoff (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
      double pd, size_t segments, size_t sps, int differential,
      double refine_max_error_db, size_t refine_samples_per_symbol,
      double refine_design_margin_db, size_t refine_n_fft,
      size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s);

  void async_dsss_receiver_destroy (async_dsss_receiver_state_t *state);

  void async_dsss_receiver_reset (async_dsss_receiver_state_t *state);

  size_t async_dsss_receiver_steps_max_out (async_dsss_receiver_state_t *state);

  size_t async_dsss_receiver_steps (async_dsss_receiver_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    float _Complex *out, size_t max_out);

  int async_dsss_receiver_seed (async_dsss_receiver_state_t *state,
                                double chip_phase, double doppler_hz_est,
                                double cn0_dbhz_est);

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
  } async_dsss_receiver_status_t;

  async_dsss_receiver_status_t async_dsss_receiver_status (
      const async_dsss_receiver_state_t *state);

  int async_dsss_receiver_get_idle (const async_dsss_receiver_state_t *state);

  int async_dsss_receiver_get_lost (const async_dsss_receiver_state_t *state);

  int async_dsss_receiver_configure_search_raw (
      async_dsss_receiver_state_t *state, size_t doppler_bins,
      size_t n_noncoh);

  void async_dsss_receiver_configure_lock_raw (
      async_dsss_receiver_state_t *state, double up_thresh,
      double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
      uint32_t n_down);

  int async_dsss_receiver_configure_chain_raw (
      async_dsss_receiver_state_t *state, size_t segments, size_t sps,
      int n);

  int    async_dsss_receiver_get_tracking (
      const async_dsss_receiver_state_t *state);
  int    async_dsss_receiver_get_refining (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_doppler_hz (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_cn0_dbhz_est (
      const async_dsss_receiver_state_t *state);
  size_t async_dsss_receiver_get_segments (
      const async_dsss_receiver_state_t *state);
  size_t async_dsss_receiver_get_sps (const async_dsss_receiver_state_t *state);
  int    async_dsss_receiver_get_n (const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_chip_phase (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_code_rate (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_lock (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_norm_freq (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_nco_freq (
      const async_dsss_receiver_state_t *state);
  int async_dsss_receiver_get_locked (
      const async_dsss_receiver_state_t *state);
  int async_dsss_receiver_get_code_locked (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_car_last_error (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_car_nco_freq (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_mpsk_last_error (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_lock_metric (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_lock_threshold (
      const async_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car_frozen + refine_dll + refine_rc + ca + car +
   * dll + rc + rx, always all nine in the searching flavor and the eight
   * without acq in hand-off mode (a fixed shape per flavor, DsssReceiver's
   * own rationale). segments/sps/n/refine_segments and the flavor are the
   * layout key. */

  typedef struct
  {
    uint8_t  state;
    uint8_t  handoff; 
    uint8_t  _pad[6];
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
    lockdet_state_t sym_lockdet;  
  } async_dsss_receiver_extra_t;

#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 3u

  size_t async_dsss_receiver_state_bytes (
      const async_dsss_receiver_state_t *state);
  void   async_dsss_receiver_get_state (
      const async_dsss_receiver_state_t *state, void *blob);
  int async_dsss_receiver_set_state (async_dsss_receiver_state_t *state,
                                     const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_CORE_H */
```


