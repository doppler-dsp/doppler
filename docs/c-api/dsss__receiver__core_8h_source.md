

# File dsss\_receiver\_core.h

[**File List**](files.md) **>** [**dsss\_receiver**](dir_39e39d42b234cb6483b3a80e996300fe.md) **>** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md)

[Go to the documentation of this file](dsss__receiver__core_8h.md)


```C++

#ifndef DSSS_RECEIVER_CORE_H
#define DSSS_RECEIVER_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "acq/acq_core.h"
#include "cic/cic_core.h"
#include "dll/dll_core.h"
#include "dp_state.h"
#include "hbdecim/hbdecim_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
#include <complex.h>
#include <stddef.h>
#include "costas/costas_core.h"
#include "snr/snr_core.h"
#include "ber/ber_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    acq_state_t           *acq;
    dll_state_t           *dll;
    RateConverter_state_t *rc;
    mpsk_receiver_state_t *rx;

    /* Pre-despread carrier loop -- embedded by value, same pattern
     * despreader_state_t's own `car` uses. Rebuilt alongside dll/rc/rx
     * (create-time placeholder, every real hit, configure_chain_raw(),
     * reset()). One update per code period (`tsamps` samples). */
    costas_state_t car;
    size_t         tsamps; 
    /* Scratch, not state: sized `tsamps`, allocated once (never per
     * steps() call -- no allocation in the hot streaming path). */
    float complex *car_wiped_buf;
    /* Genuinely stateful: raw samples handed to steps() that didn't fill
     * a whole code period yet, carried to the NEXT call so the carrier
     * loop always wipes exact, complete periods regardless of how a
     * caller chunks its input (this object's own "any block size, state
     * carries across calls" contract). Capacity `tsamps`; only the first
     * `car_carry_len` samples are valid. Must be serialized (see below)
     * or a resumed instance silently loses its period alignment. */
    float complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code -- acq_create_continuous()/
     * dll_create()'s own borrow-vs-copy semantics aren't part of either's
     * public contract,
     * so this object keeps a persistent copy rather than depend on being
     * able to read it back out of a child, or on the caller's original
     * buffer outliving construction. */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across a dll/rc/rx rebuild (create-time or
     * configure_chain_raw()) — everything mpsk_receiver_create() and
     * dll_create() need that isn't re-derived from the acquisition hit. */
    size_t spc;
    int    m;
    int    differential;
    size_t segments; 
    size_t sps;      
    int    n;        
    double chip_rate;
    double symbol_rate;

    int      tracking; 
    double   doppler_hz_est; 
    double   cn0_dbhz_est;   
    uint64_t samples_fed;    
  } dsss_receiver_state_t;

  /* SPEC-derived defaults for the pre-despread carrier loop (bn<=0.01
   * rule + this session's own validated test_costas_core.c/floor-sweep
   * bn_fll=0.03 calibration point) -- hardcoded, not public constructor
   * params, matching how Dll's own bn=0.002/zeta/spacing are already
   * hardcoded (this object's "just works" philosophy: only the signal's
   * physical parameters are required). */
#define DSSS_RX_BN_CARRIER 0.01
#define DSSS_RX_BN_FLL 0.03

  dsss_receiver_state_t *
  dsss_receiver_create (const uint8_t *code, size_t code_len, double chip_rate,
                        double symbol_rate, size_t spc, int m, double cn0_dbhz,
                        double pfa, double pd, double doppler_uncertainty,
                        size_t segments, size_t sps, int differential);

  void dsss_receiver_destroy (dsss_receiver_state_t *state);

  void dsss_receiver_reset (dsss_receiver_state_t *state);

  size_t dsss_receiver_steps_max_out (dsss_receiver_state_t *state);

  size_t dsss_receiver_steps (dsss_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  int dsss_receiver_configure_search_raw (dsss_receiver_state_t *state,
                                          size_t                 doppler_bins,
                                          size_t                 n_noncoh);

  void dsss_receiver_configure_lock_raw (dsss_receiver_state_t *state,
                                         double up_thresh, double down_thresh,
                                         size_t n_looks, double alpha,
                                         uint32_t n_up, uint32_t n_down);

  int dsss_receiver_configure_chain_raw (dsss_receiver_state_t *state,
                                         size_t segments, size_t sps, int n);

  int    dsss_receiver_get_tracking (const dsss_receiver_state_t *state);
  double dsss_receiver_get_doppler_hz (const dsss_receiver_state_t *state);
  double dsss_receiver_get_cn0_dbhz_est (const dsss_receiver_state_t *state);
  size_t dsss_receiver_get_segments (const dsss_receiver_state_t *state);
  size_t dsss_receiver_get_sps (const dsss_receiver_state_t *state);
  int    dsss_receiver_get_n (const dsss_receiver_state_t *state);
  double dsss_receiver_get_chip_phase (const dsss_receiver_state_t *state);
  double dsss_receiver_get_code_rate (const dsss_receiver_state_t *state);
  double dsss_receiver_get_lock (const dsss_receiver_state_t *state);
  double dsss_receiver_get_norm_freq (const dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car + dll + rc + rx, always all five (a fixed
   * shape -- see the state struct's own doc comment for why `tracking`
   * doesn't gate child presence here). `segments`/`sps`/`n` are the
   * layout key: set_state rejects a blob whose grid disagrees with the
   * live engine's, the same way ddc_extra_t's `rate` is checked before
   * touching any child. `car_carry_len` sizes the variable-length carry
   * buffer packed after `extra` (length-prefixed, 0..tsamps-1 samples) --
   * losing it on resume would desync the carrier loop's period
   * alignment, so it's part of the fixed-layout `extra` struct even
   * though the buffer bytes themselves are variable-length. */

  typedef struct
  {
    uint8_t  tracking;
    uint8_t  _pad[7];
    double   doppler_hz_est;
    double   cn0_dbhz_est;
    uint64_t segments;
    uint64_t sps;
    uint64_t n;
    uint64_t car_carry_len;
  } dsss_receiver_extra_t;

#define DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('D', 'S', 'R', 'X')
#define DSSS_RECEIVER_STATE_VERSION 2u /* v2: pre-despread costas_state_t
                                           car + carry buffer added */

  size_t dsss_receiver_state_bytes (const dsss_receiver_state_t *state);
  void   dsss_receiver_get_state (const dsss_receiver_state_t *state,
                                  void                        *blob);
  int dsss_receiver_set_state (dsss_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DSSS_RECEIVER_CORE_H */
```


