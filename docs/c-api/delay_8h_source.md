

# File delay.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**delay.h**](delay_8h.md)

[Go to the documentation of this file](delay_8h.md)


```C++


#ifndef DP_DELAY_H
#define DP_DELAY_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_delay_cf64 dp_delay_cf64_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_delay_cf64_t *dp_delay_cf64_create (size_t num_taps);

  void dp_delay_cf64_destroy (dp_delay_cf64_t *dl);

  void dp_delay_cf64_reset (dp_delay_cf64_t *dl);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  size_t dp_delay_cf64_num_taps (const dp_delay_cf64_t *dl);

  size_t dp_delay_cf64_capacity (const dp_delay_cf64_t *dl);

  /* ------------------------------------------------------------------
   * Hot path
   * ------------------------------------------------------------------ */

  void dp_delay_cf64_push (dp_delay_cf64_t *dl, double _Complex x);

  const double _Complex *dp_delay_cf64_ptr (const dp_delay_cf64_t *dl);

  const double _Complex *dp_delay_cf64_push_ptr (dp_delay_cf64_t *dl, double _Complex x);

  void dp_delay_cf64_write (dp_delay_cf64_t *dl, const double _Complex *in,
                            size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_DELAY_H */
```
