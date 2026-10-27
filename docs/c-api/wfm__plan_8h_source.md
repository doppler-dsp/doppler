

# File wfm\_plan.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_plan.h**](wfm__plan_8h.md)

[Go to the documentation of this file](wfm__plan_8h.md)


```C++

#ifndef WFM_PLAN_H
#define WFM_PLAN_H

#include "doppler/dp_complex.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct wfm_plan wfm_plan_t;

  wfm_plan_t *dp_wfm_plan_prepare (const char *spec_json);

  size_t dp_wfm_plan_len (const wfm_plan_t *p);

  size_t dp_wfm_plan_n_sources (const wfm_plan_t *p);

  uint64_t dp_wfm_plan_anchor_seed (const wfm_plan_t *p);

#define DP_WFM_PLAN_WHY_NO_NOISE                                              \
  "this scene carries no noise, so the Plan has no noise floor for snr to "   \
  "move: give a source a finite snr (below 100 dB) and an snr_mode"

  int dp_wfm_plan_check_snr (const wfm_plan_t *p);

  size_t dp_wfm_plan_render (const wfm_plan_t *p, const char *overrides_json,
                          float _Complex *out);

  size_t dp_wfm_plan_at (const wfm_plan_t *p, double snr, uint64_t seed,
                      float _Complex *out);

  size_t dp_wfm_plan_save_bytes (const wfm_plan_t *p);

  size_t dp_wfm_plan_save (const wfm_plan_t *p, void *blob);

  wfm_plan_t *dp_wfm_plan_restore (const void *blob, size_t n);

  int dp_wfm_plan_dump (const wfm_plan_t *p, const char *path);

  wfm_plan_t *dp_wfm_plan_load (const char *path);

  void dp_wfm_plan_destroy (wfm_plan_t *p);

#ifdef __cplusplus
}
#endif

#endif /* WFM_PLAN_H */
```


