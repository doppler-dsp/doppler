

# File wfm\_writer.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfm\_writer.h**](wfm__writer_8h.md)

[Go to the documentation of this file](wfm__writer_8h.md)


```C++

#ifndef WFM_WRITER_H
#define WFM_WRITER_H

#include <stdio.h>

#include "clib_common.h"
#include "wfmgen/wfm_compose.h" /* wfm_segment_t for SigMF annotations */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WFM_FT_RAW = 0,  
    WFM_FT_CSV = 1,  
    WFM_FT_BLUE = 2, 
    WFM_FT_SIGMF = 3 
} wfm_filetype_t;

typedef struct wfm_writer wfm_writer_t;

wfm_writer_t *wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type,
                             int endian, double fs, double fc,
                             size_t total_samples);

size_t wfm_writer_write(wfm_writer_t *w, const float _Complex *iq, size_t n);

int wfm_writer_close(wfm_writer_t *w);

int wfm_blue_write_hcb(FILE *fp, int sample_type, int endian, double fs,
                       double fc, double data_start, size_t total_samples,
                       int detached);

char *wfm_sigmf_meta_json(int sample_type, int endian, double fs, double fc,
                          const wfm_segment_t *segs, size_t n_segs);

#ifdef __cplusplus
}
#endif

#endif /* WFM_WRITER_H */
```


