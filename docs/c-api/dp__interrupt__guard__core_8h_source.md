

# File dp\_interrupt\_guard\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_interrupt\_guard**](dir_069c076bf5cb5bab676dd02f8ef44735.md) **>** [**dp\_interrupt\_guard\_core.h**](dp__interrupt__guard__core_8h.md)

[Go to the documentation of this file](dp__interrupt__guard__core_8h.md)


```C++

#ifndef DP_INTERRUPT_GUARD_CORE_H
#define DP_INTERRUPT_GUARD_CORE_H

#include "doppler/dp_interrupt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_interrupt_guard dp_interrupt_guard_t;

  typedef dp_interrupt_guard_t dp_interrupt_guard_state_t;

  dp_interrupt_guard_t *dp_interrupt_guard_create (const int32_t *signals,
                                                   size_t     n_signals,
                                                   uint32_t   latency_ms);

  void dp_interrupt_guard_destroy (dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_interrupt (dp_interrupt_guard_t *guard);

  int dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_resume (dp_interrupt_guard_t *guard);

  uint32_t dp_interrupt_guard_latency_ms (const dp_interrupt_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_GUARD_CORE_H */
```


