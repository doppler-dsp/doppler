

# File dp\_interrupt\_guard\_core.h

[**File List**](files.md) **>** [**dp\_interrupt\_guard**](dir_001936014fd0d8bf32545bf8d71a57c6.md) **>** [**dp\_interrupt\_guard\_core.h**](dp__interrupt__guard__core_8h.md)

[Go to the documentation of this file](dp__interrupt__guard__core_8h.md)


```C++
/* dp_interrupt_guard_core.h — the object face of the interrupt facility.
 *
 * The flag itself lives in dp_interrupt.h, a root header three modules
 * include. This is the component that binds it: a scoped handle whose
 * create arms and whose destroy restores exactly what it armed.
 *
 * See docs/design/io-termination.md.
 */
#ifndef DP_INTERRUPT_GUARD_CORE_H
#define DP_INTERRUPT_GUARD_CORE_H

#include "dp_interrupt.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_interrupt_guard dp_interrupt_guard_t;

  /* jm derives a component's state type as <comp>_state_t, and doppler's
     public spelling is dp_interrupt_guard_t. One alias bridges them until
     just-makeit#797 lands `state_type`; dp_tlm_core.h carries the same
     line for the same reason. */
  typedef dp_interrupt_guard_t dp_interrupt_guard_state_t;

  dp_interrupt_guard_t *dp_interrupt_guard_create (const int32_t *signals,
                                                   size_t     n_signals,
                                                   uint32_t   latency_ms);

  void dp_interrupt_guard_destroy (dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_interrupt (dp_interrupt_guard_t *guard);

  int dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_resume (dp_interrupt_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_GUARD_CORE_H */
```


