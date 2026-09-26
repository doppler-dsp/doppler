

# File boxcar\_core.h

[**File List**](files.md) **>** [**boxcar**](dir_5b2ea30dc12e54f23750507f860119fd.md) **>** [**boxcar\_core.h**](boxcar__core_8h.md)

[Go to the documentation of this file](boxcar__core_8h.md)


```C++

#ifndef DP_BOXCAR_CORE_H
#define DP_BOXCAR_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /* Maximum window length. The delay ring is a fixed in-struct array so the
   * state stays pointer-free POD (embed-by-value + whole-struct
   * serialization); a longer window is rejected at create/init time. */
#define BOXCAR_MAX_LEN 64

  typedef struct
  {
    size_t len;        
    size_t pos;        
    double inv_len;    
    double gain;       
    float  scale;      
    float _Complex acc; 
    float _Complex ring[BOXCAR_MAX_LEN]; 
  } dp_boxcar_state_t;

  JM_FORCEINLINE JM_HOT float _Complex
  dp_boxcar_step (dp_boxcar_state_t *s, float _Complex x)
  {
    s->acc += x - s->ring[s->pos];
    s->ring[s->pos] = x;
    if (++s->pos >= s->len)
      s->pos = 0;
    return s->acc * s->scale;
  }

  JM_FORCEINLINE void
  dp_boxcar_set_gain (dp_boxcar_state_t *s, double gain)
  {
    s->gain  = gain;
    s->scale = (float)(gain * s->inv_len);
  }

  JM_FORCEINLINE double
  dp_boxcar_get_gain (const dp_boxcar_state_t *s)
  {
    return s->gain;
  }

  void boxcar_init (dp_boxcar_state_t *s, size_t len, double gain);

  dp_boxcar_state_t *dp_boxcar_create (size_t len, double gain);

  void dp_boxcar_destroy (dp_boxcar_state_t *s);

  void dp_boxcar_reset (dp_boxcar_state_t *s);

  void dp_boxcar_steps (dp_boxcar_state_t *s, const float _Complex *x,
                     float _Complex *out, size_t n);

  /* ── Serializable state (standard bytes interface; see dp_state.h)
   * ────────── Pointer-free POD struct, so a whole-struct snapshot resumes
   * exactly. */
#define BOXCAR_STATE_MAGIC DP_FOURCC ('B', 'O', 'X', 'C')
#define BOXCAR_STATE_VERSION 1u

  size_t dp_boxcar_state_bytes (const dp_boxcar_state_t *s);
  void dp_boxcar_get_state (const dp_boxcar_state_t *s, void *blob);
  int dp_boxcar_set_state (dp_boxcar_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BOXCAR_CORE_H */
```


