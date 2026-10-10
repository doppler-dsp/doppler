

# File spectrogram\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**spectrogram**](dir_0e14824f67e83f19566ffb7fc1ca7a06.md) **>** [**spectrogram\_core.h**](spectrogram__core_8h.md)

[Go to the documentation of this file](spectrogram__core_8h.md)


```C++

#ifndef DP_SPECTROGRAM_CORE_H
#define DP_SPECTROGRAM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/psd/psd_core.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SPECTROGRAM_STATE_MAGIC DP_FOURCC ('S', 'P', 'G', 'M')
#define SPECTROGRAM_STATE_VERSION 1u

#define DP_SPECTROGRAM_POWER 0
#define DP_SPECTROGRAM_DB 1

typedef struct
{
  dp_psd_state_t *psd; 
  dp_f32_t *ring;      
  dp_f32_framer_t fr;  
  float _Complex *last; 
  size_t nfft;          
  size_t hop;           
  int window;           
  float beta;           
  int mode;             
  size_t consumed;      
} dp_spectrogram_state_t;

dp_spectrogram_state_t *dp_spectrogram_create (size_t nfft, size_t hop,
                                               int window, float beta,
                                               int mode);

void dp_spectrogram_destroy (dp_spectrogram_state_t *s);

void dp_spectrogram_reset (dp_spectrogram_state_t *s);

size_t dp_spectrogram_push (dp_spectrogram_state_t *s,
                            const float _Complex *in, size_t n_in, float *out,
                            size_t max_out);

size_t dp_spectrogram_push_max_out (const dp_spectrogram_state_t *s,
                                    size_t n_in);

size_t dp_spectrogram_consumed (const dp_spectrogram_state_t *s);

size_t dp_spectrogram_rows_for (const dp_spectrogram_state_t *s,
                                size_t n_in);

size_t dp_spectrogram_flush (dp_spectrogram_state_t *s, float *row);

size_t dp_spectrogram_pending (const dp_spectrogram_state_t *s);

size_t dp_spectrogram_state_bytes (const dp_spectrogram_state_t *s);

void dp_spectrogram_get_state (const dp_spectrogram_state_t *s, void *blob);

int dp_spectrogram_set_state (dp_spectrogram_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DP_SPECTROGRAM_CORE_H */
```


