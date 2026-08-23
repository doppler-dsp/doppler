

# File dp\_interrupt.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_interrupt.h**](dp__interrupt_8h.md)

[Go to the documentation of this file](dp__interrupt_8h.md)


```C++

#ifndef DP_INTERRUPT_H
#define DP_INTERRUPT_H

#include "clib_common.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_INTERRUPT_LATENCY_DEFAULT_MS 100u

  void dp_interrupt (void);

  void dp_resume (void);

  int dp_interrupted (void);

  void dp_set_interrupt_latency_ms (unsigned ms);

  unsigned dp_interrupt_latency_ms (void);

  int dp_interrupt_on_signal (int sig);

  int dp_restore_signal (int sig);

  typedef struct dp_interrupt_guard dp_interrupt_guard_t;

  dp_interrupt_guard_t *dp_interrupt_guard_create (const int *signals,
                                                   size_t     n_signals,
                                                   unsigned   latency_ms);

  void dp_interrupt_guard_destroy (dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_interrupt (dp_interrupt_guard_t *guard);

  int dp_interrupt_guard_interrupted (const dp_interrupt_guard_t *guard);

  void dp_interrupt_guard_resume (dp_interrupt_guard_t *guard);

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERRUPT_H */
```


