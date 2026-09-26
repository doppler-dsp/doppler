

# File dsss\_receiver\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md) **>** [**dsss\_receiver\_core.h**](dsss__receiver__core_8h.md)

[Go to the documentation of this file](dsss__receiver__core_8h.md)


```C++

#ifndef DP_DSSS_RECEIVER_CORE_H
#define DP_DSSS_RECEIVER_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_state.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/dp_complex.h"
#include <stddef.h>
#include "doppler/costas/costas_core.h"
#include "doppler/snr/snr_core.h"
#include "doppler/ber/ber_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    dp_acq_state_t           *acq;
    dp_dll_state_t           *dll;
    dp_RateConverter_state_t *rc;
    dp_mpsk_receiver_state_t *rx;

    /* Pre-despread carrier loop -- embedded by value, same pattern
     * dp_despreader_state_t's own `car` uses. Rebuilt alongside dll/rc/rx
     * (create-time placeholder, every real hit, configure_chain_raw(),
     * reset()). One update per code period (`tsamps` samples). */
    dp_costas_state_t car;
    size_t         tsamps; 
    /* Scratch, not state: sized `tsamps`, allocated once (never per
     * steps() call -- no allocation in the hot streaming path). */
    float _Complex *car_wiped_buf;
    /* Genuinely stateful: raw samples handed to steps() that didn't fill
     * a whole code period yet, carried to the NEXT call so the carrier
     * loop always wipes exact, complete periods regardless of how a
     * caller chunks its input (this object's own "any block size, state
     * carries across calls" contract). Capacity `tsamps`; only the first
     * `car_carry_len` samples are valid. Must be serialized (see below)
     * or a resumed instance silently loses its period alignment. */
    float _Complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code -- acq_create_continuous()/
     * dp_dll_create()'s own borrow-vs-copy semantics aren't part of either's
     * public contract,
     * so this object keeps a persistent copy rather than depend on being
     * able to read it back out of a child, or on the caller's original
     * buffer outliving construction. */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across a dll/rc/rx rebuild (create-time or
     * configure_chain_raw()) — everything dp_mpsk_receiver_create() and
     * dp_dll_create() need that isn't re-derived from the acquisition hit. */
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
  } dp_dsss_receiver_state_t;

  /* SPEC-derived defaults for the pre-despread carrier loop (bn<=0.01
   * rule + this session's own validated test_costas_core.c/floor-sweep
   * bn_fll=0.03 calibration point) -- hardcoded, not public constructor
   * params, matching how Dll's own bn=0.002/zeta/spacing are already
   * hardcoded (this object's "just works" philosophy: only the signal's
   * physical parameters are required). */
#define DSSS_RX_BN_CARRIER 0.01
#define DSSS_RX_BN_FLL 0.03

  dp_dsss_receiver_state_t *
  dp_dsss_receiver_create (const uint8_t *code, size_t code_len, double chip_rate,
                        double symbol_rate, size_t spc, int m, double cn0_dbhz,
                        double pfa, double pd, double doppler_uncertainty,
                        size_t segments, size_t sps, int differential);

  void dp_dsss_receiver_destroy (dp_dsss_receiver_state_t *state);

  void dp_dsss_receiver_reset (dp_dsss_receiver_state_t *state);

  size_t dp_dsss_receiver_steps_max_out (dp_dsss_receiver_state_t *state);

  size_t dp_dsss_receiver_steps (dp_dsss_receiver_state_t *state,
                              const float _Complex *x, size_t x_len,
                              float _Complex *out, size_t max_out);

  int dp_dsss_receiver_configure_search_raw (dp_dsss_receiver_state_t *state,
                                          size_t                 doppler_bins,
                                          size_t                 n_noncoh);

  void dp_dsss_receiver_configure_lock_raw (dp_dsss_receiver_state_t *state,
                                         double up_thresh, double down_thresh,
                                         size_t n_looks, double alpha,
                                         uint32_t n_up, uint32_t n_down);

  int dp_dsss_receiver_configure_chain_raw (dp_dsss_receiver_state_t *state,
                                         size_t segments, size_t sps, int n);

  int    dp_dsss_receiver_get_tracking (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_doppler_hz (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_cn0_dbhz_est (const dp_dsss_receiver_state_t *state);
  size_t dp_dsss_receiver_get_segments (const dp_dsss_receiver_state_t *state);
  size_t dp_dsss_receiver_get_sps (const dp_dsss_receiver_state_t *state);
  int    dp_dsss_receiver_get_n (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_chip_phase (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_code_rate (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_lock (const dp_dsss_receiver_state_t *state);
  double dp_dsss_receiver_get_norm_freq (const dp_dsss_receiver_state_t *state);

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
#define DSSS_RECEIVER_STATE_VERSION 2u /* v2: pre-despread dp_costas_state_t
                                           car + carry buffer added */

  size_t dp_dsss_receiver_state_bytes (const dp_dsss_receiver_state_t *state);
  void   dp_dsss_receiver_get_state (const dp_dsss_receiver_state_t *state,
                                  void                        *blob);
  int dp_dsss_receiver_set_state (dp_dsss_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DSSS_RECEIVER_CORE_H */
```


