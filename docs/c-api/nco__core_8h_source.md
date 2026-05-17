

# File nco\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the documentation of this file](nco__core_8h.md)


```C++

#ifndef NCO_CORE_H
#define NCO_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t phase;
    uint32_t phase_inc;
    float    norm_freq;
    uint32_t nmax;      /* wrap target; 0 means return raw uint32 */
} nco_state_t;

nco_state_t *nco_create(float norm_freq, uint32_t nmax);

void nco_destroy(nco_state_t *nco);

void nco_reset(nco_state_t *nco);

void nco_set_freq(nco_state_t *nco, float norm_freq);

float    nco_get_freq     (const nco_state_t *nco);
uint32_t nco_get_phase    (const nco_state_t *nco);
uint32_t nco_get_phase_inc(const nco_state_t *nco);

void nco_set_phase(nco_state_t *nco, uint32_t phase);

void nco_execute_u32(nco_state_t *nco, uint32_t *out, size_t n);

void nco_execute_u32_scaled(nco_state_t *nco, uint32_t *out, size_t n);

void nco_execute_u32_ovf(nco_state_t *nco,
                         uint32_t    *out,
                         uint8_t     *carry,
                         size_t       n);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
```
