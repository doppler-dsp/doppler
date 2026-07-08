

# File hbdecim\_q15\_core.h

[**File List**](files.md) **>** [**hbdecim\_q15**](dir_93499f550a23db63d09661ee916a0767.md) **>** [**hbdecim\_q15\_core.h**](hbdecim__q15__core_8h.md)

[Go to the documentation of this file](hbdecim__q15__core_8h.md)


```C++

#ifndef HBDECIM_Q15_CORE_H
#define HBDECIM_Q15_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "dp_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t   num_taps;    /* FIR branch length supplied by caller          */
    size_t   K;           /* symmetric pair count = num_taps / 2           */
    size_t   K_pad;       /* K rounded up to multiple of 16 (SIMD align)   */
    size_t   centre;      /* = num_taps / 2 (index of delay-branch tap)    */
    int      fir_on_even; /* 1: FIR on even_dl; 0: FIR on odd_dl          */

    /* Q15 FIR branch coefficients, zero-padded to K_pad entries.
     * Scaled by 0.5 at create() to match the halfband polyphase identity.
     * Allocated with 32-byte alignment for AVX2 aligned loads.            */
    int16_t *coeffs;

    /* Dual-write delay rings: even- and odd-indexed input samples split
     * into separate I and Q arrays for contiguous SIMD access.            */
    int16_t *even_I;      /* [2 * cap]  */
    int16_t *even_Q;      /* [2 * cap]  */
    int16_t *odd_I;       /* [2 * cap]  */
    int16_t *odd_Q;       /* [2 * cap]  */
    size_t   cap;         /* next power-of-2 >= num_taps                   */
    size_t   mask;        /* = cap - 1                                     */
    size_t   even_head;
    size_t   odd_head;

    int      has_pending; /* 1 when a trailing even IQ pair is buffered    */
    int16_t  pending_I;
    int16_t  pending_Q;
} hbdecim_q15_state_t;

hbdecim_q15_state_t *hbdecim_q15_create(size_t num_taps, const float *h);

void hbdecim_q15_destroy(hbdecim_q15_state_t *r);

void hbdecim_q15_reset(hbdecim_q15_state_t *r);

size_t hbdecim_q15_execute(hbdecim_q15_state_t *r,
                           const int16_t *in, size_t n_in,
                           int16_t *out, size_t max_out);

size_t hbdecim_q15_execute_max_out(hbdecim_q15_state_t *r);

double hbdecim_q15_get_rate(const hbdecim_q15_state_t *r);

size_t hbdecim_q15_get_num_taps(const hbdecim_q15_state_t *r);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack four dual-write rings + heads + pending; coeffs restored by create. */
#define HBDECIM_Q15_STATE_MAGIC DP_FOURCC ('H','B','1','5')
#define HBDECIM_Q15_STATE_VERSION 1u
size_t hbdecim_q15_state_bytes (const hbdecim_q15_state_t *state);
void hbdecim_q15_get_state (const hbdecim_q15_state_t *state, void *blob);
int hbdecim_q15_set_state (hbdecim_q15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_Q15_CORE_H */
```


