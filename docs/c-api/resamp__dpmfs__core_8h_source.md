

# File resamp\_dpmfs\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp\_dpmfs**](dir_8ad30719b9735422ba744b401f0aead4.md) **>** [**resamp\_dpmfs\_core.h**](resamp__dpmfs__core_8h.md)

[Go to the documentation of this file](resamp__dpmfs__core_8h.md)


```C++

#ifndef RESAMP_DPMFS_CORE_H
#define RESAMP_DPMFS_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RESAMP_DPMFS_DEFAULT_BLOCK 4096u

typedef struct {
    double          rate;
    size_t          M;        /* polynomial order (1..3)           */
    size_t          N;        /* taps per phase                    */
    int             upsample; /* 1 = interpolation, 0 = decimation */

    /* c[j][(M+1)*N]: row-major coefficient bank [m*N+k] */
    float          *c[2];

    /* NCO */
    uint32_t        phase;
    uint32_t        phase_inc;

    /* Interpolator: dual-buffer power-of-2 delay line */
    float _Complex *delay_buf;
    size_t          delay_cap;
    size_t          delay_mask;
    size_t          delay_head;

    /* Decimator (transposed form): iad[N] + tfd[N-1].
     * iad is rounded up to a multiple of 8 for AVX-512 safety. */
    float _Complex *iad;
    float _Complex *tfd;

    size_t          buf_cap; /* output buffer capacity in samples */
} resamp_dpmfs_state_t;

resamp_dpmfs_state_t *resamp_dpmfs_create(
    size_t poly_order, size_t n_taps,
    const float *c0, const float *c1,
    double rate);

void resamp_dpmfs_destroy(resamp_dpmfs_state_t *state);

void resamp_dpmfs_reset(resamp_dpmfs_state_t *state);

size_t resamp_dpmfs_execute_max_out(resamp_dpmfs_state_t *state);

size_t resamp_dpmfs_execute(resamp_dpmfs_state_t *state,
                            const float complex *in, size_t n_in,
                            float complex *out);

double resamp_dpmfs_get_rate(const resamp_dpmfs_state_t *state);

size_t resamp_dpmfs_get_num_taps(const resamp_dpmfs_state_t *state);

size_t resamp_dpmfs_get_poly_order(const resamp_dpmfs_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_DPMFS_CORE_H */
```
