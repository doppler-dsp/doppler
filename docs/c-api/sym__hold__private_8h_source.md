

# File sym\_hold\_private.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dsss\_receiver**](dir_6f60e61f873068f58aa42ec05bfb1a20.md) **>** [**sym\_hold\_private.h**](sym__hold__private_8h.md)

[Go to the documentation of this file](sym__hold__private_8h.md)


```C++

#ifndef DP_SYM_HOLD_PRIVATE_H
#define DP_SYM_HOLD_PRIVATE_H

#include "doppler/clib_common.h"
#include <assert.h>
#include <string.h>

typedef size_t (*dp_sym_hold_call_fn) (void *ctx, const float _Complex *x,
                                       size_t x_len, float _Complex *out,
                                       size_t max_out);

static inline size_t
dp_sym_hold_steps (float _Complex *hold, size_t *hold_len, size_t hold_cap,
                   float _Complex **scratch, size_t *scratch_cap,
                   size_t *consumed, size_t bound, dp_sym_hold_call_fn call,
                   void *ctx, const float _Complex *x, size_t x_len,
                   float _Complex *out, size_t max_out)
{
  /* Held symbols first, in order. */
  const size_t n = *hold_len < max_out ? *hold_len : max_out;
  memcpy (out, hold, n * sizeof *out);
  *hold_len -= n;
  memmove (hold, hold + n, *hold_len * sizeof *hold);
  /* All or nothing: the input is consumed only when nothing stays held and
     whatever the call can produce fits in what is left of out plus the
     store. Otherwise it is not taken, and the caller calls again. */
  const size_t room = max_out - n;
  size_t       fits;
  if (*hold_len || !dp_size_add (room, hold_cap, &fits) || bound > fits)
    {
      *consumed = 0;
      return n;
    }
#ifndef NDEBUG
  size_t need;
  if (!dp_size_add (bound, 1, &need))
    need = SIZE_MAX; /* dp_xreallocn refuses it */
#else
  const size_t need = bound;
#endif
  if (*scratch_cap < need)
    {
      /* the cast is C++'s: the header is installed and parsed as C++ */
      *scratch = (float _Complex *)dp_xreallocn (*scratch, need,
                                                 sizeof **scratch);
      *scratch_cap = need;
    }
  const size_t made = call (ctx, x, x_len, *scratch, need);
  assert (made <= bound
          && "a receiver's call produced more than its steps_max_out() "
             "bounds it to");
  const size_t k = made < room ? made : room;
  memcpy (out + n, *scratch, k * sizeof *out);
  /* the rest fits the store: made <= bound <= room + hold_cap */
  memcpy (hold, *scratch + k, (made - k) * sizeof *hold);
  *hold_len = made - k;
  *consumed = x_len;
  return n + k;
}

#endif /* DP_SYM_HOLD_PRIVATE_H */
```


