

# File spectral\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md) **>** [**spectral\_core.h**](spectral__core_8h.md)

[Go to the documentation of this file](spectral__core_8h.md)


```C++

#ifndef SPECTRAL_CORE_H
#define SPECTRAL_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float freq_norm;    
    float amplitude_db; 
  } dp_peak_t;

float kaiser_enbw(const float *w, size_t w_len);

void kaiser_window(float *w, size_t w_len, float beta);

void hann_window(float *w, size_t w_len);

void magnitude_db_cf32(const float complex *x, size_t x_len, float *out, float lin_floor, float offset_db);

void magnitude_db_cf64(const double complex *x, size_t x_len, float *out, double lin_floor, float offset_db);

size_t find_peaks_f32(const float *db, size_t db_len, size_t n_peaks, float min_db, dp_peak_t *result);

double obw_from_power(const double *pwr, size_t pwr_len, double fs, double frac);
double noise_floor_db(const float *db, size_t db_len);
#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_CORE_H */
```


