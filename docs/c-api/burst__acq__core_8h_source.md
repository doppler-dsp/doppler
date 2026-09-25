

# File burst\_acq\_core.h

[**File List**](files.md) **>** [**burst\_acq**](dir_55efb80a743a0f920c38ee6730a791eb.md) **>** [**burst\_acq\_core.h**](burst__acq__core_8h.md)

[Go to the documentation of this file](burst__acq__core_8h.md)


```C++

#ifndef BURST_ACQ_CORE_H
#define BURST_ACQ_CORE_H

#include "doppler/acq/acq_core.h"
#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/cvt/cvt_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    acq_state_t *engine;
    uint8_t underpowered;
  } burst_acq_state_t;

  burst_acq_state_t *burst_acq_create (const float _Complex *preamble,
                                       size_t preamble_len, size_t reps,
                                       double fs,
                                       double cn0_dbhz,
                                       double doppler_uncertainty, double pfa,
                                       double pd, int noise_mode,
                                       double doppler_rate);

  void burst_acq_destroy (burst_acq_state_t *state);

  void burst_acq_reset (burst_acq_state_t *state);

  size_t burst_acq_push (burst_acq_state_t *state, const float _Complex *x,
                         size_t n_in, acq_result_t *result,
                         size_t max_results);

  int burst_acq_configure_search_raw (burst_acq_state_t *state,
                                      size_t doppler_bins, size_t n_noncoh);

  int burst_acq_set_max_peaks (burst_acq_state_t *state, size_t n);

  /* ── Serializable state — forwards straight to the embedded engine's own
   * triplet (the serialized bytes ARE the shared acq_state_t's own state;
   * no separate format needed). */

  size_t burst_acq_state_bytes (const burst_acq_state_t *state);
  void   burst_acq_get_state (const burst_acq_state_t *state, void *blob);
  int    burst_acq_set_state (burst_acq_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BURST_ACQ_CORE_H */
```


