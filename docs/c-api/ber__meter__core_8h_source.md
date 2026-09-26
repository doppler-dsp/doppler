

# File ber\_meter\_core.h

[**File List**](files.md) **>** [**ber\_meter**](dir_88fe8aa1742881a2471cfb33f972e762.md) **>** [**ber\_meter\_core.h**](ber__meter__core_8h.md)

[Go to the documentation of this file](ber__meter__core_8h.md)


```C++

#ifndef DP_BER_METER_CORE_H
#define DP_BER_METER_CORE_H

#include "doppler/ber/ber_core.h" /* the records and the free functions */
#include "doppler/clib_common.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_complex.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BER_METER_STATE_MAGIC DP_FOURCC ('B', 'E', 'R', 'M')
#define BER_METER_STATE_VERSION 1u

  typedef struct
  {
    int      m;             
    int      bps;           
    size_t   target_errors; 
    double   conf;          
    uint8_t *truth;         
    size_t   truth_len;     
    /* running counters — this is what the state blob carries */
    size_t errors;     
    size_t symbols;    
    size_t bit_errors; 
    size_t bits;       
    size_t skipped;    
    size_t bursts;     
    /* the last alignment detected, and the marker geometry that found it —
       score() uses these rather than taking them from the caller, so a
       measurement cannot be handed an alignment that belongs to a different
       burst or a different marker */
    ber_align_t last;      
    size_t      mk_t0;     
    size_t      mk_n;      
    size_t      mk_period; 
  } dp_ber_meter_state_t;

  ber_align_t ber_align_detect (const float _Complex *rx, size_t rx_len,
                                const uint8_t *truth, size_t truth_len, int m,
                                size_t t0, size_t n_marker, size_t period,
                                int lag_span, double pfa);

  ber_interval_t ber_confidence (size_t errors, size_t symbols, double conf);

  /* ── the meter ────────────────────────────────────────────────────────── */

  dp_ber_meter_state_t *dp_ber_meter_create (int m, size_t target_errors,
                                       double conf);
  void               dp_ber_meter_destroy (dp_ber_meter_state_t *state);
  void dp_ber_meter_reset (dp_ber_meter_state_t *state);

  int dp_ber_meter_set_truth (dp_ber_meter_state_t *state, const uint8_t *truth,
                           size_t truth_len);

  ber_align_t ber_meter_detect (const dp_ber_meter_state_t *state,
                                const float _Complex *rx, size_t rx_len,
                                size_t t0, size_t n_marker, size_t period,
                                int lag_span, double pfa);

  int dp_ber_meter_align (dp_ber_meter_state_t *state, const float _Complex *rx,
                       size_t rx_len, size_t t0, size_t n_marker,
                       size_t period, int lag_span, double pfa);

  size_t dp_ber_meter_score (dp_ber_meter_state_t *state, const float _Complex *rx,
                          size_t rx_len, size_t lo, size_t hi);

  void ber_meter_set_align (dp_ber_meter_state_t *state, ber_align_t align,
                            size_t t0, size_t n_marker, size_t period);

  int dp_ber_meter_get_enough (const dp_ber_meter_state_t *state);

  ber_interval_t dp_ber_meter_interval (const dp_ber_meter_state_t *state,
                                     size_t errors, size_t symbols);

  ber_interval_t dp_ber_meter_ser (const dp_ber_meter_state_t *state);

  ber_interval_t dp_ber_meter_ber (const dp_ber_meter_state_t *state);

  size_t dp_ber_meter_get_errors (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_symbols (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_bit_errors (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_bits (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_skipped (const dp_ber_meter_state_t *state);
  int    dp_ber_meter_get_m (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_target_errors (const dp_ber_meter_state_t *state);
  int    dp_ber_meter_get_lag (const dp_ber_meter_state_t *state);
  double dp_ber_meter_get_phase (const dp_ber_meter_state_t *state);
  double dp_ber_meter_get_align_stat (const dp_ber_meter_state_t *state);
  double dp_ber_meter_get_align_margin_db (const dp_ber_meter_state_t *state);
  double dp_ber_meter_get_align_runner_db (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_align_occurrences (const dp_ber_meter_state_t *state);
  size_t dp_ber_meter_get_align_slips (const dp_ber_meter_state_t *state);
  int    dp_ber_meter_get_align_saturated (const dp_ber_meter_state_t *state);
  int    dp_ber_meter_get_align_ok (const dp_ber_meter_state_t *state);
  double dp_ber_meter_get_conf (const dp_ber_meter_state_t *state);

  size_t dp_ber_meter_state_bytes (const dp_ber_meter_state_t *state);
  void   dp_ber_meter_get_state (const dp_ber_meter_state_t *state, void *blob);
  int    dp_ber_meter_set_state (dp_ber_meter_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BER_METER_CORE_H */
```


