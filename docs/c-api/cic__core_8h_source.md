

# File cic\_core.h

[**File List**](files.md) **>** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the documentation of this file](cic__core_8h.md)


```C++

#ifndef CIC_CORE_H
#define CIC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t  integ_re[6];   /* N integrator accumulators, real path   */
    uint64_t  integ_im[6];   /* N integrator accumulators, imag path   */
    uint64_t *comb_re;       /* N×M comb delay line, real — heap       */
    uint64_t *comb_im;       /* N×M comb delay line, imag — heap       */
    uint32_t  comb_head[6];  /* circular write index per comb stage    */
    uint32_t  R;             /* decimation ratio                        */
    uint32_t  N;             /* number of integrator/comb stages (1–6) */
    uint32_t  M;             /* comb differential delay (1 or 2)       */
    uint32_t  phase;         /* input sample phase counter 0..R-1      */
    double    input_scale;   /* float → int64 scale (auto-computed)    */
    double    output_scale;  /* 1 / (input_scale × (R×M)^N)            */
} cic_state_t;

cic_state_t *cic_create(uint32_t R, uint32_t N, uint32_t M);

void cic_destroy(cic_state_t *state);

void cic_reset(cic_state_t *state);

size_t cic_decimate_max_out(cic_state_t *state);

size_t cic_decimate(cic_state_t *state, const float complex *in,
                    size_t n_in, float complex *out);

void cic_reconfigure(cic_state_t *state, uint32_t R, uint32_t N,
                     uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
```


