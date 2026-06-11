

# File wfm\_reader.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_reader.h**](wfm__reader_8h.md)

[Go to the documentation of this file](wfm__reader_8h.md)


```C++

#ifndef DP_WFM_READER_H
#define DP_WFM_READER_H

#include <complex.h>
#include <stddef.h>

#include "wfm/wfm_writer.h" /* wfm_filetype_t */

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct wfm_reader wfm_reader_t;

  typedef struct
  {
    int    file_type;   
    int    sample_type; 
    int    endian;      
    double fs;          
    double fc;          
    size_t num_samples; 
  } wfm_reader_info_t;

  wfm_reader_t *wfm_reader_open (const char *path, int hint_sample_type,
                                 int hint_endian);

  void wfm_reader_info (const wfm_reader_t *r, wfm_reader_info_t *info);

  size_t wfm_reader_read (wfm_reader_t *r, float _Complex *out, size_t max);

  void wfm_reader_close (wfm_reader_t *r);

#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_READER_H */
```


