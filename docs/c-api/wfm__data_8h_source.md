

# File wfm\_data.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_data.h**](wfm__data_8h.md)

[Go to the documentation of this file](wfm__data_8h.md)


```C++

#ifndef WFM_DATA_H
#define WFM_DATA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    WFM_DATA_FRAME   = 0, 
    WFM_DATA_NOT_YET = 1, 
    WFM_DATA_END     = 2, 
    WFM_DATA_ERROR   = 3  
  } wfm_data_status_t;

  typedef struct wfm_data_src wfm_data_src_t;

  typedef struct
  {
    uint64_t frames;      
    uint64_t idle_frames; 
    uint64_t pad_bits;    
    uint64_t bits;        
    uint64_t total_bits;  
    uint64_t hash;        
    int      hashed;      
    int      stream;      
  } wfm_data_stats_t;

  wfm_data_src_t *dp_wfm_data_create (const char *data, const char *path,
                                      size_t len, const char *fill,
                                      const char **why);

  wfm_data_src_t *dp_wfm_data_create_fd (int fd, size_t len,
                                         const char *fill, const char **why);

  uint64_t dp_wfm_data_length_bits (const char *data, const char *path);

  void dp_wfm_data_destroy (wfm_data_src_t *s);

  wfm_data_status_t dp_wfm_data_next (wfm_data_src_t *s, size_t reps,
                                      uint8_t *out, size_t max_out,
                                      int timeout_ms);

  wfm_data_status_t dp_wfm_data_idle (wfm_data_src_t *s, size_t reps,
                                      uint8_t *out, size_t max_out);

  void dp_wfm_data_stats (const wfm_data_src_t *s, wfm_data_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DATA_H */
```


