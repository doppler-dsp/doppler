

# File farrow\_core.h

[**File List**](files.md) **>** [**farrow**](dir_3474bb67440308cdab2155867b5160e7.md) **>** [**farrow\_core.h**](farrow__core_8h.md)

[Go to the documentation of this file](farrow__core_8h.md)


```C++

#ifndef FARROW_CORE_H
#define FARROW_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"
#include <complex.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    FARROW_LINEAR = 0,
    FARROW_PARABOLIC = 1,
    FARROW_CUBIC = 2,
};

/* All orders interpolate between tap d[1] and d[2]; d[3] is the newest sample,
 * so the interpolation point lags the newest input by FARROW_GROUP_DELAY. */
#define FARROW_GROUP_DELAY 2u

typedef struct {
    float complex d[4]; 
    int order;          
} farrow_state_t;

JM_FORCEINLINE void
farrow_init (farrow_state_t *s, int order)
{
    s->d[0] = s->d[1] = s->d[2] = s->d[3] = 0.0f;
    s->order = order;
}

JM_FORCEINLINE JM_HOT void
farrow_push (farrow_state_t *s, float complex x)
{
    s->d[0] = s->d[1];
    s->d[1] = s->d[2];
    s->d[2] = s->d[3];
    s->d[3] = x;
}

JM_FORCEINLINE JM_HOT float complex
farrow_eval (const farrow_state_t *s, float mu)
{
    float complex d0 = s->d[0], d1 = s->d[1], d2 = s->d[2], d3 = s->d[3];
    if (s->order == FARROW_LINEAR)
        return d1 + mu * (d2 - d1);
    if (s->order == FARROW_PARABOLIC)
        {
            /* symmetric piecewise-parabolic (Farrow, α = 0.5): linear plus a
             * symmetric μ(μ-1) parabolic correction → no delay bias. */
            float complex u  = d0 - d1 - d2 + d3;
            float complex c2 = 0.5f * u;
            float complex c1 = (d2 - d1) - c2;
            return (c2 * mu + c1) * mu + d1;
        }
    /* cubic */
    float complex c3 = (1.0f / 6.0f) * (-d0 + 3.0f * d1 - 3.0f * d2 + d3);
    float complex c2 = 0.5f * (d0 - 2.0f * d1 + d2);
    float complex c1
        = (1.0f / 6.0f) * (-2.0f * d0 - 3.0f * d1 + 6.0f * d2 - d3);
    return ((c3 * mu + c2) * mu + c1) * mu + d1;
}

farrow_state_t *farrow_create(int order);

void farrow_destroy(farrow_state_t *state);

void farrow_reset(farrow_state_t *state);

size_t farrow_delay_max_out(farrow_state_t *state);
size_t farrow_delay(farrow_state_t *state, const float complex *x, size_t x_len, double mu, float complex *out, size_t max_out);
size_t farrow_get_group_delay(const farrow_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the 4-tap delay line + order resume exactly into an
 * identically-built instance. */
#define FARROW_STATE_MAGIC DP_FOURCC ('F', 'R', 'R', 'W')
#define FARROW_STATE_VERSION 1u
size_t farrow_state_bytes (const farrow_state_t *state);
void   farrow_get_state (const farrow_state_t *state, void *blob);
int    farrow_set_state (farrow_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* FARROW_CORE_H */
```


