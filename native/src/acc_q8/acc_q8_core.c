#include "acc_q8/acc_q8_core.h"

acc_q8_state_t *
acc_q8_create(int32_t acc)
{
    acc_q8_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->acc = acc;
    return obj;
}

void
acc_q8_destroy(acc_q8_state_t *state)
{
    free(state);
}

void
acc_q8_reset(acc_q8_state_t *state)
{
    state->acc = 0;
}

void acc_q8_steps(
    acc_q8_state_t *state,
    const int8_t    *input,
    size_t               n)
{
    /* #pragma omp simd */
    for (size_t i = 0; i < n; i++)
        acc_q8_step(state, input[i]);
}

int32_t
acc_q8_get_acc(const acc_q8_state_t *state)
{
    return state->acc;
}

void
acc_q8_set_acc(acc_q8_state_t *state, int32_t val)
{
    state->acc = val;
}

int32_t
acc_q8_get(acc_q8_state_t *state)
{
    return state->acc;
}

int32_t
acc_q8_dump(acc_q8_state_t *state)
{
    int32_t v  = state->acc;
    state->acc = 0;
    return v;
}

void
acc_q8_madd(acc_q8_state_t *state,
            const int8_t *a, size_t a_len,
            const int8_t *b, size_t b_len)
{
    size_t n = a_len < b_len ? a_len : b_len;
    for (size_t i = 0; i < n; i++)
        state->acc += (int)a[i] * (int)b[i];
}
