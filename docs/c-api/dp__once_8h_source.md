

# File dp\_once.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_once.h**](dp__once_8h.md)

[Go to the documentation of this file](dp__once_8h.md)


```C++

#ifndef DP_ONCE_H
#define DP_ONCE_H

#include <stdatomic.h>

typedef struct
{
  atomic_int state; 
} dp_once_t;

#define DP_ONCE_INIT { 0 }

static inline void
dp_once (dp_once_t *once, void (*fn) (void))
{
  if (atomic_load_explicit (&once->state, memory_order_acquire) == 2)
    return;
  int idle = 0;
  if (atomic_compare_exchange_strong_explicit (&once->state, &idle, 1,
                                               memory_order_acq_rel,
                                               memory_order_acquire))
    {
      fn ();
      atomic_store_explicit (&once->state, 2, memory_order_release);
      return;
    }
  /* Another thread is building: wait for its release store. */
  while (atomic_load_explicit (&once->state, memory_order_acquire) != 2)
    ;
}

#endif /* DP_ONCE_H */
```


