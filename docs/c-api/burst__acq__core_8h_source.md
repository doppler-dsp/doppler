

# File burst\_acq\_core.h

[**File List**](files.md) **>** [**burst\_acq**](dir_d3ec06985dce876581dd948705a4d1da.md) **>** [**burst\_acq\_core.h**](burst__acq__core_8h.md)

[Go to the documentation of this file](burst__acq__core_8h.md)


```C++

#ifndef BURST_ACQ_CORE_H
#define BURST_ACQ_CORE_H

#include "acq/acq_core.h"
#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    acq_state_t *engine;
  } burst_acq_state_t;

  burst_acq_state_t *burst_acq_create (const uint8_t *code, size_t code_len,
                                       size_t reps, size_t spc,
                                       double chip_rate, double cn0_dbhz,
                                       double doppler_uncertainty, double pfa,
                                       double pd, int noise_mode);

  void burst_acq_destroy (burst_acq_state_t *state);

  void burst_acq_reset (burst_acq_state_t *state);

  size_t burst_acq_push (burst_acq_state_t *state, const float complex *x,
                         size_t n_in, acq_result_t *result,
                         size_t max_results);

  int burst_acq_configure_search_raw (burst_acq_state_t *state,
                                      size_t doppler_bins, size_t n_noncoh);

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


