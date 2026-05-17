

# File delay.h

[**File List**](files.md) **>** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md) **>** [**delay.h**](delay_8h.md)

[Go to the documentation of this file](delay_8h.md)


```C++


#ifndef NATIVE_DELAY_H
#define NATIVE_DELAY_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_delay_cf64 dp_delay_cf64_t;

  dp_delay_cf64_t *dp_delay_cf64_create (size_t num_taps);
  void dp_delay_cf64_destroy (dp_delay_cf64_t *dl);
  void dp_delay_cf64_reset (dp_delay_cf64_t *dl);

  size_t dp_delay_cf64_num_taps (const dp_delay_cf64_t *dl);
  size_t dp_delay_cf64_capacity (const dp_delay_cf64_t *dl);

  void dp_delay_cf64_push (dp_delay_cf64_t *dl, double _Complex x);
  const double _Complex *dp_delay_cf64_ptr (const dp_delay_cf64_t *dl);
  const double _Complex *dp_delay_cf64_push_ptr (dp_delay_cf64_t *dl,
                                                 double _Complex x);
  void dp_delay_cf64_write (dp_delay_cf64_t *dl, const double _Complex *in,
                            size_t n);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_DELAY_H */
```
