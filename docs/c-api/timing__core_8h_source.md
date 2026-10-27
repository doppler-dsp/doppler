

# File timing\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**timing**](dir_f312c7d1315f2596c2b71ef05d5d7b2b.md) **>** [**timing\_core.h**](timing__core_8h.md)

[Go to the documentation of this file](timing__core_8h.md)


```C++

#ifndef DP_TIMING_CORE_H
#define DP_TIMING_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double   fs;            
    uint64_t epoch_mono_ns; 
    uint64_t epoch_real_ns; 
    uint64_t n;             
    uint64_t underruns;     
    uint64_t max_late_ns;   
    int      resync;        
    int      has_anchor; 
  } dp_sample_clock_t;

  uint64_t dp_mono_ns (void);

  uint64_t dp_real_ns (void);

  void dp_sample_clock_init (dp_sample_clock_t *c, double fs, int resync);

  double dp_sample_clock_pace (dp_sample_clock_t *c, size_t count);

  void dp_sample_clock_set_rate (dp_sample_clock_t *c, double fs);

  uint64_t dp_sample_clock_stamp (const dp_sample_clock_t *c);

  uint64_t dp_sample_clock_stamp_at (const dp_sample_clock_t *c, uint64_t n);

  int dp_sample_clock_track (dp_sample_clock_t *c,
                             uint64_t           observed_timestamp_ns,
                             uint64_t n_at_observation, uint64_t tolerance_ns);

  void dp_sample_clock_reset (dp_sample_clock_t *c);

  void dp_sample_clock_resync (dp_sample_clock_t *c);

  void dp_sample_clock_stats (const dp_sample_clock_t *c,
                              dp_sample_clock_t       *out);

  dp_sample_clock_t *dp_sample_clock_create (double fs, int resync);

  void dp_sample_clock_destroy (dp_sample_clock_t *c);

#ifdef __cplusplus
}
#endif

#endif /* DP_TIMING_CORE_H */
```


