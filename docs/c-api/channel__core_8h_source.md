

# File channel\_core.h

[**File List**](files.md) **>** [**channel**](dir_7cd82dec1dfa46f6b0156d9a972e4575.md) **>** [**channel\_core.h**](channel__core_8h.md)

[Go to the documentation of this file](channel__core_8h.md)


```C++

#ifndef CHANNEL_CORE_H
#define CHANNEL_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "jm_perf.h"
#include <complex.h>
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    costas_state_t car;   
    dll_state_t code;     
    uint8_t *code_copy;   
    size_t nav_period;    
    /* bit-sync (used only when nav_period > 1) */
    size_t *flip_hist;    
    size_t epoch_count;   
    size_t bit_phase;     
    size_t epochs_in_bit; 
    double bit_acc;       
    int prev_sign;        
    int have_prev;        
} channel_state_t;

void channel_init(channel_state_t *ch, const uint8_t *code, size_t code_len,
                  size_t sps, double init_norm_freq, double init_chip,
                  double bn_carrier, double bn_code, double bn_fll, double zeta,
                  double spacing, size_t nav_period);

channel_state_t *channel_create(const uint8_t *code, size_t code_len, size_t sps, double init_norm_freq, double init_chip, double bn_carrier, double bn_code, double bn_fll, double zeta, double spacing, size_t nav_period);

void channel_destroy(channel_state_t *state);

void channel_reset(channel_state_t *state);

size_t channel_steps_max_out(channel_state_t *state);
size_t channel_steps(channel_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
size_t channel_bits_max_out(channel_state_t *state);
size_t channel_bits(channel_state_t *state, const float complex *x, size_t x_len, uint8_t *out, size_t max_out);
double channel_get_norm_freq(const channel_state_t *state);
void channel_set_norm_freq(channel_state_t *state, double val);
double channel_get_code_phase(const channel_state_t *state);
double channel_get_code_rate(const channel_state_t *state);
double channel_get_lock_metric(const channel_state_t *state);
size_t channel_get_bit_phase(const channel_state_t *state);
double channel_get_bn_carrier(const channel_state_t *state);
void channel_set_bn_carrier(channel_state_t *state, double val);
double channel_get_bn_code(const channel_state_t *state);
void channel_set_bn_code(channel_state_t *state, double val);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: costas + dll children + running bit-sync histogram/state;
 * the owned code copy is restored by create. */
#define CHANNEL_STATE_MAGIC DP_FOURCC ('C','H','A','N')
#define CHANNEL_STATE_VERSION 1u
size_t channel_state_bytes (const channel_state_t *state);
void channel_get_state (const channel_state_t *state, void *blob);
int channel_set_state (channel_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_CORE_H */
```


