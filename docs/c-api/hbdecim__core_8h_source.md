

# File hbdecim\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**hbdecim**](dir_29792a392a590bbead7fbef545faea73.md) **>** [**hbdecim\_core.h**](hbdecim__core_8h.md)

[Go to the documentation of this file](hbdecim__core_8h.md)


```C++

#ifndef HBDECIM_CORE_H
#define HBDECIM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t num_taps; /* FIR branch length                         */
    size_t centre;   /* = num_taps / 2                            */
    int fir_on_even; /* 1 if N even, 0 if N odd                   */
    float *h;        /* FIR coeffs × 0.5 (polyphase normalisation) */

    /* Even delay line: stores x[2m], x[2m-2], ...
     * Dual-write ring: 2 × even_cap elements.                       */
    float _Complex *even_buf;
    size_t even_cap;
    size_t even_mask;
    size_t even_head;

    /* Odd delay line: stores x[2m+1], x[2m-1], ...
     * Shares even_cap and even_mask with even_buf.                  */
    float _Complex *odd_buf;
    size_t odd_head;

    int has_pending;        /* 1 when a trailing even sample is held */
    float _Complex pending; /* the held even sample                  */
  } hbdecim_state_t;

  hbdecim_state_t *hbdecim_create (size_t num_taps, const float *h);

  void hbdecim_destroy (hbdecim_state_t *r);

  void hbdecim_reset (hbdecim_state_t *r);

  /* Serializable state (reusable elastic-resume convention): the even/odd
   * dual-write delay rings, their heads, and the pending even sample.  Coeffs
   * and sizes are config (rebuilt from num_taps on the resumed instance). */

  /* Standard bytes interface; see dp_state.h. */
#define HBDECIM_STATE_MAGIC DP_FOURCC ('H', 'B', 'D', 'C')
#define HBDECIM_STATE_VERSION 1u

  size_t hbdecim_state_bytes (const hbdecim_state_t *r);
  void hbdecim_get_state (const hbdecim_state_t *r, void *blob);
  int hbdecim_set_state (hbdecim_state_t *r, const void *blob);

  size_t hbdecim_execute (hbdecim_state_t *r, const float _Complex *in,
                          size_t num_in, float _Complex *out, size_t max_out);

  double hbdecim_get_rate (const hbdecim_state_t *r);

  size_t hbdecim_get_num_taps (const hbdecim_state_t *r);

  double hbdecim_dc_gain (const hbdecim_state_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_CORE_H */
```


