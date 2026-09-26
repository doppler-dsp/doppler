

# File wfm\_reader\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm\_reader**](dir_f352698a51aeb04a6ff33c180c5d8d41.md) **>** [**wfm\_reader\_core.h**](wfm__reader__core_8h.md)

[Go to the documentation of this file](wfm__reader__core_8h.md)


```C++

#ifndef DP_WFM_READER_H
#define DP_WFM_READER_H

#include "doppler/dp_complex.h"
#include <stddef.h>
#include <stdint.h>

#include "doppler/wfm/wfm_keywords.h" /* wfm_keyword_t */
#include "doppler/wfm_writer/wfm_writer_core.h"   /* wfm_filetype_t */
#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct wfm_reader_state dp_wfm_reader_state_t;


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
    WFM_T0_BLUE_TIMECODE, 
    WFM_T0_SIGMF          
    /* The SigMF spelling is an ISO 8601 string, parsed by
       dp_isotime_parse(). A stamp carrying no zone is REFUSED there rather
       than read as UTC, so such a capture still reports WFM_T0_NONE: being
       wrong by hours looks authoritative in a way that reporting nothing
       does not. */
  } wfm_t0_source_t;

  typedef enum
  {
    WFM_FOLLOW_NONE = 0,    
    WFM_FOLLOW_EOF,         
    WFM_FOLLOW_TIMEOUT,     
    WFM_FOLLOW_INTERRUPTED  
  } wfm_follow_end_t;

#define WFM_READER_STYPE_AUTO (-1)

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

dp_wfm_reader_state_t *dp_wfm_reader_create(const char *path, int sample_type, int endian);

  void wfm_reader_info (const dp_wfm_reader_state_t *r, wfm_reader_info_t *info);

size_t dp_wfm_reader_read(dp_wfm_reader_state_t *state, size_t n,
                       float _Complex *out, size_t max_out);

size_t dp_wfm_reader_read_max_out(dp_wfm_reader_state_t *state, size_t n);

size_t wfm_reader_num_keywords(const dp_wfm_reader_state_t *state);

  const wfm_keyword_t *wfm_reader_keyword (const dp_wfm_reader_state_t *r, size_t i);

const char *wfm_reader_keyword_tag(const dp_wfm_reader_state_t *state, size_t i);

  size_t wfm_reader_num_header_fields(const dp_wfm_reader_state_t *state);

  const wfm_keyword_t *wfm_reader_header_field(const dp_wfm_reader_state_t *state,
                                               size_t i);

  const char *wfm_reader_header_tag(const dp_wfm_reader_state_t *state, size_t i);

  const wfm_keyword_t *
  wfm_reader_find_header_field(const dp_wfm_reader_state_t *state,
                               const char *name);

  const wfm_keyword_t *wfm_reader_find_keyword (const dp_wfm_reader_state_t *r,
                                                const char        *tag);

void dp_wfm_reader_reset(dp_wfm_reader_state_t *state);

int dp_wfm_reader_seek(dp_wfm_reader_state_t *state, int64_t index);

int dp_wfm_reader_seek_time(dp_wfm_reader_state_t *state, double seconds);

  void wfm_reader_set_stop_fn (dp_wfm_reader_state_t *state, int (*fn) (void));


void dp_wfm_reader_destroy(dp_wfm_reader_state_t *state);

int dp_wfm_reader_get_fc_source(const dp_wfm_reader_state_t *state);

int dp_wfm_reader_get_fs_source(const dp_wfm_reader_state_t *state);

double dp_wfm_reader_get_t0(const dp_wfm_reader_state_t *state);

int dp_wfm_reader_get_t0_source(const dp_wfm_reader_state_t *state);

size_t dp_wfm_reader_get_trailing_bytes(const dp_wfm_reader_state_t *state);

size_t dp_wfm_reader_get_position(const dp_wfm_reader_state_t *state);

int dp_wfm_reader_get_file_type(const dp_wfm_reader_state_t *state);
int dp_wfm_reader_get_sample_type(const dp_wfm_reader_state_t *state);
int dp_wfm_reader_get_mode(const dp_wfm_reader_state_t *state);
int dp_wfm_reader_get_endian(const dp_wfm_reader_state_t *state);
double dp_wfm_reader_get_fs(const dp_wfm_reader_state_t *state);
double dp_wfm_reader_get_fc(const dp_wfm_reader_state_t *state);
size_t dp_wfm_reader_get_num_samples(const dp_wfm_reader_state_t *state);
size_t dp_wfm_reader_read_follow_max_out(dp_wfm_reader_state_t *state, size_t n);
size_t dp_wfm_reader_read_follow(dp_wfm_reader_state_t *state, size_t n, float _Complex *out, size_t max_out);
uint32_t dp_wfm_reader_get_follow_timeout_ms(const dp_wfm_reader_state_t *state);
void dp_wfm_reader_set_follow_timeout_ms(dp_wfm_reader_state_t *state, uint32_t val);
uint32_t dp_wfm_reader_get_follow_grace_ms(const dp_wfm_reader_state_t *state);
void dp_wfm_reader_set_follow_grace_ms(dp_wfm_reader_state_t *state, uint32_t val);
int dp_wfm_reader_get_ending(const dp_wfm_reader_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_READER_H */
```


