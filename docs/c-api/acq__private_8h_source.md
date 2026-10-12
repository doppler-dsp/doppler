

# File acq\_private.h

[**File List**](files.md) **>** [**acq**](dir_34926d0c3adbcf3f65d9e19727d3354d.md) **>** [**acq\_private.h**](acq__private_8h.md)

[Go to the documentation of this file](acq__private_8h.md)


```C++

#ifndef ACQ_PRIVATE_H
#define ACQ_PRIVATE_H

#include <stddef.h>

static inline int
acq_rate_better (size_t h, float stat, float best)
{
  return h == 0 || stat > best;
}

#endif /* ACQ_PRIVATE_H */
```


