

# File wfm\_reader\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm\_reader**](dir_01018a3d11538c9aca2db4daa45a442f.md) **>** [**wfm\_reader\_core.h**](wfm__reader__core_8h.md)

[Go to the documentation of this file](wfm__reader__core_8h.md)


```C++

#ifndef DP_WFM_READER_H
#define DP_WFM_READER_H

#include <complex.h>
#include <stddef.h>

#include "wfm/wfm_keywords.h" /* wfm_keyword_t */
#include "wfm_writer/wfm_writer_core.h"   /* wfm_filetype_t */
#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct wfm_reader_state wfm_reader_state_t;


  typedef enum
  {
    WFM_MODE_COMPLEX = 0, 
    WFM_MODE_SCALAR = 1   
  } wfm_mode_t;

  typedef enum
  {
    WFM_FC_NONE = 0,      
    WFM_FC_FREQ,          
    WFM_FC_RF_FREQ,       
    WFM_FC_CENTER_FREQ,   
    WFM_FC_F_C,           
    WFM_FC_SIGMF          
  } wfm_fc_source_t;

  typedef enum
  {
    WFM_FS_NONE = 0,    
    WFM_FS_BLUE_XDELTA, 
    WFM_FS_SIGMF        
  } wfm_fs_source_t;

  typedef enum
  {
    WFM_T0_NONE = 0,     
    WFM_T0_BLUE_TIMECODE 
    /* SigMF `core:datetime` is an ISO 8601 STRING and needs a parser this
       reader does not have yet; such a capture reports WFM_T0_NONE rather
       than a guess. */
  } wfm_t0_source_t;

  typedef enum
  {
    WFM_FOLLOW_NONE = 0,    
    WFM_FOLLOW_EOF,         
    WFM_FOLLOW_TIMEOUT,     
    WFM_FOLLOW_INTERRUPTED  
  } wfm_follow_end_t;

  typedef struct
  {
    int    file_type;   
    int    sample_type;
    int    mode;        
    int    endian;      
    double fs;          
    double fc;          
    size_t num_samples; 
    int    fc_source;   
    size_t trailing_bytes; 
    int    fs_source;   
    double t0_unix_sec; 
    int    t0_source;   
  } wfm_reader_info_t;

wfm_reader_state_t *wfm_reader_create(const char *path, int sample_type, int endian);

  void wfm_reader_info (const wfm_reader_state_t *r, wfm_reader_info_t *info);

size_t wfm_reader_read(wfm_reader_state_t *state, size_t n,
                       float _Complex *out, size_t max_out);

size_t wfm_reader_read_max_out(wfm_reader_state_t *state, size_t n);

size_t wfm_reader_num_keywords(const wfm_reader_state_t *state);

  const wfm_keyword_t *wfm_reader_keyword (const wfm_reader_state_t *r, size_t i);

const char *wfm_reader_keyword_tag(const wfm_reader_state_t *state, size_t i);

  size_t wfm_reader_num_header_fields(const wfm_reader_state_t *state);

  const wfm_keyword_t *wfm_reader_header_field(const wfm_reader_state_t *state,
                                               size_t i);

  const char *wfm_reader_header_tag(const wfm_reader_state_t *state, size_t i);

  const wfm_keyword_t *
  wfm_reader_find_header_field(const wfm_reader_state_t *state,
                               const char *name);

  const wfm_keyword_t *wfm_reader_find_keyword (const wfm_reader_state_t *r,
                                                const char        *tag);

void wfm_reader_reset(wfm_reader_state_t *state);

  void wfm_reader_set_stop_fn (wfm_reader_state_t *state, int (*fn) (void));


void wfm_reader_destroy(wfm_reader_state_t *state);

int wfm_reader_get_fc_source(const wfm_reader_state_t *state);

int wfm_reader_get_fs_source(const wfm_reader_state_t *state);

double wfm_reader_get_t0(const wfm_reader_state_t *state);

int wfm_reader_get_t0_source(const wfm_reader_state_t *state);

size_t wfm_reader_get_trailing_bytes(const wfm_reader_state_t *state);

int wfm_reader_get_file_type(const wfm_reader_state_t *state);
int wfm_reader_get_sample_type(const wfm_reader_state_t *state);
int wfm_reader_get_mode(const wfm_reader_state_t *state);
int wfm_reader_get_endian(const wfm_reader_state_t *state);
double wfm_reader_get_fs(const wfm_reader_state_t *state);
double wfm_reader_get_fc(const wfm_reader_state_t *state);
size_t wfm_reader_get_num_samples(const wfm_reader_state_t *state);
size_t wfm_reader_read_follow_max_out(wfm_reader_state_t *state, size_t n);
size_t wfm_reader_read_follow(wfm_reader_state_t *state, size_t n, float _Complex *out, size_t max_out);
uint32_t wfm_reader_get_follow_timeout_ms(const wfm_reader_state_t *state);
void wfm_reader_set_follow_timeout_ms(wfm_reader_state_t *state, uint32_t val);
uint32_t wfm_reader_get_follow_grace_ms(const wfm_reader_state_t *state);
void wfm_reader_set_follow_grace_ms(wfm_reader_state_t *state, uint32_t val);
int wfm_reader_get_ending(const wfm_reader_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_READER_H */
```


