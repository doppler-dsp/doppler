

# File despreader\_core.h

[**File List**](files.md) **>** [**despreader**](dir_9949992fff5aebed427f83f9eaa478ca.md) **>** [**despreader\_core.h**](despreader__core_8h.md)

[Go to the documentation of this file](despreader__core_8h.md)


```C++

#ifndef DESPREADER_CORE_H
#define DESPREADER_CORE_H

#include "clib_common.h"
#include "costas/costas_core.h"
#include "detection/detection_core.h"
#include "dll/dll_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    costas_state_t car;     
    dll_state_t    code;    
    uint8_t *code_copy;     
    size_t periods_per_bit; 
    /* bit-sync (used only when periods_per_bit > 1) */
    size_t   *flip_hist;     
    size_t    epoch_count;   
    size_t    bit_phase;     
    size_t    epochs_in_bit; 
    double    bit_acc;       
    int       prev_sign;     
    int       have_prev;     
    dp_tlm_t *tlm_ctx;       
  } despreader_state_t;

  void despreader_init (despreader_state_t *ch, const uint8_t *code,
                        size_t code_len, size_t sps, double init_norm_freq,
                        double init_chip, double bn_carrier, double bn_code,
                        double bn_fll, double zeta, double spacing,
                        size_t periods_per_bit);

  despreader_state_t *despreader_create (const uint8_t *code, size_t code_len,
                                         size_t sps, double init_norm_freq,
                                         double init_chip, double bn_carrier,
                                         double bn_code, double bn_fll,
                                         double zeta, double spacing,
                                         size_t periods_per_bit);

  void despreader_destroy (despreader_state_t *state);

  void despreader_reset (despreader_state_t *state);

  size_t despreader_steps_max_out (despreader_state_t *state);
  size_t despreader_steps (despreader_state_t *state, const float complex *x,
                           size_t x_len, float complex *out, size_t max_out);
  size_t despreader_bits_max_out (despreader_state_t *state);
  size_t despreader_bits (despreader_state_t *state, const float complex *x,
                          size_t x_len, uint8_t *out, size_t max_out);
  double despreader_get_norm_freq (const despreader_state_t *state);
  void   despreader_set_norm_freq (despreader_state_t *state, double val);
  double despreader_get_code_phase (const despreader_state_t *state);
  double despreader_get_code_rate (const despreader_state_t *state);
  double despreader_get_lock_metric (const despreader_state_t *state);

  int despreader_get_carrier_locked (const despreader_state_t *state);

  int despreader_get_code_locked (const despreader_state_t *state);

  void despreader_configure_carrier_lock (despreader_state_t *state,
                                          double up_thresh, double down_thresh,
                                          uint32_t n_up, uint32_t n_down);

  int despreader_configure_code_lock (despreader_state_t *state, double pfa,
                                      size_t n_looks, double ref_snr_db);

  size_t despreader_get_bit_phase (const despreader_state_t *state);
  double despreader_get_bn_carrier (const despreader_state_t *state);
  void   despreader_set_bn_carrier (despreader_state_t *state, double val);
  double despreader_get_bn_code (const despreader_state_t *state);
  void   despreader_set_bn_code (despreader_state_t *state, double val);

  int despreader_set_telemetry (despreader_state_t *state, dp_tlm_t *tlm,
                                const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: costas + dll children + running bit-sync histogram/state;
 * the owned code copy is restored by create. */
#define DESPREADER_STATE_MAGIC DP_FOURCC ('D', 'S', 'P', 'R')
#define DESPREADER_STATE_VERSION 4u /* v4: costas child grew (lockdet rule)   \
                                     */
  size_t despreader_state_bytes (const despreader_state_t *state);
  void   despreader_get_state (const despreader_state_t *state, void *blob);
  int    despreader_set_state (despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DESPREADER_CORE_H */
```


