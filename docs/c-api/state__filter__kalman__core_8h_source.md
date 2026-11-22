

# File state\_filter\_kalman\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**state\_filter\_kalman**](dir_cec9ba98649e30568bd09d31ea5df0bb.md) **>** [**state\_filter\_kalman\_core.h**](state__filter__kalman__core_8h.md)

[Go to the documentation of this file](state__filter__kalman__core_8h.md)


```C++

#ifndef DP_STATE_FILTER_KALMAN_CORE_H
#define DP_STATE_FILTER_KALMAN_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/state_filter/state_filter_core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

/* ── The Kalman flavor: its own component ───────────────────────────────
 * It calls the fixed-gain component (state_filter_core.h): the base struct
 * is the first member, and the update, the exact stability and the exact
 * bandwidth are the base's own functions. The loop is the same, with the
 * gains computed each update from a covariance recursion on a model with a
 * noise figure `q` (the rate random-walks) and a measurement figure `r`
 * (docs/design/loop-filter.md Part III). Its own C type, its own functions: a
 * Kalman filter handed to the fixed-gain functions that write the state would
 * pair gains and covariance wrongly.
 */

#define STATE_FILTER_KALMAN_BNT_MIN 1e-6

#define STATE_FILTER_KALMAN_BNT_MAX 0.12

#define STATE_FILTER_KALMAN_START_MAX 5.0

typedef struct
{
  dp_state_filter_state_t base; 
  double                  p[6]; 
  double                  p0[6]; 
  double                  q;    
  double                  r;    
} dp_state_filter_kalman_state_t;

int dp_state_filter_kalman_steady (double q, double r, double p[6],
                                   double gains[3]);

double dp_state_filter_kalman_ratio_for_bandwidth (double bnt);

int dp_state_filter_kalman_params_ok_qr (double q, double r, double t,
                                      double start);

void dp_state_filter_kalman_init_qr (dp_state_filter_kalman_state_t *k,
                                     double q,
                                  double r, double t, double start);

dp_state_filter_kalman_state_t *dp_state_filter_kalman_create_qr (double q,
                                                                  double r,
                                                         double t,
                                                         double start);

void dp_state_filter_kalman_destroy (dp_state_filter_kalman_state_t *k);

void dp_state_filter_kalman_reset (dp_state_filter_kalman_state_t *k);

double dp_state_filter_kalman_update (dp_state_filter_kalman_state_t *k,
                                      double e);

JM_FORCEINLINE JM_HOT double
dp_state_filter_kalman_step (dp_state_filter_kalman_state_t *k, double e)
{
  return dp_state_filter_kalman_update (k, e);
}

void dp_state_filter_kalman_steps (dp_state_filter_kalman_state_t *k,
                                   const double *x, double *out, size_t n);

size_t
dp_state_filter_kalman_state_bytes (const dp_state_filter_kalman_state_t *k);

void dp_state_filter_kalman_get_state (const dp_state_filter_kalman_state_t *k,
                                       void *blob);

int dp_state_filter_kalman_set_state (dp_state_filter_kalman_state_t *k,
                                      const void *blob);

/* ── The Kalman flavor, as a caller states it ──────────────────────────
 * Two figures in the units of the loop's error input: @c sigma_e, the rms of
 * the detector error when the loop is locked (cycles, if the error is a phase
 * in cycles), and @c sigma_a, the rms change of the rate per update (those
 * units per update per update); the `_qr` functions above take their
 * squares. An optional frequency uncertainty @c sigma_f0 (0 = absent; those
 * units per update) must be at least the steady state's own frequency
 * deviation and at most STATE_FILTER_KALMAN_START_MAX times it, and no more
 * than the trajectory rule allows. A caller with a carrier-to-noise
 * density and a jerk makes the two figures from them; nothing here knows a
 * detector.
 */

double dp_state_filter_kalman_start_for (double sigma_e, double sigma_a,
                                         double sigma_f0);

int dp_state_filter_kalman_params_ok (double sigma_e, double sigma_a, double t,
                                      double sigma_f0);

void dp_state_filter_kalman_init (dp_state_filter_kalman_state_t *k,
                                  double sigma_e,
                                  double sigma_a, double t, double sigma_f0);

dp_state_filter_kalman_state_t *dp_state_filter_kalman_create (double sigma_e,
                                                         double sigma_a,
                                                         double t,
                                                         double sigma_f0);

int dp_state_filter_kalman_configure (dp_state_filter_kalman_state_t *k,
                                      double sigma_e, double sigma_a, double t,
                                      double sigma_f0, double bn);

int dp_state_filter_kalman_params_ok_bn (double bn, double t);

void dp_state_filter_kalman_init_bn (dp_state_filter_kalman_state_t *k,
                                     double bn,
                                     double t);

dp_state_filter_kalman_state_t *dp_state_filter_kalman_create_bn (double bn,
                                                            double t);

/* ── For the binding ─────────────────────────────────────────────────────
 * The Python class takes either the figures or `bn` alone; these dispatch to
 * the typed C functions above. A C caller calls the typed ones directly. */

#define STATE_FILTER_KALMAN_STATE_MAGIC DP_FOURCC ('S', 'F', 'K', 'A')
#define STATE_FILTER_KALMAN_STATE_VERSION 1u

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args (double sigma_e, double sigma_a, double t,
                                    double sigma_f0, double bn);

const char *dp_state_filter_kalman_args_why (double sigma_e, double sigma_a,
                                             double t, double sigma_f0,
                                             double bn);

dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args_why (double sigma_e, double sigma_a,
                                        double t, double sigma_f0, double bn,
                                        const char **why);

double
dp_state_filter_kalman_sigma_f (const dp_state_filter_kalman_state_t *k);

double
dp_state_filter_kalman_bandwidth (const dp_state_filter_kalman_state_t *k,
                                 double delay, double gain);

bool dp_state_filter_kalman_stable (const dp_state_filter_kalman_state_t *k,
                                    double delay, double gain);

#ifdef __cplusplus
}
#endif

#endif /* DP_STATE_FILTER_KALMAN_CORE_H */
```


