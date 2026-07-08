

# File boxcar\_core.h

[**File List**](files.md) **>** [**boxcar**](dir_4075e3d5389fc37fde93604059f4dd85.md) **>** [**boxcar\_core.h**](boxcar__core_8h.md)

[Go to the documentation of this file](boxcar__core_8h.md)


```C++

#ifndef BOXCAR_CORE_H
#define BOXCAR_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
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
    float complex acc; 
    float complex ring[BOXCAR_MAX_LEN]; 
  } boxcar_state_t;

  JM_FORCEINLINE JM_HOT float complex
  boxcar_step (boxcar_state_t *s, float complex x)
  {
    s->acc += x - s->ring[s->pos];
    s->ring[s->pos] = x;
    if (++s->pos >= s->len)
      s->pos = 0;
    return s->acc * s->scale;
  }

  JM_FORCEINLINE void
  boxcar_set_gain (boxcar_state_t *s, double gain)
  {
    s->gain  = gain;
    s->scale = (float)(gain * s->inv_len);
  }

  JM_FORCEINLINE double
  boxcar_get_gain (const boxcar_state_t *s)
  {
    return s->gain;
  }

  void boxcar_init (boxcar_state_t *s, size_t len, double gain);

  boxcar_state_t *boxcar_create (size_t len, double gain);

  void boxcar_destroy (boxcar_state_t *s);

  void boxcar_reset (boxcar_state_t *s);

  void boxcar_steps (boxcar_state_t *s, const float complex *input,
                     float complex *output, size_t n);

  /* ── Serializable state (standard bytes interface; see dp_state.h)
   * ────────── Pointer-free POD struct, so a whole-struct snapshot resumes
   * exactly. */
#define BOXCAR_STATE_MAGIC DP_FOURCC ('B', 'O', 'X', 'C')
#define BOXCAR_STATE_VERSION 1u

  size_t boxcar_state_bytes (const boxcar_state_t *s);
  void boxcar_get_state (const boxcar_state_t *s, void *blob);
  int boxcar_set_state (boxcar_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BOXCAR_CORE_H */
```


