

# File ofdm\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**ofdm**](dir_78c6add8c81d41bee73cbf3d222b7ad8.md) **>** [**ofdm\_core.h**](ofdm__core_8h.md)

[Go to the documentation of this file](ofdm__core_8h.md)


```C++

#ifndef DP_OFDM_CORE_H
#define DP_OFDM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/fft/fft_core.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

#define OFDM_STATE_MAGIC DP_FOURCC ('O', 'F', 'D', 'M')
#define OFDM_STATE_VERSION 1u

#define OFDM_DATA 1u
#define OFDM_NULL 0u
#define OFDM_ALL_PHASES 0xFFFFFFFFu
#define OFDM_CP_DEFAULT 0xFFFFFFFFu

#define OFDM_N_FFT_MAX 65536u
#define OFDM_N_PHASE_MAX 4096u
#define OFDM_COUNT_MAX 65536u

  typedef struct
  {
    uint32_t       phase; 
    int32_t        k;     
    float _Complex value; 
  } ofdm_fixed_t;

  typedef struct
  {
    uint32_t n_fft;         
    uint32_t useful;        
    uint32_t cyclic_prefix; 
    uint32_t window;        
    double   scale;         
    uint8_t *subcarrier_map;        
    float _Complex *subcarrier_gain; 
    uint32_t        n_phase;         
    ofdm_fixed_t   *fixed;           
    uint32_t        n_fixed;  
    uint32_t        fixed_cap;      
    float _Complex *polarity;       
    uint32_t        n_polarity; 
    uint32_t       *n_data;         
    float          *rise;           
  } ofdm_class_t;

  typedef struct
  {
    uint32_t class_index;     
    uint32_t count;           
    uint32_t cyclic_prefix;   
    uint32_t polarity_offset; 
    uint32_t first_phase;     
  } ofdm_run_t;

  typedef struct
  {
    ofdm_class_t *cls;       
    uint32_t      n_cls;     
    uint32_t      cap_cls;   
    ofdm_run_t   *runs;      
    uint32_t      n_runs;    
    uint32_t      cap_runs;  
    uint32_t      max_window; 
    int sealed;              
    /* derived at seal */
    dp_fft_state_t **plans;  
    uint32_t         n_plans; 
    uint32_t        *plan_of; 
    float _Complex  *grid;   
    float _Complex  *xform;  
    uint8_t         *taken;  
    /* running state */
    uint32_t       run_index;    
    uint32_t       sym_in_run;   
    uint32_t       tail_len;     
    float _Complex *tail;        
    uint32_t        tail_cap;    
  } dp_ofdm_state_t;

  dp_ofdm_state_t *dp_ofdm_create(void);

  void dp_ofdm_destroy(dp_ofdm_state_t *state);

  void dp_ofdm_reset(dp_ofdm_state_t *state);

  size_t dp_ofdm_state_bytes(const dp_ofdm_state_t *state);

  void dp_ofdm_get_state(const dp_ofdm_state_t *state, void *blob);

  int dp_ofdm_set_state(dp_ofdm_state_t *state, const void *blob);

  int dp_ofdm_add_class(dp_ofdm_state_t *state, uint32_t n_fft, const uint8_t *subcarrier_map, size_t subcarrier_map_len, uint32_t useful, uint32_t cyclic_prefix, uint32_t window, double scale, uint32_t n_phase);

  int dp_ofdm_add_subcarrier_gain(dp_ofdm_state_t *state, uint32_t class_index, const float _Complex *gain, size_t gain_len);

  int dp_ofdm_add_polarity(dp_ofdm_state_t *state, uint32_t class_index, const float _Complex *polarity, size_t polarity_len);

  int dp_ofdm_add_fixed(dp_ofdm_state_t *state, uint32_t class_index, uint32_t phase, int32_t k, double _Complex value);

  int dp_ofdm_add_run(dp_ofdm_state_t *state, uint32_t class_index, uint32_t count, uint32_t cyclic_prefix, uint32_t polarity_offset, uint32_t first_phase);

  int dp_ofdm_check_why(dp_ofdm_state_t *state, const char **why);

  int dp_ofdm_check(dp_ofdm_state_t *state);

  size_t dp_ofdm_points_remaining(dp_ofdm_state_t *state);

  size_t dp_ofdm_modulate_max_out(dp_ofdm_state_t *state, size_t n_in);

  int64_t dp_ofdm_modulate_why(dp_ofdm_state_t *state, const float _Complex *in, size_t n_in, float _Complex *out, size_t max_out, const char **why);

  size_t dp_ofdm_modulate(dp_ofdm_state_t *state, const float _Complex *in, size_t n_in, float _Complex *out, size_t max_out);

  size_t dp_ofdm_finish_max_out(dp_ofdm_state_t *state, size_t n);

  size_t dp_ofdm_finish(dp_ofdm_state_t *state, size_t n, float _Complex *out, size_t max_out);
#ifdef __cplusplus
}
#endif

#endif /* DP_OFDM_CORE_H */
```


