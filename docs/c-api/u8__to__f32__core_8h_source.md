

# File u8\_to\_f32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**u8\_to\_f32**](dir_b468cb3e760d860bad02e34079834f5d.md) **>** [**u8\_to\_f32\_core.h**](u8__to__f32__core_8h.md)

[Go to the documentation of this file](u8__to__f32__core_8h.md)


```C++

#ifndef U8_TO_F32_CORE_H
#define U8_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    U8_TO_F32_SHIFT    = 0, 
    U8_TO_F32_MIDPOINT = 1  
} u8_to_f32_mode_t;

typedef struct {
    int   mode;   /* u8_to_f32_mode_t, validated at create */
    float iscale; /* 1/127.5, pre-computed for the midpoint multiply */
} u8_to_f32_state_t;

u8_to_f32_state_t *u8_to_f32_create(int mode);

void u8_to_f32_destroy(u8_to_f32_state_t *state);

void u8_to_f32_reset(u8_to_f32_state_t *state);

JM_FORCEINLINE float
u8_to_f32_shift(uint8_t x)
{
    return (float)((int32_t)x - 128) * 0x1p-7f;
}

JM_FORCEINLINE float
u8_to_f32_midpoint(const u8_to_f32_state_t *state, uint8_t x)
{
    return ((float)x - 127.5f) * state->iscale;
}

JM_FORCEINLINE JM_HOT float
u8_to_f32_step(const u8_to_f32_state_t *state, uint8_t x)
{
    return state->mode == U8_TO_F32_MIDPOINT ? u8_to_f32_midpoint(state, x)
                                             : u8_to_f32_shift(x);
}

void u8_to_f32_steps(
    u8_to_f32_state_t *state,
    const uint8_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* U8_TO_F32_CORE_H */
```


