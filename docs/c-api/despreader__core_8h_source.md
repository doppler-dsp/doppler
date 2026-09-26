

# File despreader\_core.h

[**File List**](files.md) **>** [**despreader**](dir_0568e7ebbbbb935946ff07943e2ec07c.md) **>** [**despreader\_core.h**](despreader__core_8h.md)

[Go to the documentation of this file](despreader__core_8h.md)


```C++

#ifndef DP_DESPREADER_CORE_H
#define DP_DESPREADER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/costas/costas_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/lo/lo_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/dp_complex.h"
#include "doppler/telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    dp_costas_state_t car;     
    dp_dll_state_t    code;    
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
  } dp_despreader_state_t;

  void despreader_init (dp_despreader_state_t *ch, const uint8_t *code,
                        size_t code_len, size_t sps, double init_norm_freq,
                        double init_chip, double bn_carrier, double bn_code,
                        double bn_fll, double zeta, double spacing,
                        size_t periods_per_bit);

  dp_despreader_state_t *dp_despreader_create (const uint8_t *code, size_t code_len,
                                         size_t sps, double init_norm_freq,
                                         double init_chip, double bn_carrier,
                                         double bn_code, double bn_fll,
                                         double zeta, double spacing,
                                         size_t periods_per_bit);

  void dp_despreader_destroy (dp_despreader_state_t *state);

  void dp_despreader_reset (dp_despreader_state_t *state);

  size_t dp_despreader_steps_max_out (dp_despreader_state_t *state);

  size_t dp_despreader_steps (dp_despreader_state_t *state, const float _Complex *x,
                           size_t x_len, float _Complex *out, size_t max_out);
  size_t dp_despreader_bits_max_out (dp_despreader_state_t *state);

  size_t dp_despreader_bits (dp_despreader_state_t *state, const float _Complex *x,
                          size_t x_len, uint8_t *out, size_t max_out);
  double dp_despreader_get_norm_freq (const dp_despreader_state_t *state);
  void   dp_despreader_set_norm_freq (dp_despreader_state_t *state, double val);
  double dp_despreader_get_code_phase (const dp_despreader_state_t *state);
  double dp_despreader_get_code_rate (const dp_despreader_state_t *state);
  double dp_despreader_get_lock_metric (const dp_despreader_state_t *state);

  int dp_despreader_get_carrier_locked (const dp_despreader_state_t *state);

  int dp_despreader_get_code_locked (const dp_despreader_state_t *state);

  void dp_despreader_configure_carrier_lock (dp_despreader_state_t *state,
                                          double up_thresh, double down_thresh,
                                          uint32_t n_up, uint32_t n_down);

  int dp_despreader_configure_code_lock (dp_despreader_state_t *state, double pfa,
                                      size_t n_looks, double ref_snr_db);

  size_t dp_despreader_get_bit_phase (const dp_despreader_state_t *state);
  double dp_despreader_get_bn_carrier (const dp_despreader_state_t *state);
  void   dp_despreader_set_bn_carrier (dp_despreader_state_t *state, double val);
  double dp_despreader_get_bn_code (const dp_despreader_state_t *state);
  void   dp_despreader_set_bn_code (dp_despreader_state_t *state, double val);

  int dp_despreader_set_telemetry (dp_despreader_state_t *state, dp_tlm_t *tlm,
                                const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: costas + dll children + running bit-sync histogram/state;
 * the owned code copy is restored by create. */
#define DESPREADER_STATE_MAGIC DP_FOURCC ('D', 'S', 'P', 'R')
#define DESPREADER_STATE_VERSION 4u /* v4: costas child grew (lockdet rule)   \
                                     */
  size_t dp_despreader_state_bytes (const dp_despreader_state_t *state);
  void   dp_despreader_get_state (const dp_despreader_state_t *state, void *blob);
  int    dp_despreader_set_state (dp_despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DESPREADER_CORE_H */
```


