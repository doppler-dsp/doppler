

# File state\_filter\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**state\_filter**](dir_f99952e39f9637fbb70decda0be42187.md) **>** [**state\_filter\_core.h**](state__filter__core_8h.md)

[Go to the documentation of this file](state__filter__core_8h.md)


```C++

#ifndef DP_STATE_FILTER_CORE_H
#define DP_STATE_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include <stdbool.h>
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

#define STATE_FILTER_SHAPE_A 2.0
#define STATE_FILTER_SHAPE_B 2.0

#define STATE_FILTER_K_MIN 0.5
#define STATE_FILTER_K_MAX 2.0
#define STATE_FILTER_DELAY_MAX 2.0
#define STATE_FILTER_BNT_MAX 0.12

#define STATE_FILTER_BNT_MIN 1e-30

#define STATE_FILTER_DELAY_LIMIT 4.0

typedef struct
{
  double kp;    
  double ki;    
  double kr;    
  double integ; 
  double rate;  
  double bn;    
  double t;     
} dp_state_filter_state_t;

int dp_state_filter_params_ok (double bn, double t);

void dp_state_filter_init (dp_state_filter_state_t *state, double bn,
                           double t);

int dp_state_filter_state_ok (const dp_state_filter_state_t *s);

dp_state_filter_state_t *dp_state_filter_create (double bn, double t);

const char *dp_state_filter_params_why (double bn, double t);

dp_state_filter_state_t *dp_state_filter_create_why (double bn, double t,
                                                     const char **why);

void dp_state_filter_destroy (dp_state_filter_state_t *state);

int dp_state_filter_configure (dp_state_filter_state_t *state, double bn,
                               double t);

void dp_state_filter_reset (dp_state_filter_state_t *state);

int dp_state_filter_loop_stable (int order, double kp, double ki, double kr,
                                 double delay, double gain);

double dp_state_filter_loop_bandwidth (int order, double kp, double ki,
                                       double kr, double delay, double gain);

double dp_state_filter_steady_error (int order, double ki, double kr,
                                     int input_order, double x);

bool dp_state_filter_stable (const dp_state_filter_state_t *state,
                             double delay, double gain);

double dp_state_filter_bandwidth (const dp_state_filter_state_t *state,
                                  double delay, double gain);

/* ── Serializable state (standard bytes interface; see dp_state.h) ────────
 * The envelope, a flavor u32, a pad u32, then kp ki kr integ rate bn t as
 * doubles. The pad word is written 0 and a blob with any other value is
 * refused, so it stays free to carry something later. The flavor word
 * is a reject key: 0 is the fixed-gain filter, and a blob of any other
 * flavor is refused, so a second flavor can add its own payload under the
 * same version.
 */
#define STATE_FILTER_STATE_MAGIC DP_FOURCC ('S', 'F', 'L', 'T')
#define STATE_FILTER_STATE_VERSION 1u

size_t dp_state_filter_state_bytes (const dp_state_filter_state_t *state);
void dp_state_filter_get_state (const dp_state_filter_state_t *state,
                                void *blob);
int dp_state_filter_set_state (dp_state_filter_state_t *state,
                               const void *blob);

JM_FORCEINLINE JM_HOT double
dp_state_filter_step (dp_state_filter_state_t *state, double x)
{
  state->rate += state->kr * x;
  state->integ += state->rate + state->ki * x;
  return state->integ + state->kp * x;
}

void dp_state_filter_steps (dp_state_filter_state_t *state, const double *x,
                            double *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_STATE_FILTER_CORE_H */
```


