

# File loop\_filter\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**loop\_filter**](dir_6fa6397534e50a536c96f665c3cf0441.md) **>** [**loop\_filter\_core.h**](loop__filter__core_8h.md)

[Go to the documentation of this file](loop__filter__core_8h.md)


```C++

#ifndef LOOP_FILTER_CORE_H
#define LOOP_FILTER_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
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
  } loop_filter_state_t;

  void loop_filter_init(loop_filter_state_t *state, double bn, double zeta,
                        double t);

  loop_filter_state_t *loop_filter_create(double bn, double zeta, double t);

  void loop_filter_destroy(loop_filter_state_t *state);

  void loop_filter_configure(loop_filter_state_t *state, double bn, double zeta,
                             double t);

  void loop_filter_reset(loop_filter_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); config fields restore identically
   * into an identically-built instance, the integrator memory resumes exactly.
   */
#define LOOP_FILTER_STATE_MAGIC DP_FOURCC('L', 'P', 'F', 'L')
#define LOOP_FILTER_STATE_VERSION 1u

  size_t loop_filter_state_bytes(const loop_filter_state_t *state);
  void loop_filter_get_state(const loop_filter_state_t *state, void *blob);
  int loop_filter_set_state(loop_filter_state_t *state, const void *blob);

  JM_FORCEINLINE JM_HOT double
  loop_filter_step (loop_filter_state_t *state, double x)
  {
    state->integ += state->ki * x;
    return state->integ + state->kp * x;
  }

  void loop_filter_steps (loop_filter_state_t *state, const double *input,
                          double *output, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOOP_FILTER_CORE_H */
```


