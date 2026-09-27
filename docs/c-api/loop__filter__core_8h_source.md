

# File loop\_filter\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**loop\_filter**](dir_5a5f36bef1d931095e74791b275c008f.md) **>** [**loop\_filter\_core.h**](loop__filter__core_8h.md)

[Go to the documentation of this file](loop__filter__core_8h.md)


```C++

#ifndef DP_LOOP_FILTER_CORE_H
#define DP_LOOP_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double kp;    
    double ki;    
    double integ; 
    double bn;    
    double zeta;  
    double t;     
  } dp_loop_filter_state_t;

  void loop_filter_init(dp_loop_filter_state_t *state, double bn, double zeta,
                        double t);

  double loop_filter_wn(double bn, double zeta);

  dp_loop_filter_state_t *dp_loop_filter_create(double bn, double zeta, double t);

  void dp_loop_filter_destroy(dp_loop_filter_state_t *state);

  void dp_loop_filter_configure(dp_loop_filter_state_t *state, double bn, double zeta,
                             double t);

  void dp_loop_filter_reset(dp_loop_filter_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); config fields restore identically
   * into an identically-built instance, the integrator memory resumes exactly.
   */
#define LOOP_FILTER_STATE_MAGIC DP_FOURCC('L', 'P', 'F', 'L')
#define LOOP_FILTER_STATE_VERSION 1u

  size_t dp_loop_filter_state_bytes(const dp_loop_filter_state_t *state);
  void dp_loop_filter_get_state(const dp_loop_filter_state_t *state, void *blob);
  int dp_loop_filter_set_state(dp_loop_filter_state_t *state, const void *blob);

  JM_FORCEINLINE JM_HOT double
  dp_loop_filter_step (dp_loop_filter_state_t *state, double x)
  {
    state->integ += state->ki * x;
    return state->integ + state->kp * x;
  }

  void dp_loop_filter_steps (dp_loop_filter_state_t *state, const double *x,
                          double *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOOP_FILTER_CORE_H */
```


